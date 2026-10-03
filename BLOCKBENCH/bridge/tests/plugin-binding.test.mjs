import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const APP_ID = 'asdk_app_6ac0dda2381c8191b52e8f5f29526256';
const root = new URL('../', import.meta.url);

async function readJson(path) {
  return JSON.parse(await readFile(new URL(path, root), 'utf8'));
}

test('ChatGPT plugin binds the registered ShardBound MCP app', async () => {
  const app = await readJson('.app.json');
  const portable = await readJson('plugin.json');
  const compat = await readJson('.codex-plugin/plugin.json');
  const localMcp = await readJson('.mcp.json');
  const portableMcp = await readJson('mcp.json');

  assert.deepEqual(app.apps.shardbound_blockbench, {
    id: APP_ID,
    required: true,
  });
  assert.equal(portable.extensions?.['com.openai']?.apps, './.app.json');
  assert.equal(compat.apps, './.app.json');
  assert.ok(localMcp.mcpServers?.shardbound_blockbench);
  assert.ok(portableMcp.mcpServers?.shardbound_blockbench);
});
