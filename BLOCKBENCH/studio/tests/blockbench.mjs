import { _electron as electron } from 'playwright-core';
import { mkdir, writeFile, readFile, access } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import assert from 'node:assert/strict';
import { validateBytes } from 'gltf-validator';
import { buildFixture } from './fixture.mjs';
import { buildSwordExample } from './sword-example.mjs';
import Ajv from 'ajv';
const contractSchema = JSON.parse(await readFile(new URL('../schemas/studio-v1.schema.json', import.meta.url)));
const contractValidator = new Ajv({ allErrors: true }).compile(contractSchema);
const validationReports = []; let companionDocuments = 0;

function glbJson(bytes) {
  const buffer = Buffer.from(bytes); assert.equal(buffer.toString('utf8', 0, 4), 'glTF');
  return JSON.parse(buffer.toString('utf8', 20, 20 + buffer.readUInt32LE(12)).trim());
}
function floats(bytes, json, index) {
  const buffer = Buffer.from(bytes), accessor = json.accessors[index], view = json.bufferViews[accessor.bufferView];
  assert.equal(accessor.componentType, 5126); const components = { SCALAR: 1, VEC3: 3, VEC4: 4 }[accessor.type];
  const base = 28 + buffer.readUInt32LE(12) + (view.byteOffset || 0) + (accessor.byteOffset || 0);
  return Array.from({ length: accessor.count }, (_, i) => Array.from({ length: components }, (_, j) => buffer.readFloatLE(base + i * (view.byteStride || components * 4) + j * 4)));
}
async function validatePackage(files, folder) {
  await mkdir(folder, { recursive: true }); const parsed = new Map();
  for (const [filename, data] of files) {
    const content = typeof data === 'string' ? data : Buffer.from(data);
    const destination = path.join(folder, filename); await mkdir(path.dirname(destination), { recursive: true }); await writeFile(destination, content);
    if (filename.endsWith('.glb')) {
      const report = await validateBytes(new Uint8Array(content), { uri: filename });
      await writeFile(destination + '.validation.json', JSON.stringify(report, null, 2));
      validationReports.push({ fixture: path.basename(folder), file: filename, errors: report.issues.numErrors, warnings: report.issues.numWarnings });
      assert.equal(report.issues.numErrors, 0, JSON.stringify(report.issues));
      const json = glbJson(content); assert.ok(json.images?.every(image => image.bufferView !== undefined) ?? true);
      assert.ok(!json.skins?.length); assert.ok((json.materials || []).every(m => m.alphaMode !== 'BLEND'));
      parsed.set(filename, json);
    } else if (filename.endsWith('.json')) { const json = JSON.parse(content); assert.equal(contractValidator(json), true, filename + ': ' + JSON.stringify(contractValidator.errors)); companionDocuments++; parsed.set(filename, json); }
  }
  return parsed;
}

