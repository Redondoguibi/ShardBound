import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { randomBytes } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { root, validateConfig } from '../mcp/config.mjs';
import { buildPlugin } from './build.mjs';

const args = process.argv.slice(2),
  dir = path.join(root, '.local'),
  file = path.join(dir, 'config.json');
const option = (name) => {
  const i = args.indexOf(name);
  if (i < 0) return undefined;
  if (!args[i + 1] || args[i + 1].startsWith('--'))
    throw new Error(`Missing value for ${name}`);
  return args[i + 1];
};
await mkdir(dir, { recursive: true });
let config;
try {
  config = JSON.parse(await readFile(file, 'utf8'));
} catch (error) {
  if (error.code !== 'ENOENT') throw error;
}
config ??= {
  token: randomBytes(32).toString('base64url'),
  loginSecret: randomBytes(32).toString('base64url'),
  bridgePort: 30123,
  outputs: path.join(root, 'outputs'),
};
config.loginSecret ??= randomBytes(32).toString('base64url');
if (option('--port')) config.bridgePort = Number(option('--port'));
if (option('--public-url'))
  config.publicUrl = new URL(option('--public-url')).href;
if (args.includes('--local-only')) delete config.publicUrl;
validateConfig(config);
await writeFile(file, JSON.stringify(config, null, 2) + '\n', { mode: 0o600 });
await writeFile(
  path.join(dir, 'chatgpt-password.txt'),
  config.loginSecret + '\n',
  { mode: 0o600 },
);
await buildPlugin();
await buildPlugin(true);
await writeFile(
  path.join(dir, 'codex-config.toml'),
  `[mcp_servers.shardbound_blockbench]\ncommand = ${JSON.stringify(process.execPath.replaceAll('\\', '/'))}\nargs = [${JSON.stringify(path.join(root, 'mcp/server.mjs').replaceAll('\\', '/'))}]\nstartup_timeout_sec = 30\ntool_timeout_sec = 60\n`,
);
console.log(
  'Ready. Load this plugin in Blockbench (File > Plugins > Load Plugin from File):\n' +
    path.join(dir, 'shardbound_bridge.js'),
);
const mtools = path.resolve(root, '../MTools/mesh_tools.js');
console.log(
  existsSync(mtools)
    ? 'MTools plugin: ' + mtools
    : 'Install MTools 2.1.0 separately in Blockbench to enable its adapted tools.',
);
console.log(
  'Local configuration: ' +
    file +
    '\nOAuth password file: ' +
    path.join(dir, 'chatgpt-password.txt'),
);
if (args.includes('--register-codex')) {
  const executable = process.env.CODEX_EXECUTABLE || 'codex';
  const result = spawnSync(
    executable,
    [
      'mcp',
      'add',
      'shardbound_blockbench',
      '--',
      process.execPath,
      path.join(root, 'mcp/server.mjs'),
    ],
    { stdio: 'inherit', windowsHide: true },
  );
  if (result.error || result.status !== 0)
    throw new Error(
      'Codex registration failed. Import .local/codex-config.toml in MCP settings.',
    );
  console.log(
    'Codex MCP registered. Restart the MCP connection or open a new chat.',
  );
}
console.log(
  'Start HTTP: npm run http. Diagnose: npm run doctor. Tests: npm test.',
);
