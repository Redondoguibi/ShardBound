import test from 'node:test';
import assert from 'node:assert/strict';
import { randomBytes, createHash } from 'node:crypto';
import { equalSecret, validateConfig } from '../mcp/config.mjs';
import { BridgeOAuth } from '../mcp/auth.mjs';
import { tools, validateMesh } from '../shared/tools.mjs';
const token = randomBytes(32).toString('base64url');

test('missing or malformed secrets fail closed', () => {
  for (const value of [undefined, null, '', false, {}, []])
    assert.equal(equalSecret(value, value), false);
  assert.equal(equalSecret(token, token), true);
  assert.equal(equalSecret(token, token + 'x'), false);
  assert.throws(
    () =>
      validateConfig({
        token,
        bridgePort: 30123,
        publicUrl: 'https://example.test',
      }),
    /loginSecret/,
  );
  assert.throws(
    () => new BridgeOAuth({ publicUrl: 'https://example.test' }),
    /loginSecret/,
  );
});
test('schema rejects unknown edits, unsafe names and invalid topology', () => {
  assert.equal(
    tools.update_elements.schema.safeParse({
      project_id: 'p',
      updates: [{ id: 'x', script: 'bad' }],
    }).success,
    false,
  );
  assert.equal(
    tools.export_project.schema.safeParse({
      project_id: 'p',
      filename: '../outside',
    }).success,
    false,
  );
  assert.throws(
    () =>
      validateMesh({
        vertices: { a: [0, 0, 0] },
        faces: { f: { vertices: ['a', 'b', 'c'] } },
      }),
    /Invalid vertices/,
  );
});
test('OAuth authorization, client binding, code replay and refresh rotation', async () => {
  const auth = new BridgeOAuth({
    publicUrl: 'https://bridge.test',
    loginSecret: token,
  });
  const client = await auth.clientsStore.registerClient({
    redirect_uris: ['https://client.test/callback'],
    client_name: 'test',
  });
  const other = { client_id: 'other' };
  let html, redirect, status;
  const res = {
    set() {
      return this;
    },
    send(v) {
      html = v;
      return this;
    },
    status(v) {
      status = v;
      return this;
    },
    redirect(v) {
      redirect = v;
      return this;
    },
  };
  const params = {
    redirectUri: client.redirect_uris[0],
    codeChallenge: 'challenge',
    scopes: ['blockbench'],
    state: 'state',
    resource: new URL(auth.resource),
  };
  await auth.authorize(client, params, res);
  const nonce = /name="nonce" value="([^"]+)"/.exec(html)[1];
  auth.approve({ headers: {}, body: { nonce } }, res);
  assert.equal(status, 403);
  assert.equal(redirect, undefined);
  auth.approve({ headers: {}, body: { nonce, password: token } }, res);
  assert.equal(new URL(redirect).searchParams.get('state'), 'state');
  const code = new URL(redirect).searchParams.get('code');
  await assert.rejects(auth.challengeForAuthorizationCode(other, code));
  assert.equal(
    await auth.challengeForAuthorizationCode(client, code),
    'challenge',
  );
  await assert.rejects(
    auth.exchangeAuthorizationCode(
      client,
      code,
      undefined,
      'https://wrong.test',
    ),
  );
  const tokens = await auth.exchangeAuthorizationCode(
    client,
    code,
    undefined,
    params.redirectUri,
    params.resource,
  );
  assert.equal(
    (await auth.verifyAccessToken(tokens.access_token)).clientId,
    client.client_id,
  );
  await assert.rejects(
    auth.exchangeAuthorizationCode(client, code, undefined, params.redirectUri),
  );
  await assert.rejects(auth.exchangeRefreshToken(other, tokens.refresh_token));
  const next = await auth.exchangeRefreshToken(client, tokens.refresh_token);
  await assert.rejects(auth.verifyAccessToken(tokens.access_token));
  await assert.rejects(auth.exchangeRefreshToken(client, tokens.refresh_token));
  await auth.revokeToken(client, { token: next.access_token });
  await assert.rejects(auth.verifyAccessToken(next.access_token));
});
