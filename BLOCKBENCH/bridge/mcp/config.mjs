import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { timingSafeEqual } from 'node:crypto';
export const root=fileURLToPath(new URL('../',import.meta.url));
export function loadConfig() {
  const filename=process.env.BLOCKBENCH_BRIDGE_CONFIG || path.join(root,'.local/config.json');
  if(!fs.existsSync(filename)) throw new Error('Run npm run setup before starting the bridge.');
  const config=JSON.parse(fs.readFileSync(filename,'utf8'));
  if(!/^[\w-]{32,}$/.test(config.token)||!Number.isInteger(config.bridgePort)||config.bridgePort<1024||config.bridgePort>65535) throw new Error('Invalid bridge configuration.');
  return {...config,filename,outputs:path.resolve(config.outputs||path.join(root,'outputs'))};
}
export function equalSecret(a,b) {
  const x=Buffer.from(a||''),y=Buffer.from(b||'');
  return x.length===y.length && timingSafeEqual(x,y);
}
