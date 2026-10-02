import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { timingSafeEqual } from 'node:crypto';
export const root = fileURLToPath(new URL('../', import.meta.url));
export function loadConfig() {
  const filename =
    process.env.BLOCKBENCH_BRIDGE_CONFIG ||
    path.join(root, '.local/config.json');
  if (!fs.existsSync(filename))
    throw new Error('Run npm run setup before starting the bridge.');
  const config = JSON.parse(fs.readFileSync(filename, 'utf8'));
  validateConfig(config);
  return {
    ...config,
    filename,
    outputs: path.resolve(config.outputs || path.join(root, 'outputs')),
  };
}
export function equalSecret(a, b) {
  if (typeof a !== 'string' || typeof b !== 'string' || !a.length || !b.length)
    return false;
  const x = Buffer.from(a),
    y = Buffer.from(b);
  return x.length === y.length && timingSafeEqual(x, y);
}
export function validateConfig(config) {
  if (
    typeof config.token !== 'string' ||
    !/^[\w-]{32,}$/.test(config.token) ||
    !Number.isInteger(config.bridgePort) ||
    config.bridgePort < 1024 ||
    config.bridgePort > 65535
  )
    throw new Error('Invalid bridge configuration.');
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
    if (
      typeof config.loginSecret !== 'string' ||
      config.loginSecret.length < 32
    )
      throw new Error(
        'Remote mode requires a loginSecret of at least 32 characters. Run setup.',
      );
  }
}
