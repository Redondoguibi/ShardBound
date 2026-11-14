import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { copyFile, mkdir, readFile, rename } from 'node:fs/promises';
import path from 'node:path';

const [server, operation, planPath] = process.argv.slice(2);
if (!server || !['clips', 'export', 'tool', 'sync'].includes(operation)) throw new Error('Invalid editor bridge operation');
const child = spawn(process.execPath, [server], { stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true });
const reader = createInterface({ input: child.stdout });
const pending = new Map();
let nextId = 0;
let bridgeError = '';
child.stderr.on('data', (data) => { if (bridgeError.length < 3000) bridgeError += data; });
function fail(error) { for (const callback of pending.values()) callback.reject(error); pending.clear(); }
child.on('error', fail);
child.on('exit', () => fail(new Error(bridgeError || 'Blockbench bridge disconnected')));
reader.on('line', (line) => {
  let response;
  try { response = JSON.parse(line); } catch { return; }
  const callback = pending.get(response.id);
  if (!callback) return;
  pending.delete(response.id);
  if (response.error) callback.reject(new Error(response.error.message));
  else callback.resolve(response.result);
});
function send(method, params) {
  const id = ++nextId;
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    child.stdin.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
  });
}
async function tool(name, args = {}) {
  const result = await send('tools/call', { name, arguments: args });
  const text = result.content?.find((item) => item.type === 'text')?.text ?? '';
  if (result.isError) throw new Error(text);
  return result.structuredContent ?? JSON.parse(text);
}
const timeout = setTimeout(() => child.kill(), 195_000);
try {
  await send('initialize', { protocolVersion: '2024-11-05', capabilities: {}, clientInfo: { name: 'shardbound-editor', version: '1.0.0' } });
  child.stdin.write(JSON.stringify({ jsonrpc: '2.0', method: 'notifications/initialized' }) + '\n');
  const current = await tool('status');
  if (!current.project_id) throw new Error('Open the intended Studio project in Blockbench');
  const guarded = async (name, args) => {
    const now = await tool('status');
    if (now.project_id !== current.project_id) throw new Error('The active Blockbench project changed');
    return tool(name, { ...args, project_id: now.project_id, expected_revision: now.revision });
  };
  if (operation === 'export') {
    const result = await guarded('studio_export', {});
    process.stdout.write(JSON.stringify({ manifest: path.join(result.directory, 'studio.manifest.json') }));
  } else if (operation === 'sync') {
    const plan = JSON.parse(await readFile(planPath, 'utf8'));
    const studio = await tool('studio_read');
    if (studio.state.id !== plan.project) throw new Error('The active project does not match the selected Studio package');
    const result = await guarded('studio_export', {});
    const old = JSON.parse(await readFile(plan.manifest, 'utf8'));
    const fresh = JSON.parse(await readFile(path.join(result.directory, 'studio.manifest.json'), 'utf8'));
    if (old.project !== fresh.project || JSON.stringify(old.models.map(({ id, type }) => [id, type]).sort()) !==
        JSON.stringify(fresh.models.map(({ id, type }) => [id, type]).sort())) throw new Error('The model IDs or types in the export changed');
    const events = JSON.parse(await readFile(path.join(result.directory, fresh.events), 'utf8'));
    if (fresh.models.every(({ type }) => type === 'armor') && events.clips.length) throw new Error('Armor packages must have no animation clips');
    const target = path.dirname(plan.manifest);
    if (path.basename(target) !== 'studio' || path.basename(plan.manifest) !== 'studio.manifest.json')
      throw new Error('Sync requires a Studio package created by the editor');
    const stage = path.join(path.dirname(target), '.editor-studio-' + process.pid);
    const backup = stage + '-previous';
    await mkdir(stage);
    for (const file of result.files) {
      const output = path.join(stage, file.name);
      await mkdir(path.dirname(output), { recursive: true });
      await copyFile(path.join(result.directory, file.name), output);
    }
    await rename(target, backup);
    try { await rename(stage, target); }
    catch (error) { await rename(backup, target); throw error; }
    process.stdout.write(JSON.stringify({ project: fresh.project, revision: result.revision }));
  } else if (operation === 'clips') {
    const plan = JSON.parse(await readFile(planPath, 'utf8'));
    const studio = await tool('studio_read');
    if (studio.state.id && plan.namespace && !studio.state.id.startsWith(plan.namespace + ':'))
      throw new Error('The Studio project namespace must match the equipment mod');
    const existing = new Map(studio.clips.map((clip) => [clip.name, clip.id]));
    for (const clip of plan.clips) if (!existing.has(clip.name)) {
      const added = await guarded('studio_clip', { action: 'add', name: clip.name, length: clip.length, loop: clip.loop });
      existing.set(clip.name, added.clip.id);
    }
    if (plan.select) {
      const clip = existing.get(plan.select);
      if (!clip) throw new Error('The selected clip does not exist');
      await guarded('studio_clip', { action: 'select', clip });
    }
    process.stdout.write(JSON.stringify({ clips: [...existing.keys()] }));
  } else {
    const plan = JSON.parse(await readFile(planPath, 'utf8'));
    const reads = new Set(['read_project', 'studio_read', 'studio_validate']);
    const writes = new Set(['create_elements', 'update_elements', 'studio_clip', 'studio_keyframes', 'paint_texture']);
    if (!reads.has(plan.name) && !writes.has(plan.name)) throw new Error('Unsupported Blockbench operation');
    const result = reads.has(plan.name) ? await tool(plan.name, plan.args ?? {}) : await guarded(plan.name, plan.args ?? {});
    process.stdout.write(JSON.stringify(result));
  }
} finally {
  clearTimeout(timeout);
  reader.close();child.stdin.end();child.kill();
}
