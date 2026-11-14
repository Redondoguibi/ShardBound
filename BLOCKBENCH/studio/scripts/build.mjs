import { build } from 'esbuild';
import { mkdir } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { VERSION } from '../src/core.mjs';
const root = fileURLToPath(new URL('../', import.meta.url));
await import('./schema.mjs');
await mkdir(path.join(root, 'dist'), { recursive: true });
await build({ entryPoints: [path.join(root, 'src/main.mjs')], outfile: path.join(root, 'dist/shardbound_studio.js'),
  bundle: true, format: 'iife', platform: 'browser', target: 'chrome120', sourcemap: false,
  banner: { js: `// ShardBound Studio ${VERSION} — Blockbench 5.2.2. Source: BLOCKBENCH/studio/src/` } });
console.log('Built dist/shardbound_studio.js');
