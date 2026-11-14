import { loadConfig } from '../mcp/config.mjs';
const config = loadConfig();
let response;
try { response = await fetch(`http://127.0.0.1:${config.bridgePort}/shutdown`, {
  method: 'POST',
  headers: { Authorization: `Bearer ${config.token}` },
  signal: AbortSignal.timeout(3000),
}); } catch (error) {
  if (process.argv.includes('--if-running') && error.cause?.code === 'ECONNREFUSED') console.log('Bridge already stopped.');
  else throw error;
}
if (response && !response.ok)
  throw new Error('Could not stop bridge: HTTP ' + response.status);
if (response) console.log('Bridge stopped.');
