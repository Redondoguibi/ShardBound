import { existsSync } from 'node:fs';
import path from 'node:path';
import { loadConfig, root } from '../mcp/config.mjs';
try {
  const config = loadConfig();
  console.log('Node: ' + process.version);
  console.log(
    'Paired Blockbench plugin: ' +
      (existsSync(path.join(root, '.local/shardbound_bridge.js'))
        ? 'OK'
        : 'MISSING — run setup'),
  );
  const base = `http://127.0.0.1:${config.bridgePort}`,
    headers = { Authorization: `Bearer ${config.token}` };
  const response = await fetch(base + '/health', {
    headers,
    signal: AbortSignal.timeout(2500),
  });
  if (!response.ok)
    throw new Error(
      'Bridge responded HTTP ' + response.status + '; verify pairing and port.',
    );
  console.log('Bridge: ' + JSON.stringify(await response.json()));
  const rpc = await fetch(base + '/rpc', {
    method: 'POST',
    headers: { ...headers, 'Content-Type': 'application/json' },
    body: JSON.stringify({ name: 'status', args: {} }),
    signal: AbortSignal.timeout(6000),
  });
  const result = await rpc.json();
  console.log('Blockbench: ' + JSON.stringify(result));
  if (!rpc.ok || result.result?.connected === false) process.exitCode = 1;
} catch (error) {
  console.error(
    error.message === 'fetch failed'
      ? 'Bridge is not running. Start with npm run http, or connect Codex.'
      : error.message,
  );
  process.exitCode = 1;
}
