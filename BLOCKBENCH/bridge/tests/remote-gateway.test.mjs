import test from 'node:test';
import assert from 'node:assert/strict';
import { randomBytes } from 'node:crypto';
import { startDaemon } from '../mcp/daemon.mjs';
import { startRemoteGateway } from '../mcp/remote-gateway.mjs';

test('public gateway exposes authenticated MCP and OAuth, excludes local routes', async () => {
  const token = randomBytes(32).toString('base64url');
  const daemon = await startDaemon({ token, loginSecret: token, bridgePort: 0, publicUrl: 'https://bridge.test' });
  const gateway = await startRemoteGateway(daemon.httpServer.address().port);
  const base = `http://127.0.0.1:${gateway.port}`;
  try {
    assert.equal((await fetch(base + '/mcp', { method: 'POST' })).status, 503);
    gateway.enable();
    for (const route of ['/health', '/rpc', '/shutdown', '/blockbench', '/.local/config.json']) {
      assert.equal((await fetch(base + route, { method: 'POST', headers: { Authorization: `Bearer ${token}` } })).status, 404);
    }
    const response = await fetch(base + '/mcp', { method: 'POST' });
    assert.equal(response.status, 401);
    assert.match(response.headers.get('www-authenticate'), /oauth-protected-resource/);
    const metadata = await (await fetch(base + '/.well-known/oauth-authorization-server')).json();
    assert.equal(metadata.authorization_endpoint, 'https://bridge.test/authorize');
    assert.equal(metadata.token_endpoint, 'https://bridge.test/token');
    assert.deepEqual(metadata.code_challenge_methods_supported, ['S256']);
    const registered = await fetch(base + '/register', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ redirect_uris: ['http://127.0.0.1/callback'], token_endpoint_auth_method: 'none' }),
    });
    assert.equal(registered.status, 201);
  } finally {
    await gateway.close();
    await daemon.close();
  }
});
