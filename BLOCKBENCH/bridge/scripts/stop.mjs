import { loadConfig } from '../mcp/config.mjs';
const config = loadConfig();
const response = await fetch(`http://127.0.0.1:${config.bridgePort}/shutdown`, {
  method: 'POST',
  headers: { Authorization: `Bearer ${config.token}` },
  signal: AbortSignal.timeout(3000),
});
if (!response.ok)
  throw new Error('Could not stop bridge: HTTP ' + response.status);
console.log('Bridge stopped.');
