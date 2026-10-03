import http from 'node:http';

// The tunnel can reach OAuth and MCP only, never pairing or local administration.
const routes = new Map([
  ['/mcp', ['POST', 'GET', 'DELETE', 'OPTIONS']],
  ['/authorize', ['GET']],
  ['/approve', ['POST']],
  ['/register', ['POST']],
  ['/token', ['POST']],
  ['/revoke', ['POST']],
  ['/.well-known/oauth-authorization-server', ['GET']],
  ['/.well-known/oauth-protected-resource/mcp', ['GET']],
]);

export async function startRemoteGateway(bridgePort) {
  let enabled = false;
  const server = http.createServer((req, res) => {
    const pathname = req.url.split('?')[0];
    if (!routes.get(pathname)?.includes(req.method)) {
      res.writeHead(404).end();
      return;
    }
    if (!enabled) {
      res.writeHead(503).end('Starting authenticated bridge');
      return;
    }
    const proxy = http.request({
      hostname: '127.0.0.1', port: bridgePort, method: req.method,
      path: req.url, headers: { ...req.headers, host: `127.0.0.1:${bridgePort}` },
      timeout: 55000,
    }, upstream => {
      res.writeHead(upstream.statusCode, upstream.headers);
      upstream.pipe(res);
    });
    proxy.on('error', () => {
      if (!res.headersSent) res.writeHead(502);
      res.end('Bridge unavailable');
    });
    proxy.on('timeout', () => proxy.destroy());
    req.on('aborted', () => proxy.destroy());
    req.pipe(proxy);
  });
  server.on('upgrade', (_req, socket) => socket.destroy());
  await new Promise((resolve, reject) => {
    server.once('error', reject);
    server.listen(0, '127.0.0.1', resolve);
  });
  return {
    port: server.address().port,
    enable() { enabled = true; },
    async close() {
      enabled = false;
      server.closeAllConnections();
      await new Promise(resolve => server.close(resolve));
    },
  };
}
