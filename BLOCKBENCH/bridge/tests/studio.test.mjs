import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile, readFile, symlink } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { createMcp } from '../mcp/server-tools.mjs';
import { tools } from '../shared/tools.mjs';
import { studioTools } from '../shared/studio-tools.mjs';

test('all Studio mutation schemas require project/revision and reject scripts and unsafe resources', () => {
  for (const [name, def] of Object.entries(studioTools)) {
    if (def.read || name === 'studio_create_project') continue;
    assert.ok(def.schema.shape.project_id); assert.equal(def.schema.shape.expected_revision.isOptional(), false);
  }
  assert.equal(tools.studio_effect.schema.safeParse({ project_id:'p',expected_revision:0,action:'add',id:'test:../escape',kind:'particle' }).success,false);
  assert.equal(tools.studio_effect.schema.safeParse({ project_id:'p',expected_revision:0,action:'add',id:'test:safe',kind:'particle',values:{script:'eval()'} }).success,false);
  assert.equal(tools.studio_sound.schema.safeParse({project_id:'p',expected_revision:0,action:'import',id:'test:sound',path:'assets/../secrets.wav'}).success,false);
  assert.equal(tools.studio_keyframes.schema.safeParse({project_id:'p',expected_revision:0,clip:'a',keyframes:[{group:'g',channel:'rotation',time:0,value:['query.anim_time',0,0]}]}).success,false);
  assert.equal(tools.save_project.schema.safeParse({project_id:'p',path:'assets/model.sbstudio'}).success,true);
});

async function harness(fn) {
  const projectRoot = await mkdtemp(path.join(os.tmpdir(),'shardbound-studio-unit-')), assets = path.join(projectRoot,'assets'); await mkdir(assets);
  const calls = [], files = new Map([['studio.manifest.json',Buffer.from('{"schema":"test"}')],['models/test.glb',Buffer.alloc(1024*1024+17,33)]]);
  const server = createMcp(async (name,args) => {
    calls.push(name);
    if(name==='studio_export') return {project_id:'p',revision:1,session:'session',id:'test:pkg',warnings:[],files:[...files].map(([name,data])=>({name,bytes:data.length}))};
    if(name==='__studio_export_chunk') {const bytes=files.get(args.file);return {total:bytes.length,data:bytes.subarray(args.offset,args.offset+1024*1024).toString('base64')};}
    if(name==='__studio_import_sound') return {sound:{id:args.id,bytes:Buffer.from(args.data.split(',')[1],'base64').length}};
    return {};
  },{projectRoot,assetRoots:[assets],outputs:path.join(projectRoot,'outputs')});
  const client=new Client({name:'studio-unit',version:'1'}),[ct,st]=InMemoryTransport.createLinkedPair(); await server.connect(st); await client.connect(ct);
  try {await fn({client,projectRoot,assets,calls,files});} finally {await client.close();await server.close();}
}
test('Studio package streams exact bytes in chunks, reserves unique destinations and releases transfer', () => harness(async ({client,files,calls}) => {
  const args={project_id:'p',expected_revision:0};
  const response=await client.callTool({name:'studio_export',arguments:args});assert.notEqual(response.isError,true);
  const result=JSON.parse(response.content[0].text);
  for(const [file,data] of files)assert.deepEqual(await readFile(path.join(result.directory,file)),data);
  const second=JSON.parse((await client.callTool({name:'studio_export',arguments:args})).content[0].text);assert.notEqual(result.directory,second.directory);
  assert.equal(calls.filter(n=>n==='__studio_export_chunk').length,6);assert.equal(calls.filter(n=>n==='__studio_export_release').length,2);
}));
test('Studio package rejects traversal and still releases the export session', () => harness(async ({client,files,calls}) => {
  files.clear();files.set('../outside.js',Buffer.from('bad'));
  const response=await client.callTool({name:'studio_export',arguments:{project_id:'p',expected_revision:0}});assert.equal(response.isError,true);
  assert.ok(calls.includes('__studio_export_release'));assert.ok(!calls.includes('__studio_export_chunk'));
}));
test('audio imports preserve real file bytes and reject symlink escapes from assets', () => harness(async ({client,assets,projectRoot,calls}) => {
  await writeFile(path.join(assets,'audio.wav'),Buffer.from('unit bytes'));
  const args={project_id:'p',expected_revision:0,action:'import',id:'test:audio',path:'assets/audio.wav'};
  const response=await client.callTool({name:'studio_sound',arguments:args});assert.notEqual(response.isError,true);assert.equal(JSON.parse(response.content[0].text).sound.bytes,10);
  const privateDir=path.join(projectRoot,'private');await mkdir(privateDir);await writeFile(path.join(privateDir,'audio.wav'),'private');
  await symlink(privateDir,path.join(assets,'escaped'),process.platform==='win32'?'junction':'dir');
  const rejected=await client.callTool({name:'studio_sound',arguments:{...args,path:'assets/escaped/audio.wav'}});assert.equal(rejected.isError,true);
  assert.equal(calls.filter(n=>n==='__studio_import_sound').length,1);
}));
