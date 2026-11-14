import express from 'express';
import http from 'node:http';
import { randomUUID } from 'node:crypto';
import { WebSocketServer, WebSocket } from 'ws';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { StreamableHTTPServerTransport } from '@modelcontextprotocol/sdk/server/streamableHttp.js';
import { mcpAuthRouter } from '@modelcontextprotocol/sdk/server/auth/router.js';
import { requireBearerAuth } from '@modelcontextprotocol/sdk/server/auth/middleware/bearerAuth.js';
import { BridgeOAuth } from './auth.mjs';
import { loadConfig, equalSecret } from './config.mjs';
import { createMcp } from './server-tools.mjs';
import { tools } from '../shared/tools.mjs';

export async function startDaemon(config) {
  const app = express(),
    httpServer = http.createServer(app);
  const sockets = new WebSocketServer({
    noServer: true,
    maxPayload: 64 * 1024 * 1024,
  });
  let plugin = null,
    tail = Promise.resolve(),
    closed = false;
  const pending = new Map();
  const close = async () => {
    if (closed) return;
    closed = true;
    for (const ws of sockets.clients) ws.terminate();
    sockets.close();
    httpServer.closeAllConnections();
    await new Promise((resolve) => httpServer.close(resolve));
  };
  const internalNames = new Set(['__open_project_data', '__studio_import_sound', '__studio_import_skin', '__studio_export_chunk', '__studio_export_release']);
  const localAuth = (req, res, next) =>
    equalSecret(req.headers.authorization, `Bearer ${config.token}`)
      ? next()
      : res.sendStatus(401);
  app.disable('x-powered-by');
  app.use((req, res, next) => {
    // Both local browser requests and DNS rebinding must pass an explicit origin/host check.
    const allowed = [
      '127.0.0.1',
      'localhost',
      ...(config.publicUrl ? [new URL(config.publicUrl).hostname] : []),
    ];
    let hostname;
    try {
      hostname = new URL(`http://${req.headers.host}`).hostname;
    } catch {}
    if (!allowed.includes(hostname)) return res.sendStatus(403);
    if (
      req.headers.origin &&
      ![
        ...(config.publicUrl ? [new URL(config.publicUrl).origin] : []),
        `http://127.0.0.1:${config.bridgePort}`,
        `http://localhost:${config.bridgePort}`,
      ].includes(req.headers.origin)
    )
      return res.sendStatus(403);
    next();
  });
  app.use(express.json({ limit: '48mb' }));
  app.get('/health', localAuth, (req, res) =>
    res.json({
      service: 'shardbound-blockbench',
      version: '0.5.0',
      connected: !!plugin,
      publicUrl: config.publicUrl || null,
    }),
  );
  app.post('/shutdown', localAuth, (req, res) => {
    res.json({ stopped: true });
    setTimeout(() => void close(), 50);
  });
  const dispatch = (name, args, deadline) =>
    new Promise((resolve, reject) => {
      const remaining = deadline - Date.now();
      if (remaining <= 0)
        return reject(
          new Error(
            'Request expired while queued; it was not sent to Blockbench.',
          ),
        );
      if (!plugin || plugin.readyState !== WebSocket.OPEN)
        return reject(
          new Error(
            'Blockbench disconnected. Load .local/shardbound_bridge.js and keep Blockbench open.',
          ),
        );
      const id = randomUUID();
      const timer = setTimeout(() => {
        pending.delete(id);
        reject(
          new Error(
            'Blockbench timed out. Inspect the project before retrying; an operation may already have executed.',
          ),
        );
      }, remaining);
      pending.set(id, { resolve, reject, timer });
      plugin.send(JSON.stringify({ type: 'call', id, name, args, deadline }));
    });
  const serialize = (fn) => {
    const next = tail.then(fn);
    tail = next.catch(() => {});
    return next;
  };
  const call = (name, args) => {
    const deadline = Date.now() + (name === 'studio_export' ? 180000 : 45000);
    if (internalNames.has(name))
      return serialize(() => dispatch(name, args, deadline));
    if (!Object.hasOwn(tools, name))
      return Promise.reject(new Error('Unknown tool'));
    const parsed = tools[name].schema.parse(args);
    if (name === 'status' && !plugin)
      return Promise.resolve({
        connected: false,
        service: 'shardbound-blockbench',
        message: 'Load the paired Blockbench plugin.',
      });
    if (name === 'list_model_files')
      return Promise.reject(
        new Error('list_model_files is handled by the MCP server.'),
      );
    return serialize(() => dispatch(name, parsed, deadline));
  };
  app.post('/rpc', localAuth, async (req, res) => {
    try {
      res.json({ result: await call(req.body.name, req.body.args) });
    } catch (error) {
      res.status(400).json({ error: error.message });
    }
  });
  let auth;
  if (config.publicUrl) {
    const url = new URL(config.publicUrl);
    if (
      url.protocol !== 'https:' ||
      url.pathname !== '/' ||
      url.search ||
      url.hash ||
      url.username ||
      url.password
    )
      throw new Error(
        'publicUrl must be an HTTPS origin without path or credentials.',
      );
    auth = new BridgeOAuth(config);
    app.use(
      mcpAuthRouter({
        provider: auth,
        issuerUrl: url,
        resourceServerUrl: new URL('/mcp', url),
        scopesSupported: ['blockbench'],
        resourceName: 'ShardBound Blockbench',
      }),
    );
    app.post(
      '/approve',
      express.urlencoded({ extended: false, limit: '4kb' }),
      (req, res) => auth.approve(req, res),
    );
    app.use(
      '/mcp',
      requireBearerAuth({
        verifier: auth,
        requiredScopes: ['blockbench'],
        resourceMetadataUrl: new URL(
          '/.well-known/oauth-protected-resource/mcp',
          url,
        ).href,
      }),
    );
  } else app.use('/mcp', localAuth);
  app.post('/mcp', async (req, res) => {
    const server = createMcp(call, config, !!auth);
    const transport = new StreamableHTTPServerTransport({
      sessionIdGenerator: undefined,
      enableJsonResponse: true,
    });
    res.on('close', () => {
      transport.close();
      server.close();
    });
    try {
      await server.connect(transport);
      await transport.handleRequest(req, res, req.body);
    } catch {
      if (!res.headersSent)
        res.status(500).json({
          jsonrpc: '2.0',
          error: { code: -32603, message: 'MCP request failed' },
          id: null,
        });
    }
  });
  app.all('/mcp', (req, res) => res.status(405).set('Allow', 'POST').end());
  httpServer.on('upgrade', (req, socket, head) => {
    // Public HTTP tunnels never receive access to the plugin's transport.
    if (
      req.url !== '/blockbench' ||
      !['127.0.0.1', '::1', '::ffff:127.0.0.1'].includes(
        req.socket.remoteAddress,
      ) ||
      !/^127\.0\.0\.1:\d+$/.test(req.headers.host || '') ||
      req.headers['x-forwarded-for'] ||
      !['null', 'file://', undefined].includes(req.headers.origin)
    ) {
      socket.destroy();
      return;
    }
    sockets.handleUpgrade(req, socket, head, (ws) =>
      sockets.emit('connection', ws),
    );
  });
  sockets.on('connection', (ws) => {
    let authenticated = false;
    const timer = setTimeout(() => ws.close(1008, 'Pairing timeout'), 5000);
    ws.on('message', (bytes) => {
      let message;
      try {
        message = JSON.parse(bytes.toString());
      } catch {
        ws.close(1008);
        return;
      }
      if (!authenticated) {
        if (
          message.type !== 'hello' ||
          !equalSecret(message.token, config.token) ||
          plugin
        ) {
          ws.close(
            1008,
            'Invalid pairing or another Blockbench instance is connected',
          );
          return;
        }
        clearTimeout(timer);
        authenticated = true;
        plugin = ws;
        ws.send(JSON.stringify({ type: 'ready', version: '0.5.0' }));
        return;
      }
      const request = pending.get(message.id);
      if (!request) return;
      clearTimeout(request.timer);
      pending.delete(message.id);
      message.error
        ? request.reject(new Error(message.error))
        : request.resolve(message.result);
    });
    ws.on('error', () => {});
    ws.on('close', () => {
      clearTimeout(timer);
      if (plugin === ws) {
        plugin = null;
        for (const request of pending.values()) {
          clearTimeout(request.timer);
          request.reject(
            new Error(
              'Blockbench disconnected during operation. Inspect before retrying.',
            ),
          );
        }
        pending.clear();
      }
    });
  });
  await new Promise((resolve, reject) => {
    httpServer.once('error', reject);
    httpServer.listen(config.bridgePort, '127.0.0.1', resolve);
  });
  return { httpServer, call, close };
}
if (
  process.argv[1] &&
  path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)
) {
  try {
    const daemon = await startDaemon(loadConfig());
    console.error('ShardBound Bridge ready (loopback).');
    for (const signal of ['SIGINT', 'SIGTERM'])
      process.on(signal, async () => {
        await daemon.close();
        process.exit(0);
      });
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
