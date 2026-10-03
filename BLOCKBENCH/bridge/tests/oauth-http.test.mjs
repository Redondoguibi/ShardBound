import test from 'node:test';
import assert from 'node:assert/strict';
import { randomBytes, createHash } from 'node:crypto';
import { startDaemon } from '../mcp/daemon.mjs';

test('OAuth HTTP discovery, DCR, S256 PKCE, consent and protected MCP', async () => {
  const secret = randomBytes(32).toString('base64url');
  const daemon = await startDaemon({
    token: secret,
    loginSecret: secret,
    bridgePort: 0,
    publicUrl: 'https://bridge.test',
    outputs: 'unused',
  });
  const base = `http://127.0.0.1:${daemon.httpServer.address().port}`;
  const form = async (endpoint, values, origin) =>
    fetch(base + endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded', ...(origin ? { Origin: origin } : {}) },
      body: new URLSearchParams(values),
      redirect: 'manual',
    });
  try {
    const denied = await fetch(base + '/mcp', { method: 'POST' });
    assert.equal(denied.status, 401);
    assert.match(
      denied.headers.get('www-authenticate'),
      /oauth-protected-resource/,
    );
    const metadata = await (
      await fetch(base + '/.well-known/oauth-protected-resource/mcp')
    ).json();
    assert.equal(metadata.resource, 'https://bridge.test/mcp');
    const registered = await fetch(base + '/register', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        redirect_uris: ['https://client.test/callback'],
        token_endpoint_auth_method: 'none',
        grant_types: ['authorization_code', 'refresh_token'],
        response_types: ['code'],
        client_name: 'Test client',
      }),
    });
    assert.equal(registered.status, 201);
    const client = await registered.json();
    const verifier = randomBytes(32).toString('base64url');
    const query = new URLSearchParams({
      client_id: client.client_id,
      redirect_uri: client.redirect_uris[0],
      response_type: 'code',
      scope: 'blockbench',
      code_challenge_method: 'S256',
      code_challenge: createHash('sha256').update(verifier).digest('base64url'),
      resource: metadata.resource,
      state: 'test-state',
    });
    const page = await fetch(base + '/authorize?' + query);
    assert.equal(page.status, 200);
    assert.equal(page.headers.get('referrer-policy'), 'same-origin');
    const html = await page.text();
    const nonce = /name="nonce" value="([^"]+)"/.exec(html)[1];
    assert.equal((await form('/approve', { nonce })).status, 403);
    for (const origin of ['null', 'https://untrusted.test']) {
      assert.equal((await form('/approve', { nonce, password: secret }, origin)).status, 403);
    }
    const approval = await form('/approve', { nonce, password: secret }, 'https://bridge.test');
    assert.equal(approval.status, 302);
    const callback = new URL(approval.headers.get('location'));
    assert.equal(callback.searchParams.get('state'), 'test-state');
    const values = {
      grant_type: 'authorization_code',
      client_id: client.client_id,
      redirect_uri: client.redirect_uris[0],
      code: callback.searchParams.get('code'),
      resource: metadata.resource,
      code_verifier: verifier,
    };
    assert.equal(
      (await form('/token', { ...values, code_verifier: 'incorrect' })).status,
      400,
    );
    const tokenResponse = await form('/token', values);
    assert.equal(tokenResponse.status, 200);
    const token = await tokenResponse.json();
    const mcp = await fetch(base + '/mcp', {
      method: 'POST',
      headers: {
        Authorization: `Bearer ${token.access_token}`,
        'Content-Type': 'application/json',
        Accept: 'application/json, text/event-stream',
      },
      body: JSON.stringify({
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: {
          protocolVersion: '2025-03-26',
          capabilities: {},
          clientInfo: { name: 'test', version: '1' },
        },
      }),
    });
    assert.equal(mcp.status, 200);
    assert.ok((await mcp.json()).result.serverInfo);
    const refreshed = await form('/token', {
      grant_type: 'refresh_token',
      client_id: client.client_id,
      refresh_token: token.refresh_token,
      resource: metadata.resource,
    });
    assert.equal(refreshed.status, 200);
    assert.equal((await form('/token', values)).status, 400);
  } finally {
    await daemon.close();
  }
});
