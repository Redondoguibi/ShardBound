import test from 'node:test';
import assert from 'node:assert/strict';
import { access, readFile } from 'node:fs/promises';
import path from 'node:path';
import { buildPlugin, root } from '../scripts/build.mjs';

test('unpaired Blockbench plugin bundles successfully', async () => {
  const output = await buildPlugin(false);
  await access(output);
  const source = await readFile(output, 'utf8');
  assert.match(source, /shardbound_bridge/);
  assert.equal(path.dirname(output), path.join(root, 'plugin'));
});
