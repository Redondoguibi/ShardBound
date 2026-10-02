import { randomBytes, createHash } from 'node:crypto';
import {
  InvalidGrantError,
  InvalidTokenError,
  InvalidClientMetadataError,
  InvalidScopeError,
  InvalidTargetError,
} from '@modelcontextprotocol/sdk/server/auth/errors.js';
import { equalSecret } from './config.mjs';

const random = () => randomBytes(32).toString('base64url');
const hash = (s) => createHash('sha256').update(s).digest('hex');
const escape = (s) =>
  String(s).replace(
    /[&<>"']/g,
    (c) =>
      ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[
        c
      ],
  );

// Single-user OAuth provider. SDK handles endpoint schemas, client authentication,
// rate limiting and S256 PKCE verification. Grants are kept in memory: restart revokes all.
export class BridgeOAuth {
  constructor(config) {
    if (
      typeof config.loginSecret !== 'string' ||
      config.loginSecret.length < 32
    )
      throw new Error(
        'Remote mode requires a loginSecret of at least 32 characters.',
      );
    this.config = config;
    this.clients = new Map();
    this.pending = new Map();
    this.codes = new Map();
    this.tokens = new Map();
    this.attempts = [];
    this.resource = new URL('/mcp', config.publicUrl).href;
    this.clientsStore = {
      getClient: (id) => this.clients.get(id),
      registerClient: async (client) => {
        if (this.clients.size >= 200)
          throw new InvalidClientMetadataError(
            'Registration capacity reached. Restart the bridge.',
          );
        if (
          !client.redirect_uris?.length ||
          client.redirect_uris.some((uri) => {
            try {
              const u = new URL(uri);
              return (
                u.protocol !== 'https:' &&
                !(
                  u.protocol === 'http:' &&
                  ['127.0.0.1', 'localhost', '[::1]'].includes(u.hostname)
                )
              );
            } catch {
              return true;
            }
          })
        )
          throw new InvalidClientMetadataError(
            'Use HTTPS or a loopback callback.',
          );
        const record = {
          ...client,
          client_id: random(),
          client_id_issued_at: Math.floor(Date.now() / 1000),
        };
        this.clients.set(record.client_id, record);
        return record;
      },
    };
  }
  clean() {
    const now = Date.now();
    for (const map of [this.pending, this.codes, this.tokens])
      for (const [k, v] of map) if (v.expires < now) map.delete(k);
  }
  checkResource(resource) {
    if (resource && resource.href !== this.resource)
      throw new InvalidTargetError('Unexpected MCP resource.');
  }
  checkScopes(scopes) {
    if (scopes?.some((s) => s !== 'blockbench'))
      throw new InvalidScopeError('Only blockbench scope is supported.');
  }
  async authorize(client, params, res) {
    this.clean();
    this.checkScopes(params.scopes);
    this.checkResource(params.resource);
    if (this.pending.size >= 200)
      throw new InvalidGrantError('Too many pending authorizations.');
    const nonce = random();
    this.pending.set(nonce, { client, params, expires: Date.now() + 300000 });
    res.set({
      'Content-Security-Policy':
        "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'",
      'Cache-Control': 'no-store',
      'Referrer-Policy': 'no-referrer',
    });
    res.send(
      `<!doctype html><html lang="pt-BR"><meta charset="utf-8"><title>Conectar ao Blockbench</title><style>body{font:17px system-ui;max-width:640px;margin:8vh auto;padding:24px;background:#171b21;color:#e7ecf1}input,button{font:inherit;padding:12px;margin-top:16px;width:100%;box-sizing:border-box}code{word-break:break-all}</style><h1>Conectar ao seu Blockbench</h1><p>Cliente: <b>${escape(client.client_name || client.client_id)}</b></p><p>Esta conexão permite ler e modificar o projeto aberto, executar os comandos adaptados do MTools e exportar modelos.</p><p>Destino de retorno: <code>${escape(params.redirectUri)}</code></p><form method="post" action="/approve"><input type="hidden" name="nonce" value="${nonce}"><label>Senha local de conexão (arquivo .local/chatgpt-password.txt)<input type="password" name="password" autocomplete="current-password" required></label><button type="submit">Autorizar conexão</button></form></html>`,
    );
  }
  approve(req, res) {
    this.clean();
    this.attempts = this.attempts.filter((t) => t > Date.now() - 60000);
    if (this.attempts.length >= 10)
      return res.status(429).send('Aguarde um minuto.');
    this.attempts.push(Date.now());
    if (
      req.headers.origin &&
      req.headers.origin !== new URL(this.config.publicUrl).origin
    )
      return res.status(403).send('Origin inválida.');
    const pending = this.pending.get(req.body?.nonce);
    if (!pending || !equalSecret(req.body?.password, this.config.loginSecret))
      return res.status(403).send('Senha inválida ou solicitação expirada.');
    this.pending.delete(req.body.nonce);
    const code = random();
    this.codes.set(hash(code), { ...pending, expires: Date.now() + 60000 });
    const callback = new URL(pending.params.redirectUri);
    callback.searchParams.set('code', code);
    if (pending.params.state)
      callback.searchParams.set('state', pending.params.state);
    res.set('Cache-Control', 'no-store').redirect(callback.href);
  }
  getCode(client, code) {
    this.clean();
    const record = this.codes.get(hash(code));
    if (!record || record.client.client_id !== client.client_id)
      throw new InvalidGrantError('Invalid or expired authorization code.');
    return record;
  }
  async challengeForAuthorizationCode(client, code) {
    return this.getCode(client, code).params.codeChallenge;
  }
  issue(clientId, scopes) {
    this.clean();
    if (this.tokens.size >= 1000)
      throw new InvalidGrantError('Token capacity reached.');
    const access = random(),
      refresh = random(),
      grant = random();
    const record = {
      clientId,
      scopes: scopes?.length ? scopes : ['blockbench'],
      resource: this.resource,
      grant,
    };
    this.tokens.set(hash(access), {
      ...record,
      kind: 'access',
      expires: Date.now() + 3600000,
    });
    this.tokens.set(hash(refresh), {
      ...record,
      kind: 'refresh',
      expires: Date.now() + 7 * 86400000,
    });
    return {
      access_token: access,
      token_type: 'Bearer',
      expires_in: 3600,
      refresh_token: refresh,
      scope: record.scopes.join(' '),
    };
  }
  async exchangeAuthorizationCode(
    client,
    code,
    verifier,
    redirectUri,
    resource,
  ) {
    const record = this.getCode(client, code);
    this.checkResource(resource);
    if (redirectUri !== record.params.redirectUri)
      throw new InvalidGrantError('Redirect URI mismatch.');
    this.codes.delete(hash(code));
    return this.issue(client.client_id, record.params.scopes);
  }
  async exchangeRefreshToken(client, token, scopes, resource) {
    this.clean();
    this.checkResource(resource);
    this.checkScopes(scopes);
    const record = this.tokens.get(hash(token));
    if (
      !record ||
      record.kind !== 'refresh' ||
      record.clientId !== client.client_id
    )
      throw new InvalidGrantError('Invalid refresh token.');
    for (const [k, v] of this.tokens)
      if (v.grant === record.grant) this.tokens.delete(k);
    return this.issue(client.client_id, scopes || record.scopes);
  }
  async verifyAccessToken(token) {
    this.clean();
    const record = this.tokens.get(hash(token));
    if (!record || record.kind !== 'access')
      throw new InvalidTokenError('Invalid or expired access token.');
    return {
      token,
      clientId: record.clientId,
      scopes: record.scopes,
      expiresAt: Math.floor(record.expires / 1000),
      resource: new URL(record.resource),
    };
  }
  async revokeToken(client, { token }) {
    const record = this.tokens.get(hash(token));
    if (record?.clientId === client.client_id)
      for (const [k, v] of this.tokens)
        if (v.grant === record.grant) this.tokens.delete(k);
  }
}
