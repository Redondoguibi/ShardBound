import { StdioServerTransport } from '@modelcontextprotocol/sdk/server/stdio.js';
import { spawn } from 'node:child_process';
import { mkdirSync, openSync, closeSync } from 'node:fs';
import path from 'node:path';
import { setTimeout as delay } from 'node:timers/promises';
import { loadConfig, root } from './config.mjs';
import { createMcp } from './server-tools.mjs';
import { startDaemon } from './daemon.mjs';
import { auditTransport } from './audit.mjs';

const config = loadConfig(),
  base = `http://127.0.0.1:${config.bridgePort}`;
async function health() {
  try {
    const r = await fetch(`${base}/health`, {
      headers: { Authorization: `Bearer ${config.token}` },
      signal: AbortSignal.timeout(800),
    });
    if (r.status === 401)
      throw new Error(
        'Port is occupied by a bridge with different pairing credentials.',
      );
    return r.ok && (await r.json()).service === 'shardbound-blockbench';
  } catch (error) {
    if (error.message.startsWith('Port is occupied')) throw error;
    return false;
  }
}
async function ensureDaemon() {
  if (await health()) return;
  mkdirSync(path.join(root, '.local'), { recursive: true });
  const log = openSync(path.join(root, '.local/bridge.log'), 'a');
  const child = spawn(process.execPath, [path.join(root, 'mcp/daemon.mjs')], {
    detached: true,
    windowsHide: true,
    stdio: ['ignore', log, log],
    env: { ...process.env, BLOCKBENCH_BRIDGE_CONFIG: config.filename },
  });
  child.on('error', (error) =>
    console.error('Could not start bridge:', error.message),
  );
  child.unref();
  closeSync(log);
  for (let i = 0; i < 30; i++) {
    await delay(200);
    if (await health()) return;
  }
  throw new Error('Bridge startup failed. Inspect .local/bridge.log.');
}
if (process.argv.includes('--http')) {
  const daemon = await startDaemon(config);
  console.error(`Bridge HTTP listening on ${base}/mcp`);
  for (const signal of ['SIGINT', 'SIGTERM'])
    process.on(signal, async () => {
      await daemon.close();
      process.exit(0);
    });
} else {
  await ensureDaemon();
  const server = createMcp(async (name, args) => {
    const response = await fetch(`${base}/rpc`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${config.token}`,
      },
      body: JSON.stringify({ name, args }),
      signal: AbortSignal.timeout(name === 'studio_export' ? 185000 : 50000),
    });
    const result = await response.json();
    if (!response.ok || result.error)
      throw new Error(result.error || `Bridge HTTP ${response.status}`);
    return result.result;
  }, config);
  const transport = new StdioServerTransport();
  const attachAudit = auditTransport(transport, path.join(root, '.local/mcp-audit.jsonl'));
  await server.connect(transport);
  attachAudit();
}
