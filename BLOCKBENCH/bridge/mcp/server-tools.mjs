import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { mkdir, readFile, readdir, stat, writeFile, realpath, open } from 'node:fs/promises';
import path from 'node:path';
import { randomUUID } from 'node:crypto';
import { tools } from '../shared/tools.mjs';

function isInside(root, target) {
  const rel = path.relative(root, target);
  return rel === '' || (!rel.startsWith('..') && !path.isAbsolute(rel));
}

async function resolveAssetPath(config, relativePath) {
  const full = path.resolve(config.projectRoot, ...relativePath.split('/'));
  if (
    !isInside(config.projectRoot, full) ||
    !config.assetRoots.some((root) => isInside(root, full))
  )
    throw new Error('Path is outside the configured asset roots.');
  // Also verify existing ancestors: a junction/symlink under assets must not escape.
  const realProject = await realpath(config.projectRoot);
  async function resolveExisting(target) { let ancestor = target;
    while (true) { try { return path.resolve(await realpath(ancestor), path.relative(ancestor, target)); }
      catch (error) { if (error.code !== 'ENOENT') throw error; const parent = path.dirname(ancestor); if (parent === ancestor) throw error; ancestor = parent; } }
  }
  const actual = await resolveExisting(full), actualRoots = await Promise.all(config.assetRoots.map(resolveExisting));
  if (!isInside(realProject, actual) || !actualRoots.some(root => isInside(realProject, root) && isInside(root, actual))) throw new Error('Asset symlink/junction escapes configured asset roots.');
  return full;
}

async function listModels(config, args) {
  const base = await resolveAssetPath(config, args.directory);
  const extensions = new Set(
    args.extensions.map((ext) => `.${ext.toLowerCase()}`),
  );
  const results = [];
  async function walk(dir, depth) {
    if (results.length >= 500) return;
    let entries;
    try {
      entries = await readdir(dir, { withFileTypes: true });
    } catch (error) {
      if (error.code === 'ENOENT') return;
      throw error;
    }
    for (const entry of entries) {
      if (results.length >= 500) break;
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        if (args.recursive && depth < 16) await walk(full, depth + 1);
      } else if (
        entry.isFile() &&
        extensions.has(path.extname(entry.name).toLowerCase())
      ) {
        await resolveAssetPath(config, path.relative(config.projectRoot, full).split(path.sep).join('/'));
        const info = await stat(full);
        results.push({
          path: path
            .relative(config.projectRoot, full)
            .split(path.sep)
            .join('/'),
          bytes: info.size,
          modified: info.mtime.toISOString(),
        });
      }
    }
  }
  await walk(base, 0);
  results.sort((a, b) => a.path.localeCompare(b.path));
  return {
    files: results,
    count: results.length,
    truncated: results.length >= 500,
  };
}

async function readEditableProject(config, relativePath) {
  const full = await resolveAssetPath(config, relativePath);
  const info = await stat(full);
  if (!info.isFile()) throw new Error('Project path is not a file.');
  if (info.size > 32 * 1024 * 1024)
    throw new Error('Editable project is larger than the 32 MiB bridge limit.');
  return { full, content: await readFile(full, 'utf8') };
}

function expectedExtension(format) {
  return `.${format}`;
}

async function writeResult(config, args, result, kind) {
  let filename;
  if (args.path) {
    filename = await resolveAssetPath(config, args.path);
    const expected = expectedExtension(
      kind === 'save' ? result.format : args.format,
    );
    if (path.extname(filename).toLowerCase() !== expected)
      throw new Error(`Destination must end in ${expected}.`);
  } else {
    await mkdir(config.outputs, { recursive: true });
    filename = path.join(
      config.outputs,
      `${args.filename}-${Date.now()}-${randomUUID().slice(0, 8)}.${args.format}`,
    );
  }
  await mkdir(path.dirname(filename), { recursive: true });
  const options = args.overwrite ? {} : { flag: 'wx' };
  if (result.binary) {
    if (!result.content_base64)
      throw new Error('Blockbench returned binary export without content.');
    await writeFile(
      filename,
      Buffer.from(result.content_base64, 'base64'),
      options,
    );
    delete result.content_base64;
  } else {
    if (typeof result.content !== 'string')
      throw new Error('Blockbench returned invalid export content.');
    await writeFile(filename, result.content, { encoding: 'utf8', ...options });
    delete result.content;
  }
  result.path = filename;
  result.project_path = isInside(config.projectRoot, filename)
    ? path.relative(config.projectRoot, filename).split(path.sep).join('/')
    : null;
  result.note = result.project_path
    ? 'Saved inside the game repository.'
    : 'Saved on the computer running Blockbench. This is not a remote ChatGPT download URL.';
  return result;
}

async function readResource(config, relative, extensions, maxBytes) {
  if (!relative || !extensions.includes(path.extname(relative).toLowerCase())) throw new Error(`Use a real asset file: ${extensions.join(', ')}.`);
  const full = await resolveAssetPath(config, relative), info = await stat(full);
  if (!info.isFile() || info.size > maxBytes) throw new Error('Resource is not a file or exceeds the import size limit.');
  return readFile(full);
}

