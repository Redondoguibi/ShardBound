import { access, mkdir } from 'node:fs/promises';
import net from 'node:net';
import path from 'node:path';
import { loadConfig, root } from '../mcp/config.mjs';
import { checkController } from './check-controller.mjs';

const offline = process.argv.includes('--offline');
let errors = 0;
const ok = (message) => console.log(`[OK] ${message}`);
const warn = (message) => console.log(`[WARN] ${message}`);
const fail = (message) => {
  errors++;
  console.error(`[FAIL] ${message}`);
};

const nodeMajor = Number(process.versions.node.split('.')[0]);
if (nodeMajor >= 22) ok(`Node ${process.versions.node}`);
else fail(`Node ${process.versions.node}; Node 22+ is required.`);

let config;
try {
  config = loadConfig();
  ok(`Configuration: ${config.filename}`);
} catch (error) {
  fail(error.message);
}

for (const [label, filename] of [
  ['Paired Blockbench plugin', path.join(root, '.local/shardbound_bridge.js')],
  ['Codex MCP snippet', path.join(root, '.local/codex-mcp.toml')],
  ['MTools 2.1.0 source', path.resolve(root, '../MTools/mesh_tools.js')],
]) {
  try {
    await access(filename);
    ok(label);
  } catch {
    (label.startsWith('MTools') ? warn : fail)(`${label} missing: ${filename}`);
  }
}

try {
  await import('@modelcontextprotocol/sdk/server/mcp.js');
  await import('express');
  await import('ws');
  await import('zod');
  ok('Runtime dependencies');
} catch (error) {
  fail(
    `Runtime dependencies are incomplete. Run npm install. (${error.message})`,
  );
}

if (config) {
  try {
    await mkdir(config.outputs, { recursive: true });
    await access(config.outputs);
    ok(`Output directory: ${config.outputs}`);
  } catch (error) {
    fail(`Output directory is not usable: ${error.message}`);
  }

  if (config.projectRoot) {
    try {
      await access(config.projectRoot);
      ok(`Project root: ${config.projectRoot}`);
    } catch {
      fail(`Project root does not exist: ${config.projectRoot}`);
    }
  }

  if (!offline) {
    const base = `http://127.0.0.1:${config.bridgePort}`;
    try {
      const response = await fetch(`${base}/health`, {
        headers: { Authorization: `Bearer ${config.token}` },
        signal: AbortSignal.timeout(900),
      });
      if (!response.ok)
        fail(`Bridge health endpoint returned HTTP ${response.status}.`);
      else {
        const health = await response.json();
        ok(`MCP daemon listening on 127.0.0.1:${config.bridgePort}`);
        if (health.service !== 'shardbound-blockbench')
          fail('The health endpoint does not belong to ShardBound Blockbench.');
        else if (!health.connected)
          fail('MCP daemon is running, but Blockbench is not connected.');
        else {
          try {
            const status = await checkController(config);
            ok(
              `Blockbench ${status.blockbench}: controller responded to status RPC`,
            );
            console.log('Blockbench: ' + JSON.stringify(status));
          } catch (error) {
            fail(
              `Blockbench is connected, but its controller failed: ${error.message}`,
            );
          }
        }
        if (health.publicUrl)
          ok(`Remote MCP URL configured: ${health.publicUrl}/mcp`);
        else
          ok('Loopback bridge configured. The OpenAI Tunnel does not require publicUrl; use Status-ChatGPT-Desktop-Plugin.ps1 to verify the Chat connection.');
      }
    } catch {
      const free = await new Promise((resolve) => {
        const server = net.createServer();
        server.unref();
        server.once('error', () => resolve(false));
        server.listen(config.bridgePort, '127.0.0.1', () =>
          server.close(() => resolve(true)),
        );
      });
      if (free)
        fail(
          `MCP daemon is not running on port ${config.bridgePort}. It will auto-start when the stdio MCP server is launched.`,
        );
      else
        fail(
          `Port ${config.bridgePort} is occupied by another process and the bridge health check failed.`,
        );
    }
  } else
    ok(
      'Offline checks complete; daemon and Blockbench connection were not tested.',
    );
}

if (errors) {
  console.error(
    `\nDoctor found ${errors} blocking problem${errors === 1 ? '' : 's'}.`,
  );
  process.exitCode = 1;
} else {
  console.log('\nDoctor found no blocking problems.');
}
