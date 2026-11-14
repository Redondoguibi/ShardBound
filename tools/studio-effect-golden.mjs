// Cross-language reference values from the actual Blockbench preview implementation.
import fs from 'node:fs';
import {particles, trailTimes} from '../BLOCKBENCH/studio/src/effects.mjs';
const data=JSON.parse(fs.readFileSync(new URL('../examples/studio/studio_demo/sword/studio.effects.json',import.meta.url)));
const samples=[];
for(const shape of ['point','box','sphere','cone']) for(const drag of [0,1.7]) {
  const definition={...data.definitions.find(e=>e.kind==='particle'),shape,drag,rate:12,burst:7,seed:4294967290,shapeSize:[3,5,7],direction:[1,2,-3],speedSpread:2,frames:8,columns:4,rows:2,angularVelocity:125,curves:{size:[{age:0,value:.1},{age:.4,value:2},{age:1,value:0}],color:[{age:0,value:'#a020ff'},{age:1,value:'#10ffaa'}]}};
  const interval={definition,start:.25,end:1.1,continuous:true};
  for(const time of [.1,.25,.5,.9,1.3,1.8])samples.push({definition,start:interval.start,end:interval.end,continuous:true,time,particles:particles(interval,time)});
}
const definition=data.definitions.find(e=>e.kind==='trail');
const trails=[.1,.3,.75,.9,1.3].map(time=>({time,values:trailTimes({definition,start:.25,end:.75},time)}));
fs.writeFileSync(new URL('../tests/fixtures/studio/effects-golden.json',import.meta.url),JSON.stringify({samples,trailDefinition:definition,trails},null,2)+'\n');
