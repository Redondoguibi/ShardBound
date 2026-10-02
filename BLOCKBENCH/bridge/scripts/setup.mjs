import { randomBytes } from 'node:crypto';
import { mkdir, writeFile, chmod } from 'node:fs/promises';
import net from 'node:net';
import path from 'node:path';
import { buildPlugin, root } from './build.mjs';

const local = path.join(root, '.local');
const repoRoot = path.resolve(root, '../..');
const configFile = path.join(local, 'config.json');
const passwordFile = path.join(local, 'chatgpt-password.txt');
const codexFile = path.join(local, 'codex-mcp.toml');

function secret(bytes = 32) {
  return randomBytes(bytes).toString('base64url');
}

function validatePublicUrl(value) {
  if (!value) return null;
  const url = new URL(value);
  if (url.protocol !== 'https:' || url.pathname !== '/' || url.search || url.hash || url.username || url.password) {
    throw new Error('BLOCKBENCH_BRIDGE_PUBLIC_URL must be an HTTPS origin without path, query, hash, or credentials.');
  }
  return url.origin;
}

function canListen(port) {
  return new Promise(resolve => {
    const server = net.createServer();
    server.unref();
    server.once('error', () => resolve(false));
    server.listen(port, '127.0.0.1', () => server.close(() => resolve(true)));
  });
}

async function choosePort() {
  const requested = process.env.BLOCKBENCH_BRIDGE_PORT ? Number(process.env.BLOCKBENCH_BRIDGE_PORT) : null;
  if (requested !== null) {
    if (!Number.isInteger(requested) || requested < 1024 || requested > 65535) throw new Error('BLOCKBENCH_BRIDGE_PORT must be an integer from 1024 to 65535.');
    if (!(await canListen(requested))) throw new Error(`Requested bridge port ${requested} is already in use.`);
    return requested;
  }
  for (let port = 30123; port <= 30163; port++) if (await canListen(port)) return port;
  throw new Error('No free bridge port found in 30123-30163. Set BLOCKBENCH_BRIDGE_PORT explicitly.');
}

function tomlString(value) {
  return JSON.stringify(String(value));
}

await mkdir(local, { recursive: true });
const bridgePort = await choosePort();
const publicUrl = validatePublicUrl(process.env.BLOCKBENCH_BRIDGE_PUBLIC_URL || '');
const config = {
  token: secret(),
  loginSecret: secret(24),
  bridgePort,
  publicUrl,
  outputs: path.join(root, 'outputs'),
  projectRoot: repoRoot,
  assetRoots: [path.join(repoRoot, 'assets')],
};

await writeFile(configFile, `${JSON.stringify(config, null, 2)}\n`, { mode: 0o600 });
await writeFile(passwordFile, `${config.loginSecret}\n`, { mode: 0o600 });
try { await chmod(configFile, 0o600); await chmod(passwordFile, 0o600); } catch {}

const pluginFile = await buildPlugin(true);
const serverFile = path.join(root, 'mcp/server.mjs');
const codexSnippet = [
  '[mcp_servers.shardbound-blockbench]',
  `command = ${tomlString(process.execPath)}`,
  `args = [${tomlString(serverFile)}]`,
  'startup_timeout_sec = 20',
  'tool_timeout_sec = 60',
  '',
].join('\n');
await writeFile(codexFile, codexSnippet, 'utf8');

console.log('ShardBound Blockbench Bridge configured.');
console.log(`Config: ${configFile}`);
console.log(`Paired Blockbench plugin: ${pluginFile}`);
console.log(`Codex MCP snippet: ${codexFile}`);
console.log(`ChatGPT local authorization password: ${passwordFile}`);
console.log('Next: load the paired .local/shardbound_bridge.js plugin in Blockbench, then run npm run doctor.');
if (!publicUrl) console.log('ChatGPT remote access is not enabled yet; configure an HTTPS MCP tunnel before setting BLOCKBENCH_BRIDGE_PUBLIC_URL.');
