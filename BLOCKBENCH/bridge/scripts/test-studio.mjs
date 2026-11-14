import { _electron as electron } from 'playwright-core';
import { build } from 'esbuild';
import { cp, mkdir, writeFile, readFile, readdir } from 'node:fs/promises';
import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import { randomBytes, createHash } from 'node:crypto';
import path from 'node:path';
import assert from 'node:assert/strict';
import { Client } from '@modelcontextprotocol/sdk/client/index.js';
import { InMemoryTransport } from '@modelcontextprotocol/sdk/inMemory.js';
import { root } from '../mcp/config.mjs';
import { createMcp } from '../mcp/server-tools.mjs';
import { startDaemon } from '../mcp/daemon.mjs';
import { tools } from '../shared/tools.mjs';

const work = path.join(root, '.local/studio-e2e', String(Date.now())), assets = path.join(work, 'assets');
await mkdir(assets, { recursive: true });
const studioFile = path.resolve(root, '../studio/dist/shardbound_studio.js');
const token = randomBytes(32).toString('base64url'), config = { bridgePort: 0, token, projectRoot: work, assetRoots: [assets], outputs: path.join(work, 'outputs') };
const daemon = await startDaemon(config), messages = [], executed = new Set(); let app, client, phase = 'startup';
const pluginFile = path.join(work, 'shardbound_bridge.js');
await build({ entryPoints: [path.join(root, 'plugin-src/main.mjs')], outfile: pluginFile, bundle: true, format: 'iife', platform: 'browser', target: 'chrome120',
  define: { __PAIRING_TOKEN__: JSON.stringify(token), __BRIDGE_PORT__: String(daemon.httpServer.address().port) }, logLevel: 'warning' });