const root = fileURLToPath(new URL('../', import.meta.url));
const work = path.join(root, '.local/e2e', String(Date.now())); await mkdir(work, { recursive: true });
await mkdir(path.join(work, 'profile'), { recursive: true });
// The fixture grants only its disposable export directory in Blockbench's real permission store.
await writeFile(path.join(work, 'profile/plugin_permissions.json'), JSON.stringify({ shardbound_studio: { allowed: { fs: { directories: [work] } } } }));
const executable = process.env.BLOCKBENCH_EXE || path.join(process.env.LOCALAPPDATA, 'Programs/Blockbench/Blockbench.exe');
let app; const messages = []; let phase = 'startup';
try {
  app = await electron.launch({ executablePath: executable, args: ['--userData', path.join(work, 'profile'), '--autoplay-policy=no-user-gesture-required',
    '--disable-background-timer-throttling', '--disable-renderer-backgrounding', '--disable-backgrounding-occluded-windows'], timeout: 60000 });
  const page = await app.firstWindow();
  page.setDefaultTimeout(10000);
  page.on('pageerror', error => messages.push({ phase, message: error.message, stack: error.stack }));
  page.on('console', message => { if (message.type() === 'error' && /WebGLProgram|Shader Error|shader error/i.test(message.text())) messages.push({ phase, message: message.text() }); });
  await app.evaluate(({ BrowserWindow }) => BrowserWindow.getAllWindows().forEach(w => { w.webContents.setBackgroundThrottling(false); w.hide(); }));
  await page.waitForFunction(() => window.Blockbench?.setup_successful && window.Plugin && window.Formats?.free, {}, { timeout: 60000 });
  assert.equal(await page.evaluate(() => Blockbench.version), '5.2.2');
  await new Promise(resolve => setTimeout(resolve, 1000));
  phase = 'plugin-load';
  await page.evaluate(async filename => { Dialog.open?.hide(); await new Plugin().loadFromFile({ path: filename }, false); }, path.join(root, 'dist/shardbound_studio.js'));
  await page.waitForFunction(() => !!window.ShardBoundStudio, {}, { timeout: 10000 });
  console.log('PASS plugin load in real Blockbench 5.2.2');
  const nativeMenu = async (menu, ...labels) => {
    await page.evaluate(menu => MenuBar.menus[menu].label.click(), menu);
    const entry = label => page.locator('.contextMenu').getByText(label, { exact: true }).filter({ visible: true }).locator('..');
    for (const label of labels.slice(0, -1)) await entry(label).hover();
    await entry(labels.at(-1)).click();
  };
  phase = 'native-workspace';
  const nativeWorkspace = await page.evaluate(() => {
    const s = ShardBoundStudio; s.createProject('monster', { id: 'test:monster', name: 'Monstro' });
    const body = new Group({ name: 'Corpo', origin: [0, 8, 0] }).addTo(s.find(s.state().models[0].roots[0])).init();
    new Cube({ name: 'Corpo', from: [-4, 0, -3], to: [4, 12, 3] }).addTo(body).init(); body.select(); s.refresh();
    return { panel: !!Panels.shardbound_studio, reference: s.state().context, skins: Condition(BarItems.sbstudio_skin.condition),
      bind: Condition(Group.prototype.menu.structure.find(e => e.id === 'sbstudio_bind').condition), cubes: Cube.all.length,
      tools: MenuBar.menus.tools.structure.some(e => e.id?.startsWith('sbstudio_')), duplicateNew: !!BarItems.sbstudio_newProject };
  });
  assert.equal(nativeWorkspace.panel, false); assert.ok(!nativeWorkspace.reference);
  assert.equal(nativeWorkspace.skins, false); assert.equal(nativeWorkspace.bind, false); assert.equal(nativeWorkspace.cubes, 1);
  assert.equal(nativeWorkspace.tools, false); assert.equal(nativeWorkspace.duplicateNew, false);
  assert.equal(await page.locator('.sbstudio-panel').count(), 0);
  await page.screenshot({ path: path.join(work, 'monster-native-workspace.png') });
  await page.evaluate(() => Group.first_selected.showContextMenu({ clientX: 1200, clientY: 200 }));
  await page.locator('.contextMenu').getByText('Propriedades do modelo', { exact: true }).locator('..').click();
  assert.equal(await page.locator('dialog:visible input#id').inputValue(), 'test:monster');
  await page.locator('dialog:visible .cancel_btn').click();
  await page.evaluate(() => BarItems.project_window.click());
  assert.equal(await page.locator('dialog:visible #studio_id').inputValue(), 'test:monster');
  assert.equal(await page.locator('dialog:visible #studio_projectile').isVisible(), false);
  await page.locator('dialog:visible .cancel_btn').click();
  console.log('PASS monster opens in native workspace without Studio panel, player reference or skin/equipment actions; model menu is usable');
  phase = 'player';
  const result = await page.evaluate(() => {
    const studio = ShardBoundStudio; studio.createProject('player', { id: 'test:player', variant: 'wide' });
    const animation = studio.createClip('run', 1, 'loop', 24);
    const group = Group.all.find(g => g.name === 'RightArm');
    animation.getBoneAnimator(group).createKeyframe({ x: -45, y: 0, z: 0 }, 0, 'rotation', false, false);
    animation.getBoneAnimator(group).createKeyframe({ x: 45, y: 0, z: 0 }, 0.5, 'rotation', false, false);
    Timeline.setTime(0.25); Animator.preview(false);
    return { format: Format.id, groups: Group.all.map(g => g.name), cubes: Cube.all.length, animation: Animation.selected.name,
      channels: Object.keys(animation.animators.effects.channels), saved: studio.codec.compile({ raw: true }) };
  });
  assert.equal(result.format, 'shardbound_studio'); assert.equal(result.cubes, 12); assert.equal(result.animation, 'run');
  assert.ok(result.channels.includes('shardbound_events')); assert.equal(result.saved.shardbound_studio.id, 'test:player');
  await writeFile(path.join(work, 'player.sbstudio'), JSON.stringify(result.saved, null, 2));
  await page.screenshot({ path: path.join(work, 'studio-player.png') });
  console.log('PASS player, native group timeline and editable project compile');
  phase = 'player-export';
  await page.waitForFunction(() => Texture.all.every(t => t.img.complete && t.img.naturalWidth));
  const playerExport = await page.evaluate(async () => {
    const s = ShardBoundStudio, before = { time: Timeline.time, animation: Animation.selected.uuid };
    const result = await s.exporter.compile();
    return { files: [...result.files].map(([p, v]) => [p, typeof v === 'string' ? v : Array.from(v)]), before,
      after: { time: Timeline.time, animation: Animation.selected.uuid }, exporting: s.exporting };
  });
  assert.deepEqual(playerExport.before, playerExport.after); assert.equal(playerExport.exporting, false);
  const playerPackage = await validatePackage(playerExport.files, path.join(work, 'player-package'));
  assert.ok(playerPackage.get('models/test/player.glb').animations.some(a => a.name === 'run'));
  console.log('PASS animated GLB, embedded skin, Khronos validation and editor restoration');
  phase = 'fixture';
  const fixture = await page.evaluate(buildFixture);
  assert.equal(fixture.models, 9); assert.equal(fixture.clips, 4); assert.equal(fixture.events, 6); assert.equal(fixture.arrow, 'core:arrow');
  assert.deepEqual(fixture.report.errors, []); assert.ok(fixture.preview.particles > 0); assert.equal(fixture.preview.trails, 1); assert.ok(fixture.preview.flashes > 0); assert.equal(fixture.preview.glows, 1);
  console.log('PASS all content types, full-player equipment animation, armor, arbitrary monster, native mesh and four VFX types');
  await page.screenshot({ path: path.join(work, 'studio-effects.png') });
  phase = 'timeline';
  const timeline = await page.evaluate(async () => {
    const s = ShardBoundStudio, selected = Animation.selected, sound = s.events().find(e => e.type === 'sound');
    s.updateEvent(sound.id, { volume: 0.2 }, 0.1);
    const after = s.events().map(e => ({ type: e.type, time: e.time }));
    Undo.undo(); const undone = s.events().map(e => ({ type: e.type, time: e.time })); Undo.redo();
    const matrices = [0, 0.5].map(time => { Timeline.setTime(time); Animator.preview(false); return s.find(s.state().models.find(m => m.type === 'melee').roots[0]).mesh.matrixWorld.elements.slice(); });
    const armorMatrices = [0, 0.5].map(time => { Timeline.setTime(time); Animator.preview(false); return s.find(s.state().models.find(m => m.type === 'armor').roots[0]).mesh.matrixWorld.elements.slice(); });
    Timeline.setTime(0); Animator.preview(false); s.preview.reset(); s.preview.render(selected, 0, { playing: true });
    s.preview.render(selected, 0.15, { playing: true });
    await new Promise(resolve => setTimeout(resolve, 200));
    const audio = { ...s.preview.stats }, error = s.preview.lastError; s.preview.reset();
    let invalidAudio = false; try { await s.addSound('test:bad_audio', 'data:audio/wav;base64,AAAA'); } catch { invalidAudio = true; }
    Timeline.setTime(0.25); Animator.preview(false);
    return { after, undone, matrices, armorMatrices, audio, error, invalidAudio, channels: Object.keys(selected.animators.effects.channels) };
  });
  assert.equal(timeline.after.find(e => e.type === 'sound').time, 0.1); assert.ok(timeline.after.filter(e => e.time === 0).length === 4);
  assert.equal(timeline.undone.filter(e => e.time === 0).length, 5); assert.notDeepEqual(...timeline.matrices); assert.notDeepEqual(...timeline.armorMatrices);
  assert.ok(timeline.audio.audioStarted > 0); assert.equal(timeline.error, ''); assert.equal(timeline.invalidAudio, true);
  assert.deepEqual(timeline.channels, ['shardbound_events']);
  console.log('PASS coincident event edits, native undo/redo, equipment transforms and real audio decode/playback');
  phase = 'roundtrip';
  const roundtrip = await page.evaluate(() => {
    const s = ShardBoundStudio, saved = s.codec.compile({ raw: true }), before = { metadata: JSON.stringify(s.state()), events: s.allEvents(), animations: Animation.all.map(a => a.name), elements: Outliner.elements.length };
    Project.saved = true; s.codec.load(saved, { path: '', no_file: true });
    const after = { metadata: JSON.stringify(s.state()), events: s.allEvents(), animations: Animation.all.map(a => a.name), elements: Outliner.elements.length };
    return { before, after, saved };
  });
  assert.deepEqual(roundtrip.before, roundtrip.after);
  await writeFile(path.join(work, 'studio-demo.sbstudio'), JSON.stringify(roundtrip.saved, null, 2));
  console.log('PASS editable project save/reopen with geometry, native clips, sprites, audio and timeline events');
  phase = 'fixture-export';
  const packageExport = await page.evaluate(async () => {
    Project.view_mode = 'uv'; Canvas.updateAll();
    const result = await ShardBoundStudio.exporter.compile(); if (Project.view_mode !== 'uv') throw new Error('View mode was not restored');
    Project.view_mode = 'textured'; Canvas.updateAll();
    return [...result.files].map(([p, v]) => [p, typeof v === 'string' ? v : Array.from(v)]);
  });
  const fullPackage = await validatePackage(packageExport, path.join(work, 'complete-package'));
  assert.equal([...fullPackage.keys()].filter(p => p.endsWith('.glb')).length, 9);
  const sword = fullPackage.get('models/studio_demo/sword.glb');
  assert.equal(sword.nodes.filter(n => n.mesh !== undefined).length, 4);
  assert.deepEqual(sword.animations.map(a => a.name), ['run', 'jump', 'attack', 'fire']);
  const playerChannels = fullPackage.get('studio.player-animations.json'); assert.equal(Object.keys(playerChannels.clips[0].channels).length, 6);
  assert.ok(playerChannels.clips[0].root.position.length); assert.equal(Object.keys(playerChannels.context.groups).length, 6);
  assert.equal(fullPackage.get('studio.events.json').clips.find(c => c.name === 'attack').events.length, 6);
  const monsterJson = fullPackage.get('models/studio_demo/monster.glb'), monsterBytes = packageExport.find(([p]) => p === 'models/studio_demo/monster.glb')[1];
  const jawNode = monsterJson.nodes.findIndex(n => n.name === 'Jaw'), clip = monsterJson.animations[0];
  const channel = clip.channels.find(c => c.target.node === jawNode && c.target.path === 'translation'), sampler = clip.samplers[channel.sampler];
  const times = floats(monsterBytes, monsterJson, sampler.input).flat(), positions = floats(monsterBytes, monsterJson, sampler.output);
  const boundaryIndices = times.map((t, i) => t > 0.4991 && t < 0.49917 ? i : -1).filter(i => i >= 0);
  assert.equal(boundaryIndices.length, 2); assert.equal(positions[boundaryIndices[0]][0], 0); assert.equal(positions[boundaryIndices[1]][0], 6);
  const scaleChannel = clip.channels.find(c => c.target.node === jawNode && c.target.path === 'scale'), scaleSampler = clip.samplers[scaleChannel.sampler];
  assert.equal(floats(monsterBytes, monsterJson, scaleSampler.output)[times.indexOf(0.5)][0], 2);
  console.log('PASS nine independent validated GLBs, companion contracts, sprites/audio and full reference-player channels without body duplication');
  phase = 'ui';
  await page.evaluate(() => { Modes.options.animate.select(); Animation.all.find(a => a.name === 'attack').select(); Timeline.setTime(0.25); });
  await page.locator('#panel_timeline [toolbar_item="sbstudio_newEffect"]').click();
  assert.equal(await page.locator('dialog:visible').count(), 0);
  const inspector = page.locator('.sbstudio-keyframe-properties');
  await inspector.waitFor({state:'visible'});
  await inspector.getByText('Avançado', {exact:true}).click();
  await page.locator('#sbstudio-field-id').fill('test:ui_effect');
  await page.locator('#sbstudio-field-id').dispatchEvent('change');
  await page.locator('#sbstudio-field-burst').fill('8');
  await page.locator('#sbstudio-field-burst').dispatchEvent('change');
  await inspector.getByText('Movimento', {exact:true}).click();
  await page.locator('#sbstudio-field-speed').fill('3');
  await page.locator('#sbstudio-field-speed').dispatchEvent('change');
  const edited = await page.evaluate(() => ShardBoundStudio.state().effects.find(e => e.id === 'test:ui_effect'));
  assert.equal(edited.burst,8); assert.equal(edited.speed,3);
  await page.locator('#sbstudio-field-size').fill('10');
  const live = await page.evaluate(() => {
    const s=ShardBoundStudio; s.preview.render(null,0.1);
    return { saved:s.state().effects.find(e=>e.id==='test:ui_effect').size, sizes:s.preview.root.children.filter(c=>c.isSprite).map(c=>c.scale.x), solo:s.preview.soloId };
  });
  assert.equal(live.saved,0.5); assert.equal(live.solo,'test:ui_effect'); assert.ok(live.sizes.some(v=>v>1));
  await page.locator('#sbstudio-field-size').dispatchEvent('change');
  const sizeUndo=await page.evaluate(()=>{const s=ShardBoundStudio;const get=()=>s.state().effects.find(e=>e.id==='test:ui_effect').size; const committed=get();Undo.undo();const undone=get();Undo.redo();return {committed,undone,redone:get()};});
  assert.deepEqual(sizeUndo,{committed:10,undone:0.5,redone:10});
  assert.equal(await page.evaluate(()=>ShardBoundStudio.events().filter(e=>e.resource==='test:ui_effect').length),1);
  const uiClipName=await page.evaluate(()=>Animation.selected.name);
  await inspector.getByText('Curvas ao longo da vida',{exact:true}).click();
  await inspector.getByRole('button',{name:'Adicionar ponto',exact:true}).click();
  assert.equal(await page.evaluate(()=>ShardBoundStudio.state().effects.find(e=>e.id==='test:ui_effect').curves.size.length),3);
  await inspector.evaluate(e=>{e.scrollTop=0;});
  assert.ok(await page.evaluate(()=>document.documentElement.scrollHeight<=innerHeight+2), 'Editing effects must keep the viewport visible; the whole window must not scroll');
  await page.screenshot({path:path.join(work,'studio-live-effect.png')});
  await inspector.getByText('Sprite e atlas',{exact:true}).click();
  await inspector.getByRole('button',{name:'Pintar sprite',exact:true}).click();
  await page.waitForFunction(()=>{const e=ShardBoundStudio.state().effects.find(e=>e.id==='test:ui_effect');return Texture.all.some(t=>t.uuid===e.texture&&t.img.naturalWidth===32);});
  assert.equal(await page.evaluate(()=>Mode.selected.id),'paint');
  const spriteUndo=await page.evaluate(()=>{const s=ShardBoundStudio,e=s.state().effects.find(e=>e.id==='test:ui_effect'),id=e.texture; const before=Texture.all.find(t=>t.uuid===id).getBase64();Undo.undo();const removed=!Texture.all.some(t=>t.uuid===id);Undo.redo();return{before,after:Texture.all.find(t=>t.uuid===id).getBase64(),removed};});
  assert.equal(spriteUndo.removed,true);assert.equal(spriteUndo.before,spriteUndo.after);
  const materialTarget=await page.evaluate(()=>{const t=Texture.all[0];t.showContextMenu({clientX:300,clientY:250});return t.name;});
  await page.locator('.contextMenu').getByText('Material ShardBound',{exact:true}).locator('..').click();
  assert.equal(await page.locator('dialog:visible .dialog_title').textContent(),'Material — '+materialTarget);
  await page.locator('dialog:visible .cancel_btn').click();
  await page.screenshot({path:path.join(work,'studio-sprite-paint.png')});
  await page.evaluate(name=>{Modes.options.animate.select();Animation.all.find(a=>a.name===name).select();Animation.selected.animators.effects.createKeyframe(null,0.5,'shardbound_events',true);},uiClipName);
  await page.locator('.contextMenu').getByText('Nova partícula',{exact:true}).waitFor();
  await page.keyboard.press('Escape');
  assert.equal(await page.evaluate(()=>ShardBoundStudio.events().filter(e=>e.resource==='test:ui_effect').length),1);
  const soundId=await page.evaluate(()=>ShardBoundStudio.state().sounds[0].id);
  await nativeMenu('animation','Recursos de efeitos e sons',soundId,'Inserir neste frame');
  assert.equal(await page.evaluate(()=>ShardBoundStudio.ui.inspector.vm.event.type),'sound');
  assert.equal(await page.locator('dialog:visible').count(),0);
  // Remove only this UI test's extra insertion; preserve the original fixture and its audio assertions.
  await page.evaluate(()=>ShardBoundStudio.removeEvent(ShardBoundStudio.ui.inspector.vm.eventId));
  await page.evaluate(()=>{const s=ShardBoundStudio,e=s.events().find(e=>e.resource==='test:ui_effect');Animation.selected.animators.effects.shardbound_events.find(k=>k.uuid===e.keyframe).select();});
  console.log('PASS native timeline creation, live draft preview before commit, undo/redo, inline curves, sprite painting and audio resource insertion without modal forms');
  phase = 'directory-export';
  const directories = await page.evaluate(async folder => {
    const first = await ShardBoundStudio.exporter.write(folder), second = await ShardBoundStudio.exporter.write(folder);
    return { first, second };
  }, work);
  assert.notEqual(directories.first.directory, directories.second.directory);
  console.log('PASS one-command directory export without overwriting existing packages');
  phase = 'regressions';
  const regressions = await page.evaluate(async () => {
    const s = ShardBoundStudio, player = s.state().models.find(m => m.type === 'player'), texture = Texture.all.find(t => t.uuid === player.player.texture);
    const original = texture.getBase64(), canvas = document.createElement('canvas'); canvas.width = canvas.height = 64;
    const ctx = canvas.getContext('2d'); ctx.fillStyle = '#ff0044'; ctx.fillRect(0, 0, 64, 64);
    const imported = canvas.toDataURL(); await s.importSkin(imported); const newBitmap = texture.getBase64();
    Undo.undo(); const oldBitmap = texture.getBase64(); Undo.redo(); const redoBitmap = texture.getBase64();
    let invalidSkin = false; canvas.width = 32; try { await s.importSkin(canvas.toDataURL()); } catch { invalidSkin = true; }
    const selected = Animation.selected, animator = selected.animators.effects, existing = animator.shardbound_events.find(k => k.data_points.some(p => p.studio_event.resource === 'test:ui_effect'));
    const copy = animator.addKeyframe({ ...existing.getUndoCopy(), time: 0.75 }, guid());
    const ids = s.events().filter(e => e.resource === 'test:ui_effect').map(e => e.id);
    s.updateEvent(ids[1], { volume: 0.4 }, 0.8); const copiedTimes = s.events().filter(e => e.resource === 'test:ui_effect').map(e => e.time); s.removeEvent(ids[0]);
    const originalParse = THREE.GLTFExporter.prototype.parse; let exportError = false;
    try { THREE.GLTFExporter.prototype.parse = () => { throw new Error('Injected exporter failure'); }; await s.exporter.compile(); }
    catch (error) { exportError = error.message === 'Injected exporter failure'; }
    finally { THREE.GLTFExporter.prototype.parse = originalParse; }
    let versionError = false; const invalid = s.codec.compile({ raw: true }); invalid.shardbound_studio.version = 999; const projectId = Project.uuid;
    try { s.codec.load(invalid); } catch { versionError = true; }
    s.refresh();
    return { original, newBitmap, oldBitmap, redoBitmap, invalidSkin, ids, copiedTimes, exportError, exporting: s.exporting,
      suspended: s.preview.suspended, versionError, sameProject: projectId === Project.uuid };
  });
  assert.notEqual(regressions.original, regressions.newBitmap); assert.equal(regressions.oldBitmap, regressions.original); assert.equal(regressions.redoBitmap, regressions.newBitmap);
  assert.equal(regressions.invalidSkin, true); assert.equal(new Set(regressions.ids).size, 2); assert.deepEqual(regressions.copiedTimes, [0.25, 0.8]);
  assert.equal(regressions.exportError, true); assert.equal(regressions.exporting, false); assert.equal(regressions.suspended, false);
  assert.equal(regressions.versionError, true); assert.equal(regressions.sameProject, true);
  console.log('PASS skin import/undo/redo, copied events, invalid-file rejection and export failure recovery');
  phase = 'reload';
  const beforeReload = await page.evaluate(() => ({ state: JSON.stringify(ShardBoundStudio.state()), events: ShardBoundStudio.allEvents() }));
  await page.evaluate(() => Plugins.registered.shardbound_studio.unload());
  assert.equal(await page.evaluate(() => [Group, Texture, Animation].some(type => type.prototype.menu.structure.some(entry => entry?.id?.startsWith('sbstudio_')))), false);
  const unloaded = await page.evaluate(() => Codecs.project.compile({ raw: true }));
  assert.ok(unloaded.shardbound_studio.effects.length); assert.ok(unloaded.animations.some(a => a.animators.effects?.keyframes?.length));
  await page.evaluate(async filename => { await new Plugin().loadFromFile({ path: filename }, false); }, path.join(root, 'dist/shardbound_studio.js'));
  const afterReload = await page.evaluate(() => ({ state: JSON.stringify(ShardBoundStudio.state()), events: ShardBoundStudio.allEvents() }));
  assert.deepEqual(beforeReload, afterReload);
  assert.equal(await page.evaluate(() => !!Panels.shardbound_studio), false);
  assert.deepEqual(await page.evaluate(() => [Group, Texture, Animation].map(type => type.prototype.menu.structure.filter(entry => entry?.id?.startsWith('sbstudio_')).length)), [3, 2, 3]);
  console.log('PASS unload/reload preserves serializable metadata and timeline events in open projects');
  phase = 'coexistence';
  const mtoolsFile = path.join(root, '../MTools/mesh_tools.js'), mtoolsAvailable = await access(mtoolsFile).then(() => true, () => false);
  if (mtoolsAvailable) await page.evaluate(async filename => { await new Plugin().loadFromFile({ path: filename }, false); }, mtoolsFile);
  const coexistence = await page.evaluate(() => {
    const studio = ShardBoundStudio, project = Project;
    const tools = Object.keys(BarItems).filter(id => id.includes('triangulate'));
    newProject(Formats.free); const freeChannels = Object.keys(new EffectAnimator(new Animation()).channels);
    const freeFormat = Format.id; Project.saved = true; project.select();
    return { tools, freeChannels, freeFormat, studioFormat: Format.id, nativeSkin: !!Codecs.skin, events: studio.allEvents().length };
  });
  if (mtoolsAvailable) assert.ok(coexistence.tools.length);
  assert.deepEqual(coexistence.freeChannels, ['particle', 'sound', 'timeline']); assert.equal(coexistence.freeFormat, 'free'); assert.equal(coexistence.studioFormat, 'shardbound_studio');
  console.log(mtoolsAvailable ? 'PASS MTools loads alongside Studio; native Generic Model channels and formats remain usable' : 'PASS native Generic Model coexistence; SKIP external MTools fixture (not included in standalone distribution)');
  phase = 'sword-example';
  const swordExample = await page.evaluate(buildSwordExample); assert.deepEqual(swordExample.report.errors, []);
  await writeFile(path.join(work, 'crystal-sword.sbstudio'), JSON.stringify(swordExample.saved, null, 2));
  await page.screenshot({ path: path.join(work, 'crystal-sword.png') });
  const exampleFiles = await page.evaluate(async () => [...(await ShardBoundStudio.exporter.compile()).files].map(([p, v]) => [p, typeof v === 'string' ? v : Array.from(v)]));
  await validatePackage(exampleFiles, path.join(work, 'sword-package'));
  const nativePlayback = await page.evaluate(async () => {
    const s = ShardBoundStudio, count = s.preview.stats.audioStarted; Timeline.setTime(0.4); Timeline.start();
    const start = performance.now(); while (s.preview.stats.audioStarted === count && Timeline.playing && performance.now() - start < 2000) await new Promise(resolve => setTimeout(resolve, 50));
    const time = Timeline.time; Timeline.pause();
    return { before: count, after: s.preview.stats.audioStarted, time, error: s.preview.lastError };
  });
  assert.ok(nativePlayback.after > nativePlayback.before, JSON.stringify(nativePlayback)); assert.equal(nativePlayback.error, '');
  console.log('PASS original two-folder sword example, complete package and native timeline playback with synchronized audio');
  phase = 'audio-lifecycle';
  const audioLifecycle = await page.evaluate(async () => {
    const s = ShardBoundStudio, clip = Animation.selected; s.preview.reset(); s.preview.render(clip, 0, { playing: true });
    const before = s.preview.stats.audioStarted; s.preview.render(clip, 0.9, { playing: true });
    await new Promise(resolve => setTimeout(resolve, 100)); const late = s.preview.stats.audioStarted;
    s.preview.render(clip, 0.9, { playing: true }); await new Promise(resolve => setTimeout(resolve, 50)); const repeat = s.preview.stats.audioStarted;
    const audioClip = s.createClip('audio_lifecycle', 2, 'once', 24);
    s.addEvent(audioClip, 0.1, { type: 'sound', resource: 'studio_demo:crystal_chime', action: 'burst', loop: true, instance: 'looping_burst' });
    s.addEvent(audioClip, 0.8, { type: 'sound', resource: 'studio_demo:crystal_chime', action: 'stop', instance: 'looping_burst' });
    s.preview.reset(); const count = s.preview.stats.audioStarted; Timeline.setTime(0); Timeline.start();
    const start = performance.now(); while (s.preview.stats.audioStarted === count && performance.now() - start < 1500) await new Promise(resolve => setTimeout(resolve, 20));
    const active = s.preview.activeSounds; Timeline.setTime(1); Animator.preview(true); const stopped = s.preview.activeSounds; Timeline.pause(); s.preview.reset();
    return { before, late, repeat, active, stopped, error: s.preview.lastError };
  });
  await writeFile(path.join(work, 'audio-lifecycle.json'), JSON.stringify(audioLifecycle, null, 2));
  assert.equal(audioLifecycle.late, audioLifecycle.before + 1, JSON.stringify(audioLifecycle)); assert.equal(audioLifecycle.repeat, audioLifecycle.late);
  assert.equal(audioLifecycle.active, 1, JSON.stringify(audioLifecycle)); assert.equal(audioLifecycle.stopped, 0); assert.equal(audioLifecycle.error, '');
  console.log('PASS frame-drop audio crossing without duplicate, looping burst stop and clean cancellation');
  phase = 'binding-undo';
  const bindingUndo = await page.evaluate(() => {
    const s = ShardBoundStudio, m = s.createProject('item', { id: 'test:binding' });
    const before = { groups: Group.all.length, elements: Outliner.elements.length, textures: Texture.all.length };
    s.bind(m, 'LeftArm'); const hasContext = !!s.state().context; Undo.undo();
    const after = { groups: Group.all.length, elements: Outliner.elements.length, textures: Texture.all.length }; Undo.redo();
    const redo = { context: !!s.state().context, textures: Texture.all.length };
    const clip = s.createClip('new_clip', 1, 'once', 24); Undo.undo(); Undo.redo();
    return { before, after, hasContext, redo, channels: Object.keys(Animation.all.find(a => a.name === 'new_clip').animators.effects.channels) };
  });
  assert.deepEqual(bindingUndo.before, bindingUndo.after); assert.equal(bindingUndo.hasContext, true); assert.equal(bindingUndo.redo.context, true); assert.equal(bindingUndo.redo.textures, 1);
  assert.deepEqual(bindingUndo.channels, ['shardbound_events']);
  await page.evaluate(() => Formats.shardbound_studio.new());
  await page.locator('dialog:visible .dialog_title').filter({ hasText: 'Novo projeto ShardBound Studio' }).waitFor();
  assert.equal(await page.locator('dialog:visible #variant').isVisible(), false);
  await page.locator('dialog:visible .cancel_btn').click();
  console.log('PASS binding context and clip creation undo/redo; native format entry opens Studio creation wizard');
  phase = 'native-authoring-regressions';
  const audioData = await page.evaluate(() => ModelProject.all.flatMap(p => p.shardbound_studio?.sounds || []).find(s => s.duration > 0)?.data);
  assert.ok(audioData);
  await page.evaluate(() => { ShardBoundStudio.createProject('monster', { id: 'test:native' }); Modes.options.animate.select(); });
  await page.locator('[toolbar_item="add_animation"]').filter({ visible: true }).first().click();
  assert.equal(await page.locator('dialog:visible #studio_length').isVisible(), true);
  assert.equal(await page.locator('dialog:visible #animation_properties_vue').isVisible(), false);
  await page.locator('dialog:visible #name').fill('native_attack');
  await page.locator('dialog:visible #studio_length').fill('2');
  await page.locator('dialog:visible .confirm_btn').click();
  assert.deepEqual(await page.evaluate(() => [Animation.selected.name, Animation.selected.length, Object.keys(Animation.selected.animators.effects.channels)]), ['native_attack', 2, ['shardbound_events']]);
  const locatorId = await page.evaluate(() => { const s = ShardBoundStudio, root = s.find(s.state().models[0].roots[0]); const loc = s.addLocator('Emitter', root); root.select(); Timeline.setTime(0.5); return loc.uuid; });
  await page.locator('[toolbar_item="sbstudio_newEffect"]').filter({ visible: true }).click();
  assert.equal(await page.evaluate(() => ShardBoundStudio.state().effects.length), 1);
  await page.evaluate(() => Undo.undo());
  assert.deepEqual(await page.evaluate(() => [ShardBoundStudio.state().effects.length, ShardBoundStudio.events().length]), [0, 0]);
  await page.evaluate(() => Undo.redo());
  await page.locator('.sbstudio-keyframe-properties').getByText('Evento e origem', { exact: true }).click();
  await page.locator('.sbstudio-keyframe-properties label').filter({ hasText: /^Origem/ }).locator('select').selectOption(locatorId);
  assert.equal(await page.evaluate(() => ShardBoundStudio.events()[0].target), locatorId);
  const transitions = await page.evaluate(() => {
    const s = ShardBoundStudio, id = s.state().effects[0].id; s.updateEffect(id, { kind: 'glow' });
    const valid = s.events()[0].type === 'glow'; let rejected = false;
    try { s.updateEffect(id, { kind: 'flash' }); } catch { rejected = true; }
    return { valid, rejected, kind: s.state().effects[0].kind, event: s.events()[0].type };
  });
  assert.deepEqual(transitions, { valid: true, rejected: true, kind: 'glow', event: 'glow' });
  await page.locator('.sbstudio-keyframe-properties').getByRole('button', { name: /Prévia em loop/ }).click();
  await page.getByRole('spinbutton', { name: 'Deslocamento 0', exact: true }).fill('3');
  await page.getByRole('spinbutton', { name: 'Deslocamento 0', exact: true }).dispatchEvent('change');
  assert.equal(await page.evaluate(() => { const p = ShardBoundStudio.preview; p.render(null, 0.1); return p.root.children.find(c => c.isSprite).position.x; }), 3);
  await page.getByRole('spinbutton', { name: 'Tempo do evento', exact: true }).fill('0.6');
  await page.getByRole('spinbutton', { name: 'Tempo do evento', exact: true }).dispatchEvent('change');
  assert.equal(await page.evaluate(() => ShardBoundStudio.events()[0].time), 0.6);
  const audioEdits = await page.evaluate(async data => {
    const s = ShardBoundStudio, clip = Animation.selected, file = { name: 'real.wav', content: Buffer.from(data.split(',')[1], 'base64') };
    await s.ui.audioFiles([file], clip, 0.75);
    const created = s.events().find(e => e.type === 'sound'), first = created.resource;
    const initial = s.state().sounds.length; Undo.undo(); const undone = [s.state().sounds.length, s.events().some(e => e.type === 'sound')]; Undo.redo();
    await s.ui.audioFiles([{ ...file, name: 'replacement.wav' }], clip, 1.5, s.events().find(e => e.type === 'sound').id);
    const replacement = s.events().find(e => e.type === 'sound'), replaced = replacement.resource !== first && replacement.time === 0.75;
    Undo.undo(); const restored = [s.state().sounds.length, s.events().find(e => e.type === 'sound').resource]; Undo.redo();
    return { initial, undone, replaced, restored, first, duration: s.state().sounds.at(-1).duration };
  }, audioData);
  assert.equal(audioEdits.initial, 1); assert.deepEqual(audioEdits.undone, [0, false]); assert.equal(audioEdits.replaced, true);
  assert.deepEqual(audioEdits.restored, [1, audioEdits.first]); assert.ok(audioEdits.duration > 0);
  const nativeAudio = await page.evaluate(async data => {
    const s = ShardBoundStudio, original = Blockbench.import, before = s.events().length;
    let chooser; Blockbench.import = (options, callback) => { chooser = { options, callback }; };
    try { BarItems.sbstudio_audio.click(); chooser.callback([]); } finally { Blockbench.import = original; }
    const cancelled = s.events().length === before, extensions = chooser.options.extensions;
    Timeline.setTime(1.25);
    await Filesystem.drag_handlers.sbstudio_audio.cb([{ name: 'dropped.wav', content: Buffer.from(data.split(',')[1], 'base64') }]);
    const inserted = s.events().find(e => e.resource.endsWith(':dropped'));
    return { cancelled, extensions, time: inserted?.time, duration: s.state().sounds.find(r => r.id === inserted?.resource)?.duration };
  }, audioData);
  assert.equal(nativeAudio.cancelled, true); assert.deepEqual(nativeAudio.extensions, ['wav', 'ogg', 'mp3']); assert.equal(nativeAudio.time, 1.25); assert.ok(nativeAudio.duration > 0);
  const savedPath = path.join(work, 'native-save.sbstudio');
  await page.evaluate(async destination => {
    const original = Blockbench.export; let captured;
    Blockbench.export = options => { captured = options; };
    try { BarItems.save_project_as.click(); } finally { Blockbench.export = original; }
    if (captured.extensions.join() !== 'sbstudio') throw new Error('Wrong native Save As extension');
    captured.custom_writer(captured.content, destination);
    const start = performance.now(); while (Project.save_path !== destination && performance.now() - start < 3000) await new Promise(r => setTimeout(r, 20));
    BarItems.save_project.click();
  }, savedPath);
  assert.equal(JSON.parse(await readFile(savedPath, 'utf8')).shardbound_studio.id, 'test:native');
  const uvRepair = await page.evaluate(() => {
    const s = ShardBoundStudio; s.createProject('melee', { id: 'test:legacy_uv' }); const t = Texture.all[0];
    t.uv_width = t.uv_height = 32; const old = s.codec.compile({ raw: true }); Project.saved = true; s.codec.load(old);
    const repaired = Texture.all[0].uv_width; Undo.undo(); const undone = Texture.all[0].uv_width; Undo.redo(); const redone = Texture.all[0].uv_width;
    const customized = JSON.parse(JSON.stringify(old)); customized.elements.find(e => e.faces).faces.north.uv[0] += 0.25;
    Project.saved = true; s.codec.load(customized); return { repaired, undone, redone, custom: Texture.all[0].uv_width };
  });
  assert.deepEqual(uvRepair, { repaired: 64, undone: 32, redone: 64, custom: 32 });
  console.log('PASS native animation controls, locator origin, atomic particle/audio undo, effect transitions, Save/Save As boundary and guarded legacy UV repair');
  await writeFile(path.join(work, 'errors.json'), JSON.stringify(messages, null, 2));
  const relevant = messages.filter(m => m.phase !== 'startup');
  assert.deepEqual(relevant, []);
  await writeFile(path.join(work, 'test-report.json'), JSON.stringify({ status: 'passed', completedAt: new Date().toISOString(),
    editor: 'Blockbench Desktop 5.2.2', platform: process.platform, node: process.version,
    pluginSHA256: createHash('sha256').update(await readFile(path.join(root, 'dist/shardbound_studio.js'))).digest('hex'),
    glb: validationReports, companionDocuments, pluginErrors: relevant.length, startupErrors: messages.filter(m => m.phase === 'startup').length,
    mtools: mtoolsAvailable ? 'loaded and verified coexistence' : 'external fixture unavailable' }, null, 2));
  console.log('Evidence: ' + work);
} catch (error) {
  if (app) {
    const page = await app.firstWindow().catch(() => null);
    if (page) { await page.screenshot({ path: path.join(work, 'failure.png') }).catch(() => {}); await writeFile(path.join(work, 'failure.html'), await page.content().catch(() => '')); }
  }
  throw error;
} finally {
  await writeFile(path.join(work, 'errors.json'), JSON.stringify(messages, null, 2));
  if (app) { await app.evaluate(({ app }) => app.exit(0)).catch(() => {}); await app.close().catch(() => {}); }
}
