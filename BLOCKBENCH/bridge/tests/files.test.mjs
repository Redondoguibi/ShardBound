import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { createMcp } from '../mcp/server-tools.mjs';

test('repository saves preserve existing files unless overwrite is explicit', async () => {
  const projectRoot = await mkdtemp(
    path.join(os.tmpdir(), 'shardbound-files-'),
  );
  const assetRoot = path.join(projectRoot, 'assets');
  await mkdir(assetRoot);
  let content = '{"name":"original"}';
  let opened;
  const server = createMcp(
    async (name, args) => {
      if (name === '__open_project_data') {
        opened = args;
        return { project_id: 'p' };
      }
      return { format: 'bbmodel', content };
    },
    {
      projectRoot,
      assetRoots: [assetRoot],
      outputs: path.join(projectRoot, 'outputs'),
    },
  );
  const client = new Client({ name: 'files-test', version: '1' });
  const [clientTransport, serverTransport] =
    InMemoryTransport.createLinkedPair();
  await server.connect(serverTransport);
  await client.connect(clientTransport);
  const destination = 'assets/source.bbmodel';
  const save = (extra) =>
    client.callTool({
      name: 'save_project',
      arguments: { project_id: 'p', path: destination, ...extra },
    });
  try {
    assert.notEqual((await save()).isError, true);
    content = '{"name":"edited"}';
    assert.equal((await save()).isError, true);
    assert.equal(
      await readFile(path.join(projectRoot, destination), 'utf8'),
      '{"name":"original"}',
    );
    assert.notEqual((await save({ overwrite: true })).isError, true);
    assert.equal(
      await readFile(path.join(projectRoot, destination), 'utf8'),
      content,
    );
    const listed = await client.callTool({
      name: 'list_model_files',
      arguments: { directory: 'assets' },
    });
    assert.equal(JSON.parse(listed.content[0].text).files[0].path, destination);
    assert.notEqual(
      (
        await client.callTool({
          name: 'open_project_file',
          arguments: { path: destination },
        })
      ).isError,
      true,
    );
    assert.equal(opened.content, content);
    assert.equal(opened.path, path.join(projectRoot, destination));
  } finally {
    await client.close();
    await server.close();
    assert.equal(
      path.dirname(path.resolve(projectRoot)),
      path.resolve(os.tmpdir()),
    );
    assert.ok(path.basename(projectRoot).startsWith('shardbound-files-'));
    await rm(projectRoot, { recursive: true, force: true });
  }
});
