import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

const root = new URL('../', import.meta.url);
const read = (path) => readFile(new URL(path, root), 'utf8');

test('default ChatGPT connection uses OpenAI Secure MCP Tunnel', async () => {
  const connect = await read('Connect-ChatGPT.ps1');
  const install = await read('scripts/Install-TunnelClient.ps1');
  const configure = await read('scripts/Configure-SecureTunnel.ps1');
  const common = await read('scripts/SecureTunnel-Common.ps1');
  const pkg = JSON.parse(await read('package.json'));

  assert.match(connect, /runtimes connect/);
  assert.match(connect, /--mcp-command/);
  assert.match(connect, /env:CONTROL_PLANE_API_KEY/);
  assert.doesNotMatch(connect, /trycloudflare|cloudflared/i);

  assert.match(install, /openai\/tunnel-client\/releases\/latest/);
  assert.match(install, /SHA256SUMS\.txt/);
  assert.match(install, /Get-FileHash/);

  assert.match(configure, /Save-RuntimeApiKey/);
  assert.match(common, /AbsoluteUri/);
  assert.match(common, /node -e import/);
  assert.doesNotMatch(common, /return \('\"\{0\}\" \"\{1\}\"'/);
  assert.equal(pkg.scripts['chatgpt:start'].includes('Connect-ChatGPT.ps1'), true);
  assert.equal(pkg.scripts['chatgpt:legacy:start'], 'node scripts/chatgpt.mjs start');
});
