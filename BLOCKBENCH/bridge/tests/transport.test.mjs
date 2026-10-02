import test from 'node:test';
import assert from 'node:assert/strict';
import { randomBytes } from 'node:crypto';
import { once } from 'node:events';
import { WebSocket } from 'ws';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StreamableHTTPClientTransport } from '@modelcontextprotocol/sdk/client/streamableHttp.js';
import { startDaemon } from '../mcp/daemon.mjs';

test('HTTP MCP authenticates, lists schemas and serializes calls to paired plugin', async () => {
  const token = randomBytes(32).toString('base64url'),
    daemon = await startDaemon({ bridgePort: 0, token, outputs: 'unused' });
  const base = `http://127.0.0.1:${daemon.httpServer.address().port}`;
  let ws, client;
  try {
    assert.equal((await fetch(base + '/mcp', { method: 'POST' })).status, 401);
    assert.equal(
      (
        await fetch(base + '/health', {
          headers: {
            Authorization: `Bearer ${token}`,
            Origin: 'https://evil.test',
          },
        })
      ).status,
      403,
    );
    client = new Client({ name: 'test', version: '1' });
    await client.connect(
      new StreamableHTTPClientTransport(new URL(base + '/mcp'), {
        requestInit: { headers: { Authorization: `Bearer ${token}` } },
      }),
    );
    const list = await client.listTools();
    assert.equal(list.tools.length, 18);
    assert.equal(
      list.tools.find((t) => t.name === 'export_project').annotations
        .readOnlyHint,
      false,
    );
    assert.equal(
      list.tools.find((t) => t.name === 'export_project').annotations
        .idempotentHint,
      false,
    );
    const disconnected = await client.callTool({
      name: 'status',
      arguments: {},
    });
    assert.equal(JSON.parse(disconnected.content[0].text).connected, false);
    ws = new WebSocket(base.replace('http:', 'ws:') + '/blockbench');
    await once(ws, 'open');
    const ready = once(ws, 'message');
    ws.send(JSON.stringify({ type: 'hello', token }));
    assert.equal(JSON.parse((await ready)[0]).type, 'ready');
    let inFlight = 0,
      max = 0;
    const deadlines = [];
    ws.on('message', async (bytes) => {
      const m = JSON.parse(bytes);
      if (m.type !== 'call') return;
      deadlines.push(m.deadline);
      max = Math.max(max, ++inFlight);
      await new Promise((r) => setTimeout(r, 20));
      ws.send(
        JSON.stringify({ id: m.id, result: { project_id: 'p', revision: 1 } }),
      );
      inFlight--;
    });
    const calls = [
      daemon.call('status', {}),
      daemon.call('read_project', {}),
      daemon.call('__open_project_data', {
        path: 'test.bbmodel',
        content: '{}',
      }),
    ];
    const enqueuedAt = Date.now();
    const results = await Promise.all(calls);
    assert.ok(
      deadlines.every(
        (deadline) =>
          Number.isFinite(deadline) && deadline <= enqueuedAt + 45000,
      ),
      'Queue wait must consume the original request deadline',
    );
    assert.equal(max, 1);
    assert.equal(results[1].project_id, 'p');
    const result = await client.callTool({ name: 'status', arguments: {} });
    assert.equal(JSON.parse(result.content[0].text).project_id, 'p');
    const wrong = new WebSocket(base.replace('http:', 'ws:') + '/blockbench');
    await once(wrong, 'open');
    const rejected = once(wrong, 'close');
    wrong.send(JSON.stringify({ type: 'hello', token: 'wrong' }));
    assert.equal((await rejected)[0], 1008);
  } finally {
    ws?.terminate();
    await client?.close();
    await daemon.close();
  }
});
