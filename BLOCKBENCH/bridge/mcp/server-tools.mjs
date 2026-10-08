import { McpServer } from '@modelcontextprotocol/sdk/server/mcp.js';
import { mkdir, readFile, readdir, stat, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { randomUUID } from 'node:crypto';
import { tools } from '../shared/tools.mjs';

function isInside(root, target) {
  const rel = path.relative(root, target);
  return rel === '' || (!rel.startsWith('..') && !path.isAbsolute(rel));
}

function resolveAssetPath(config, relativePath) {
  const full = path.resolve(config.projectRoot, ...relativePath.split('/'));
  if (
    !isInside(config.projectRoot, full) ||
    !config.assetRoots.some((root) => isInside(root, full))
  )
    throw new Error('Path is outside the configured asset roots.');
  return full;
}

async function listModels(config, args) {
  const base = resolveAssetPath(config, args.directory);
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
  const full = resolveAssetPath(config, relativePath);
  const info = await stat(full);
  if (!info.isFile()) throw new Error('Project path is not a file.');
  if (info.size > 6 * 1024 * 1024)
    throw new Error('Editable project is larger than the 6 MiB bridge limit.');
  return { full, content: await readFile(full, 'utf8') };
}

function expectedExtension(format) {
  return `.${format}`;
}

async function writeResult(config, args, result, kind) {
  let filename;
  if (args.path) {
    filename = resolveAssetPath(config, args.path);
    const expected = expectedExtension(
      kind === 'save' ? 'bbmodel' : args.format,
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

export function createMcp(call, config, oauth = false) {
  const server = new McpServer(
    { name: 'shardbound-blockbench', version: '0.4.0' },
    {
      instructions:
        "Control the user's open Blockbench Generic Model. Start with status and read_project; use current project_id and expected_revision for edits. Cubes support per-face UVs/textures. Use list_model_files/open_project_file for editable .bbmodel sources and save_project/export_project for repository-safe writes. Select components before MTools. Inspect mtools_list and capture_view to verify results. Operations are serialized. A timeout may have changed the model: inspect before retrying. Do not assume unlisted MTools actions are supported.",
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
          if (name === 'list_model_files') {
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
