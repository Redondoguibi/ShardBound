import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { randomBytes, createHash } from 'node:crypto';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { StreamableHTTPClientTransport } from '@modelcontextprotocol/sdk/client/streamableHttp.js';
import { loadConfig, root } from '../mcp/config.mjs';

const config = loadConfig();
const state = JSON.parse(await readFile(path.join(root, '.local/chatgpt-session.json'), 'utf8'));
const local = await fetch(`http://127.0.0.1:${config.bridgePort}/health`, {
  headers: { Authorization: `Bearer ${config.token}` }, signal: AbortSignal.timeout(3000),
});
assert.equal((await local.json()).publicUrl, state.publicUrl, 'Tunnel must belong to the running local bridge');
const base = state.publicUrl;
assert.equal(new URL(base).protocol, 'https:');
const request = (url, options = {}) => fetch(url, { ...options, redirect: 'manual', signal: AbortSignal.timeout(15000) });
const form = (endpoint, values) => request(base + endpoint, {
  method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: new URLSearchParams(values),
});
for (const route of ['/health', '/rpc', '/shutdown', '/blockbench'])
  assert.equal((await request(base + route)).status, 404, route + ' must not be public');
assert.equal((await request(base + '/mcp', { method: 'POST' })).status, 401);
const metadata = await (await request(base + '/.well-known/oauth-protected-resource/mcp')).json();
assert.equal(metadata.resource, base + '/mcp');
const registration = await request(base + '/register', {
  method: 'POST', headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ redirect_uris: ['http://127.0.0.1/callback'], token_endpoint_auth_method: 'none', grant_types: ['authorization_code', 'refresh_token'], response_types: ['code'], client_name: 'ShardBound HTTPS self-test' }),
});
assert.equal(registration.status, 201);
const clientInfo = await registration.json();
const verifier = randomBytes(32).toString('base64url');
const query = new URLSearchParams({ client_id: clientInfo.client_id, redirect_uri: clientInfo.redirect_uris[0], response_type: 'code', scope: 'blockbench', code_challenge_method: 'S256', code_challenge: createHash('sha256').update(verifier).digest('base64url'), resource: metadata.resource, state: randomBytes(16).toString('hex') });
const page = await request(base + '/authorize?' + query);
assert.equal(page.status, 200);
const nonce = /name="nonce" value="([^"]+)"/.exec(await page.text())?.[1];
assert.ok(nonce);
const approval = await form('/approve', { nonce, password: config.loginSecret });
assert.equal(approval.status, 302);
const callback = new URL(approval.headers.get('location'));
assert.equal(callback.searchParams.get('state'), query.get('state'));
const tokenResponse = await form('/token', { grant_type: 'authorization_code', client_id: clientInfo.client_id, redirect_uri: clientInfo.redirect_uris[0], code: callback.searchParams.get('code'), resource: metadata.resource, code_verifier: verifier });
assert.equal(tokenResponse.status, 200);
const tokens = await tokenResponse.json();
const client = new Client({ name: 'HTTPS verification', version: '1' });
try {
  await client.connect(new StreamableHTTPClientTransport(new URL(metadata.resource), { requestInit: { headers: { Authorization: `Bearer ${tokens.access_token}` } } }));
  const listed = await client.listTools();
  assert.equal(listed.tools.length, 18);
  for (const name of ['status', 'read_project', 'capture_view', 'mtools_run']) assert.ok(listed.tools.some(t => t.name === name));
  const status = await client.callTool({ name: 'status', arguments: {} });
  assert.notEqual(status.isError, true);
  console.log(JSON.stringify({ mcpUrl: metadata.resource, oauth: 'PASS', tools: listed.tools.map(t => t.name), status: JSON.parse(status.content[0].text) }, null, 2));
} finally {
  await client.close();
  const revoked = await form('/revoke', { client_id: clientInfo.client_id, token: tokens.access_token });
  assert.equal(revoked.status, 200);
}