try {
  app = await electron.launch({ executablePath: process.env.BLOCKBENCH_EXE || path.join(process.env.LOCALAPPDATA, 'Programs/Blockbench/Blockbench.exe'),
    args: ['--userData', path.join(work, 'profile'), '--autoplay-policy=no-user-gesture-required', '--disable-background-timer-throttling', '--disable-renderer-backgrounding'], timeout: 60000 });
  const page = await app.firstWindow(); page.on('pageerror', e => messages.push({ phase, message: e.message, stack: e.stack }));
  await app.evaluate(({ BrowserWindow }) => BrowserWindow.getAllWindows().forEach(w => { w.webContents.setBackgroundThrottling(false); w.hide(); }));
  await page.waitForFunction(() => window.Blockbench?.setup_successful && window.Formats?.free, {}, { timeout: 60000 });
  assert.equal(await page.evaluate(() => Blockbench.version), '5.2.2');
  await new Promise(resolve => setTimeout(resolve, 1000)); phase = 'plugin-load';
  await page.evaluate(async files => { Dialog.open?.hide(); for (const file of files) await new Plugin().loadFromFile({ path: file }, false); },
    [studioFile, path.resolve(root, '../MTools/mesh_tools.js'), pluginFile]);
  await page.waitForFunction(() => !!window.ShardBoundStudio);
  const server = createMcp(daemon.call, config), [ct, st] = InMemoryTransport.createLinkedPair();
  client = new Client({ name: 'studio-real-blockbench-test', version: '1' }); await server.connect(st); await client.connect(ct);
  const list = await client.listTools(); assert.equal(list.tools.length, Object.keys(tools).length);
  async function invoke(name, args = {}, guard = true) {
    phase = name; executed.add(name);
    if (guard && tools[name].schema.shape.project_id) { const status = await invoke('status', {}, false); args = { project_id: status.project_id, expected_revision: status.revision, ...args }; }
    const result = await client.callTool({ name, arguments: args }, undefined, { timeout: 240000 });
    if (result.isError) throw new Error(`${name}: ${result.content[0].text}`);
    if (result.content[0].type === 'image') return result;
    return JSON.parse(result.content[0].text);
  }
  for (let i = 0; i < 50; i++) { if ((await invoke('status', {}, false)).connected) break; await new Promise(resolve => setTimeout(resolve, 200)); }
  assert.equal((await invoke('status')).studio.api_version, 1);
  const initial = await invoke('studio_create_project', { type: 'melee', id: 'test:sword', name: 'Studio MCP sword' });
  const rootId = initial.model.roots[0];
  await invoke('read_project', { geometry:true });
  const tx = await invoke('put_texture', { name: 'Sword atlas', width: 32, height: 32, color: '#2288ffff' });
  await invoke('paint_texture', { texture: tx.texture_id, rectangles: [{ x: 0, y: 0, width: 8, height: 8, color: '#ff3300ff' }] });
  assert.deepEqual(await page.evaluate(id => [...Texture.all.find(t => t.uuid === id).ctx.getImageData(0, 0, 1, 1).data], tx.texture_id), [255, 51, 0, 255]);
  await invoke('undo_redo', { direction: 'undo' }); await new Promise(resolve => setTimeout(resolve, 80));
  assert.deepEqual(await page.evaluate(id => [...Texture.all.find(t => t.uuid === id).ctx.getImageData(0, 0, 1, 1).data], tx.texture_id), [34, 136, 255, 255]);
  await invoke('undo_redo', { direction: 'redo' });
  await invoke('create_elements', { elements: [{ type: 'cube', parent: rootId, name: 'Lâmina', from: [-.5, 0, -.5], to: [.5, 12, .5], faces: Object.fromEntries(['north','south','east','west','up','down'].map(key => [key, { texture: tx.texture_id, uv: [0, 0, 8, 8] }])) }] });
  await invoke('studio_material', { texture: tx.texture_id, alphaMode: 'MASK', alphaCutoff: .25 });
  await invoke('studio_project_settings', { sampleRate: 24, unitsPerMeter: 16, projectileVisual: 'core:arrow' });
  await invoke('studio_model', { action: 'bind', model: 'test:sword', part: 'RightArm' });
  const loc = await invoke('studio_locator', { action: 'add', name: 'tip', parent: rootId, position: [0, 12, 0] });
  await invoke('studio_locator', { action: 'update', id: loc.locator.id, position: [0, 11, 0] });
  const createdClip = await invoke('studio_clip', { action: 'add', name: 'attack', length: 1, fps: 24, loop: 'once' }), clip = createdClip.clip.id;
  let studio = await invoke('studio_read'); const arm = studio.state.context.groups.RightArm;
  const keyResult = await invoke('studio_keyframes', { clip, keyframes: [{ group: arm, channel: 'rotation', time: 0, value: [0, 0, 0], interpolation: 'step' }, { group: arm, channel: 'rotation', time: .5, value: [-90, 0, 0] }, { group: rootId, channel: 'scale', time: 0, value: [1, 1, 1] }] });
  await invoke('studio_keyframes', { clip, keyframes: [{ id: keyResult.keyframes[1].id, group: arm, channel: 'rotation', time: .5, value: [-70, 0, 0], interpolation: 'bezier', bezier_left_time: [-.1,-.1,-.1], bezier_right_time: [.1,.1,.1] }] });
  await invoke('studio_clip', { action: 'update', clip, length: 1.2, fps: 30 });
  const temporaryKeys = await invoke('studio_keyframes', { clip, keyframes:[{group:rootId,channel:'position',time:.9,value:[1,2,3],interpolation:'catmullrom'}] });
  await invoke('studio_keyframes', { clip, remove:[temporaryKeys.keyframes[0].id] });
  const tempClip = await invoke('studio_clip', {action:'add',name:'temporary',length:1});
  await invoke('studio_clip', {action:'remove',clip:tempClip.clip.id}); await invoke('studio_clip', {action:'select',clip});
  const tempLoc = await invoke('studio_locator', {action:'add',name:'unused',parent:rootId}); await invoke('studio_locator',{action:'remove',id:tempLoc.locator.id});
  await invoke('studio_effect', { action: 'add', kind: 'particle', id: 'test:sparks', values: { burst: 8, rate: 0, texture: tx.texture_id, blend: 'additive', curves: { color: [{ age: 0, value: '#ffffff' }, { age: 1, value: '#ff9900' }] } } });
  await invoke('studio_effect', { action: 'update', id: 'test:sparks', values: { gravity: [0,-3,0] } });
  await invoke('studio_effect', {action:'add',kind:'glow',id:'test:unused'}); await invoke('studio_effect',{action:'remove',id:'test:unused'});
  for (const kind of ['trail','flash','glow']) await invoke('studio_effect', { action: 'add', kind, id: `test:${kind}`, values: {} });
  const event = await invoke('studio_event', { action: 'add', clip, time: .1, values: { type: 'particle', resource: 'test:sparks', target: loc.locator.id, follow: true } });
  for (const kind of ['trail','flash','glow']) await invoke('studio_event', { action: 'add', clip, time: .1, values: { type: kind, resource: `test:${kind}`, target: rootId } });
  await invoke('studio_event', { action: 'update', id: event.event.id, time: .2, values: { follow: false } });
  const wav = Buffer.alloc(44 + 8000); wav.write('RIFF'); wav.writeUInt32LE(wav.length - 8, 4); wav.write('WAVEfmt ',8); wav.writeUInt32LE(16,16); wav.writeUInt16LE(1,20); wav.writeUInt16LE(1,22); wav.writeUInt32LE(8000,24); wav.writeUInt32LE(16000,28); wav.writeUInt16LE(2,32); wav.writeUInt16LE(16,34); wav.write('data',36); wav.writeUInt32LE(8000,40);
  for (let i=0;i<4000;i++) wav.writeInt16LE(Math.round(Math.sin(i/8000*2*Math.PI*440)*5000*(1-i/4000)),44+i*2);
  await writeFile(path.join(assets,'chime.wav'), wav);
  await invoke('studio_sound', { action: 'import', id: 'test:chime', path: 'assets/chime.wav' });
  await invoke('studio_sound', {action:'import',id:'test:unused',path:'assets/chime.wav'}); await invoke('studio_sound',{action:'remove',id:'test:unused'});
  // Hold a real decoder result while simulating a separate native user edit.
  await page.evaluate(() => { const proto=AudioContext.prototype; window.studioDecoderOriginal=proto.decodeAudioData;
    proto.decodeAudioData=function(...args) { return window.studioDecoderOriginal.apply(this,args).then(decoded=>new Promise(resolve=>{window.releaseStudioDecode=()=>resolve(decoded);})); }; });
  const raceStatus=await invoke('status');
  const racingImport=client.callTool({name:'studio_sound',arguments:{project_id:raceStatus.project_id,expected_revision:raceStatus.revision,action:'import',id:'test:race',path:'assets/chime.wav'}});
  await page.waitForFunction(()=>typeof window.releaseStudioDecode==='function');
  await page.evaluate(()=>{ShardBoundStudio.edit('Simulated native user edit',()=>{ShardBoundStudio.state().sampleRate=30;});releaseStudioDecode();});
  const raced=await racingImport; assert.equal(raced.isError,true);assert.match(raced.content[0].text,/revision changed/);
  await page.evaluate(()=>{AudioContext.prototype.decodeAudioData=window.studioDecoderOriginal;delete window.studioDecoderOriginal;delete window.releaseStudioDecode;});
  assert.equal((await invoke('studio_read')).state.sounds.some(s=>s.id==='test:race'),false);
  await invoke('studio_event', { action: 'add', clip, time: .1, values: { type: 'sound', resource: 'test:chime', volume: .2 } });
  const extraEvent = await invoke('studio_event',{action:'add',clip,time:.8,values:{type:'particle',resource:'test:sparks'}}); await invoke('studio_event',{action:'remove',id:extraEvent.event.id});
  await invoke('studio_timeline', { action: 'seek', clip, time: .25 });
  await invoke('capture_view', { width: 640, height: 480 });
  const beforeAudio = await page.evaluate(() => ShardBoundStudio.preview.stats.audioStarted);
  await invoke('studio_timeline', { action: 'play', clip, time: 0 });
  await page.waitForFunction(before => ShardBoundStudio.preview.stats.audioStarted > before, beforeAudio, { timeout: 10000 });
  await invoke('studio_timeline', { action: 'pause' });
  studio = await invoke('studio_read', { clip }); assert.equal(studio.keyframes.length, 3); assert.equal(studio.events.length, 5); assert.ok(!JSON.stringify(studio).includes('data:audio'));
  assert.deepEqual((await invoke('studio_validate')).validation.errors, []);
  const bad = await client.callTool({ name: 'studio_effect', arguments: { project_id: studio.project_id, expected_revision: studio.revision - 1, action: 'remove', id: 'test:sparks' } }); assert.equal(bad.isError, true);
  const referenced = await client.callTool({ name: 'studio_locator', arguments: { project_id: studio.project_id, expected_revision: studio.revision, action: 'remove', id: loc.locator.id } }); assert.equal(referenced.isError, true);
  console.log('PASS real Studio MCP modeling, bitmap Undo, P/R/S keyframes, four VFX, attachments, decoded/played sound, guards and validation');
  await invoke('save_project', { path: 'assets/sword.sbstudio' });
  const saved = JSON.parse(await readFile(path.join(assets,'sword.sbstudio'))); assert.equal(saved.animations.length, 1); assert.equal(saved.shardbound_studio.effects.length, 4);
  await invoke('open_project_file', { path: 'assets/sword.sbstudio' }); assert.equal((await invoke('studio_read')).events.length, 5);
  const editor = path.resolve(root, '../../tools/editor-studio.mjs');
  const editorConfig = path.join(work, 'editor-bridge.json');
  const editorPlan = path.join(work, 'editor-clips.json');
  await writeFile(editorConfig, JSON.stringify({ ...config, bridgePort: daemon.httpServer.address().port }));
  await writeFile(editorPlan, JSON.stringify({ namespace: 'test', select: 'idle', clips: [{ name: 'idle', length: 1, loop: 'loop' }, { name: 'attack', length: 1.2, loop: 'once' }] }));
  const runEditor = (operation, ...args) => promisify(execFile)(process.execPath, [editor, path.join(root, 'mcp/server.mjs'), operation, ...args], {
    windowsHide: true, timeout: 210000, env: { ...process.env, BLOCKBENCH_BRIDGE_CONFIG: editorConfig },
  });
  const editorClips = JSON.parse((await runEditor('clips', editorPlan)).stdout);
  assert.deepEqual(editorClips.clips.sort(), ['attack', 'idle']);
  assert.equal(await page.evaluate(() => Animation.selected?.name), 'idle');
  const editorExport = JSON.parse((await runEditor('export')).stdout);
  assert.equal(JSON.parse(await readFile(editorExport.manifest)).project, 'test:sword');
  const installed = path.join(work, 'editor-mod', 'studio');
  await mkdir(path.dirname(installed), { recursive: true });
  await cp(path.dirname(editorExport.manifest), installed, { recursive: true });
  const syncPlan = path.join(work, 'editor-sync.json');
  await writeFile(syncPlan, JSON.stringify({ project: 'test:sword', manifest: path.join(installed, 'studio.manifest.json') }));
  const editorSync = JSON.parse((await runEditor('sync', syncPlan)).stdout);
  assert.equal(editorSync.project, 'test:sword');
  assert.equal(JSON.parse(await readFile(path.join(installed, 'studio.manifest.json'))).project, 'test:sword');
  console.log('PASS editor bridge created and selected idle and exported a live Studio project');
  const pkg = await invoke('studio_export'); assert.ok(pkg.files.some(f => f.name.endsWith('.glb'))); assert.ok(pkg.files.some(f => f.name.endsWith('.wav')));
  const secondPkg = await invoke('studio_export'); assert.notEqual(pkg.directory, secondPkg.directory);
  const manifest = JSON.parse(await readFile(path.join(pkg.directory,'studio.manifest.json'))); assert.equal(manifest.models.length,1);
  const { validateBytes } = await import('../../studio/node_modules/gltf-validator/index.js');
  const validations = [];
  for (const file of pkg.files) { const bytes = await readFile(path.join(pkg.directory,file.name)); assert.equal(bytes.length,file.bytes); if(file.name.endsWith('.glb')) { const report=await validateBytes(new Uint8Array(bytes)); validations.push({ file:file.name, errors:report.issues.numErrors, warnings:report.issues.numWarnings }); assert.equal(report.issues.numErrors,0); } }
  assert.equal((await invoke('list_model_files', { directory: 'assets' })).count,1);
  console.log('PASS editable Studio roundtrip and complete chunked package export twice without overwrite; Khronos GLB validation');
  await invoke('studio_create_project', { type:'player', id:'test:player', name:'MCP Player', variant:'slim' });
  const skin = await page.evaluate(() => Texture.all[0].getDataURL()); await writeFile(path.join(assets,'skin.png'), Buffer.from(skin.split(',')[1],'base64'));
  await invoke('studio_skin', { action:'import', path:'assets/skin.png' }); await invoke('studio_skin', { action:'layers', visible:false });
  assert.equal(await page.evaluate(() => ShardBoundStudio.state().models[0].player.layers.every(id=> !ShardBoundStudio.find(id).visibility)),true);
  await invoke('studio_model', { action:'add', type:'effect', id:'test:temporary' }); await invoke('studio_model', { action:'update', model:'test:temporary', id:'test:temp', name:'Temporary' }); await invoke('studio_model', { action:'select', model:'test:temp' }); await invoke('studio_model', { action:'remove', model:'test:temp' });
  await invoke('create_project', { name:'Generic coexistence' });
  const generated = await invoke('mtools_run', { action:'polyhedron', parameters:{select:'Icosahedron',radius:3,detail:1} });
  assert.ok(generated.selection.length); await invoke('select_elements', { ids:generated.selection, all_components:true, mode:'face' }); await invoke('mtools_run', { action:'triangulate' });
  assert.equal((await invoke('status')).format, 'free');
  await page.evaluate(() => ModelProject.all.find(p=>p.format.id==='shardbound_studio').select());
  await invoke('mtools_list');
  await invoke('studio_timeline', { action:'pause', clip: (await invoke('studio_read')).clips[0].id });
  const studioMesh = await invoke('mtools_run', { action:'polyhedron', parameters:{select:'Tetrahedron',radius:2,detail:1} });
  assert.ok(studioMesh.selection.length);
  await invoke('update_elements',{updates:studioMesh.selection.map(id=>({id,parent:rootId}))});
  const meshRead = await invoke('read_project',{geometry:true,limit:200});
  for(const id of studioMesh.selection) { const mesh = meshRead.elements.find(e=>e.id===id); await invoke('edit_mesh',{id,faces:Object.fromEntries(Object.entries(mesh.faces).map(([key,face])=>[key,{...face,texture:tx.texture_id}]))}); }
  const parentGroup = await invoke('create_elements',{elements:[{type:'group',name:'A',parent:rootId}]});
  const childGroup = await invoke('create_elements',{elements:[{type:'group',name:'B',parent:parentGroup.created[0].id}]});
  const last = await invoke('status');
  const cyclic = await client.callTool({name:'update_elements',arguments:{project_id:last.project_id,expected_revision:last.revision,updates:[{id:parentGroup.created[0].id,parent:childGroup.created[0].id}]}});assert.equal(cyclic.isError,true);
  await invoke('delete_elements',{ids:[childGroup.created[0].id]}); await invoke('delete_elements',{ids:[parentGroup.created[0].id]});
  assert.deepEqual((await invoke('studio_validate')).validation.errors,[]);
  console.log('PASS slim skin import/layers, model management, Generic compatibility and real MTools in Studio');
  const runtimeErrors = messages.filter(e=>e.phase!=='startup'); assert.deepEqual(runtimeErrors,[]);
  await page.screenshot({ path:path.join(work,'integration.png') });
  const report={ status:'passed', completedAt:new Date().toISOString(), blockbench:'5.2.2', bridge:'0.5.0', studio:await page.evaluate(() => ShardBoundStudio.version), discovered_tools:list.tools.length, tested_tools:[...executed].sort(),
    studio_sha256:createHash('sha256').update(await readFile(studioFile)).digest('hex'), bridge_source_sha256:createHash('sha256').update(await readFile(path.join(root,'plugin-src/studio.mjs'))).digest('hex'), glb:validations, errors:runtimeErrors, native_startup_errors:messages.filter(e=>e.phase==='startup') };
  await writeFile(path.join(work,'test-report.json'),JSON.stringify(report,null,2));
  console.log('PASS Studio integration; evidence: '+work);
  await client.close(); await server.close();
} catch(error) { await writeFile(path.join(work,'failure.json'),JSON.stringify({ phase,error:error.stack,messages },null,2)); throw error; }
finally { await client?.close().catch(()=>{}); if(app) { await app.evaluate(({app})=>app.exit(0)).catch(()=>{}); } await daemon.close(); }