async function exportStudio(call, config, args) {
  const parent = args.directory ? await resolveAssetPath(config, args.directory) : config.outputs;
  // Reserve a fresh, exclusive directory before compiling. No overwrite option.
  await mkdir(parent, { recursive: true });
  const destination = path.join(parent, `studio-${Date.now()}-${randomUUID().slice(0, 8)}`);
  await mkdir(destination);
  let result;
  try {
    result = await call('studio_export', args);
    if (!result.session || !Array.isArray(result.files) || !result.files.length || result.files.length > 10000) throw new Error('Invalid Studio package transfer manifest.');
    let total = 0; const names = new Set();
    for (const file of result.files) {
      if (typeof file.name !== 'string' || !file.name || file.name.includes('\\') || file.name.includes(':') || file.name.split('/').some(p => !p || p === '.' || p === '..') || !Number.isInteger(file.bytes) || file.bytes < 0 || names.has(file.name.toLowerCase())) throw new Error('Invalid/duplicate package file path or size.');
      total += file.bytes; names.add(file.name.toLowerCase());
    }
    if (total > 256 * 1024 * 1024) throw new Error('Studio package exceeds the 256 MiB MCP limit.');
    for (const file of result.files) {
      const target = path.resolve(destination, ...file.name.split('/')); if (!isInside(destination, target)) throw new Error('Invalid package path.');
      await mkdir(path.dirname(target), { recursive: true });
      const stream = await open(target, 'wx');
      try { for (let offset = 0; offset < file.bytes;) {
        const chunk = await call('__studio_export_chunk', { session: result.session, file: file.name, offset });
        if (chunk.total !== file.bytes || typeof chunk.data !== 'string' || !/^[A-Za-z0-9+/=]+$/.test(chunk.data)) throw new Error('Invalid Studio package chunk.');
        const bytes = Buffer.from(chunk.data, 'base64'); if (!bytes.length || bytes.length > 1024 * 1024 || offset + bytes.length > file.bytes) throw new Error('Invalid Studio chunk length.');
        let written = 0; while (written < bytes.length) { const part = await stream.write(bytes, written, bytes.length - written); if (!part.bytesWritten) throw new Error('Package write made no progress.'); written += part.bytesWritten; }
        offset += bytes.length;
      } } finally { await stream.close(); }
    }
    return { project_id: result.project_id, revision: result.revision, directory: destination, files: result.files, warnings: result.warnings, bytes: total,
      note: 'Complete Studio package saved on the Blockbench computer. Runtime integration remains phase B; this path is not a ChatGPT download URL.' };
  } catch (error) { throw new Error(`${error.message} Partial output (if any): ${destination}`); }
  finally { if (result?.session) await call('__studio_export_release', { session: result.session }).catch(() => {}); }
}

export function createMcp(call, config, oauth = false) {
  const server = new McpServer(
    { name: 'shardbound-blockbench', version: '0.5.0' },
    {
      instructions:
        "Control Generic Model and ShardBound Studio in Blockbench 5.2.2. Start status, read_project, capture_view; show the actual capture before editing. For Studio call studio_read, use native model roots and reference-player folders, and use studio_* tools for clips, numeric group keyframes, FX/audio and export. Always use current project_id and expected_revision for edits; re-read after every mutation or timeout. Studio GLBs require studio_export (full package, excludes reference geometry). Use assets/ for editable .bbmodel/.sbstudio and sound/skin imports. MTools requires mtools_list and exact mesh/component selection; only automated actions are supported. Never execute arbitrary scripts or modify gameplay runtime.",
    },
  );
  for (const [name, def] of Object.entries(tools))
    server.registerTool(
      name,
      {
        title: name.replaceAll('_', ' '),
        description: def.description,
        inputSchema: def.schema,
        annotations: {
          readOnlyHint: !!def.read,
          destructiveHint: def.destructive ?? !def.read,
          idempotentHint: !!def.read,
          openWorldHint: false,
        },
        _meta: {
          securitySchemes: oauth
            ? [{ type: 'oauth2', scopes: ['blockbench'] }]
            : [{ type: 'noauth' }],
        },
      },
      async (rawArgs) => {
        try {
          const args = def.schema.parse(rawArgs);
          let result;
          if (name === 'studio_export') {
            result = await exportStudio(call, config, args);
          } else if (name === 'studio_sound' && args.action === 'import') {
            const bytes = await readResource(config, args.path, ['.wav', '.ogg', '.mp3'], 24 * 1024 * 1024);
            const mime = { '.wav': 'wav', '.ogg': 'ogg', '.mp3': 'mpeg' }[path.extname(args.path).toLowerCase()];
            result = await call('__studio_import_sound', { ...args, data: `data:audio/${mime};base64,${bytes.toString('base64')}`, filename: path.basename(args.path) });
          } else if (name === 'studio_skin' && args.action === 'import' && args.path) {
            if (args.texture) throw new Error('Provide path or texture, not both.');
            const bytes = await readResource(config, args.path, ['.png'], 4 * 1024 * 1024);
            result = await call('__studio_import_skin', { ...args, data: `data:image/png;base64,${bytes.toString('base64')}` });
          } else if (name === 'list_model_files') {
            result = await listModels(config, args);
          } else if (name === 'open_project_file') {
            const file = await readEditableProject(config, args.path);
            result = await call('__open_project_data', {
              path: file.full,
              content: file.content,
            });
            result.path = args.path;
          } else if (name === 'save_project') {
            result = await call(name, args);
            result = await writeResult(config, args, result, 'save');
          } else if (name === 'export_project') {
            result = await call(name, args);
            result = await writeResult(config, args, result, 'export');
          } else {
            result = await call(name, args);
          }
          if (result.image) {
            const match = /^data:image\/png;base64,(.+)$/.exec(result.image);
            if (!match)
              throw new Error('Blockbench returned an invalid image.');
            return {
              content: [
                { type: 'image', mimeType: 'image/png', data: match[1] },
                {
                  type: 'text',
                  text: JSON.stringify({
                    project_id: result.project_id,
                    revision: result.revision,
                  }),
                },
              ],
            };
          }
          return { content: [{ type: 'text', text: JSON.stringify(result) }] };
        } catch (error) {
          return {
            isError: true,
            content: [{ type: 'text', text: error.message || String(error) }],
          };
        }
      },
    );
  return server;
}
