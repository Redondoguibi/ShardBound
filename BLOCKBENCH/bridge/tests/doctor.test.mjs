import test from 'node:test';
import assert from 'node:assert/strict';
import http from 'node:http';
import { once } from 'node:events';
import { checkController } from '../scripts/check-controller.mjs';

test('doctor checks the controller response, not just an open connection', async () => {
  let payload = { result: { blockbench: '5.2.1', project_id: null } };
  let authorization;
  const server = http.createServer((req, res) => {
    authorization = req.headers.authorization;
    assert.equal(req.url, '/rpc');
    assert.equal(req.method, 'POST');
    req.resume();
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify(payload));
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const config = {
    bridgePort: server.address().port,
    token: 'test-only-token',
  };
  try {
    assert.equal((await checkController(config)).blockbench, '5.2.1');
    assert.equal(authorization, 'Bearer test-only-token');
    for (const invalid of [{ connected: false }, {}, null]) {
      payload = { result: invalid };
      await assert.rejects(checkController(config), /valid status/);
    }
    payload = { error: 'controller.call is not a function' };
    await assert.rejects(checkController(config), /controller.call/);
  } finally {
    server.closeAllConnections();
    await new Promise((resolve) => server.close(resolve));
  }
});
