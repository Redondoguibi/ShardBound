import { build } from 'esbuild';
import { mkdir, readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
export const root = fileURLToPath(new URL('../', import.meta.url));
export async function buildPlugin(paired = false) {
  const config = paired ? JSON.parse(await readFile(path.join(root,'.local/config.json'),'utf8')) : { token:'', bridgePort:30123 };
  const outdir = path.join(root,paired?'.local':'plugin');
  await mkdir(outdir,{recursive:true});
  await build({ entryPoints:[path.join(root,'plugin-src/main.mjs')], outfile:path.join(outdir,'shardbound_bridge.js'), bundle:true, format:'iife', platform:'browser', target:'chrome120', define:{ __PAIRING_TOKEN__:JSON.stringify(config.token), __BRIDGE_PORT__:String(config.bridgePort) }, logLevel:'warning' });
}
if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await buildPlugin();
