import { spawn } from 'node:child_process';
import { mkdir, readFile, writeFile, open, unlink } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';
import { loadConfig, root } from '../mcp/config.mjs';
import { startDaemon } from '../mcp/daemon.mjs';
import { startRemoteGateway } from '../mcp/remote-gateway.mjs';

const local = path.join(root, '.local');
const stateFile = path.join(local, 'chatgpt-session.json');
const logFile = path.join(local, 'chatgpt.log');
const config = loadConfig();
const base = `http://127.0.0.1:${config.bridgePort}`;
const headers = { Authorization: `Bearer ${config.token}` };
async function health() {
  try {
    const response = await fetch(base + '/health', { headers, signal: AbortSignal.timeout(1500) });
    if (!response.ok) throw new Error(`Bridge health HTTP ${response.status}`);
    const result = await response.json();
    if (result.service !== 'shardbound-blockbench') throw new Error('Unexpected service on bridge port');
    return result;
  } catch (error) {
    if (error.cause?.code === 'ECONNREFUSED') return null;
    throw error;
  }
}
async function session() {
  try {
    const state = JSON.parse(await readFile(stateFile, 'utf8'));
    const live = await health();
    return live?.publicUrl === state.publicUrl ? { ...state, connected: live.connected } : null;
  } catch (error) {
    if (error.code === 'ENOENT') return null;
    throw error;
  }
}
function show(state) {
  console.log(JSON.stringify({ ...state, passwordFile: path.join(local, 'chatgpt-password.txt') }, null, 2));
}

async function run() {
  const executable = process.env.CLOUDFLARED_PATH || path.join(local, 'bin', 'cloudflared.exe');
  if (!existsSync(executable)) throw new Error('Install cloudflared with scripts/Install-Cloudflared.ps1 first.');
  const lockFile = path.join(local, 'chatgpt.lock');
  try {
    const previousPid = Number(await readFile(lockFile, 'utf8'));
    try { process.kill(previousPid, 0); throw new Error('A ChatGPT tunnel process is already running.'); }
    catch (error) { if (error.code !== 'ESRCH') throw error; }
    await unlink(lockFile);
  } catch (error) { if (error.code !== 'ENOENT') throw error; }
  const lock = await open(lockFile, 'wx');
  await lock.writeFile(String(process.pid));
  await lock.close();
  let gateway, tunnel, daemon, closing = false;
  const close = async () => {
    if (closing) return;
    closing = true;
    tunnel?.kill();
    await gateway?.close();
    await daemon?.close();
    await unlink(stateFile).catch(() => {});
    await unlink(lockFile).catch(() => {});
  };
  for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, () => void close());
  try {
    gateway = await startRemoteGateway(config.bridgePort);
    const publicUrl = await new Promise((resolve, reject) => {
      const timer = setTimeout(() => reject(new Error('Tunnel URL timed out. See .local/chatgpt.log.')), 45000);
      let output = '';
      tunnel = spawn(executable, ['tunnel', '--no-autoupdate', '--protocol', 'http2', '--url', `http://127.0.0.1:${gateway.port}`], { windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] });
      const consume = bytes => {
        const text = bytes.toString();
        process.stderr.write(text);
        output = (output + text).slice(-12000);
        const match = output.match(/https:\/\/[a-z0-9-]+\.trycloudflare\.com\b/);
        if (match) { clearTimeout(timer); resolve(match[0]); }
      };
      tunnel.stdout.on('data', consume);
      tunnel.stderr.on('data', consume);
      tunnel.once('error', error => { clearTimeout(timer); reject(error); });
      tunnel.once('exit', code => { clearTimeout(timer); reject(new Error(`Tunnel exited: ${code}`)); void close(); });
    });
    const previous = await health();
    if (previous) {
      const stopped = await fetch(base + '/shutdown', { method: 'POST', headers, signal: AbortSignal.timeout(3000) });
      if (!stopped.ok) throw new Error('Could not restart bridge with OAuth enabled.');
      for (let i = 0; i < 30 && await health(); i++) await delay(100);
    }
    // publicUrl is ephemeral runtime state. Preserve the user's config and pairing.
    daemon = await startDaemon({ ...config, publicUrl });
    daemon.httpServer.once('close', () => void close());
    gateway.enable();
    await writeFile(stateFile, JSON.stringify({ publicUrl, mcpUrl: publicUrl + '/mcp', pid: process.pid, startedAt: new Date().toISOString(), temporary: true }, null, 2));
    console.log('ChatGPT HTTPS bridge ready: ' + publicUrl + '/mcp');
  } catch (error) {
    await close();
    throw error;
  }
}

await mkdir(local, { recursive: true });
const command = process.argv[2] || 'status';
if (command === 'run') await run();
else if (command === 'start') {
  const existing = await session();
  if (existing) show(existing);
  else {
    const log = await open(logFile, 'a');
    const child = spawn(process.execPath, [fileURLToPath(import.meta.url), 'run'], { detached: true, windowsHide: true, stdio: ['ignore', log.fd, log.fd] });
    child.unref();
    await log.close();
    let current;
    for (let i = 0; i < 120; i++) {
      current = await session();
      if (current) break;
      if (child.exitCode !== null) throw new Error('Tunnel startup failed. See ' + logFile);
      await delay(500);
    }
    if (!current) throw new Error('Tunnel startup is still pending. See ' + logFile);
    show(current);
  }
} else if (command === 'stop') {
  const current = await session();
  if (!current) console.log('No managed ChatGPT tunnel is active.');
  else {
    const response = await fetch(base + '/shutdown', { method: 'POST', headers, signal: AbortSignal.timeout(3000) });
    if (!response.ok) throw new Error('Shutdown failed: ' + response.status);
    console.log('ChatGPT bridge and temporary tunnel stopped. Codex can restart its local daemon.');
  }
} else if (command === 'status') {
  const current = await session();
  if (current) show(current);
  else { console.log('ChatGPT tunnel is not active. Run npm run chatgpt:start.'); process.exitCode = 1; }
} else throw new Error('Use start, stop, status or run.');
