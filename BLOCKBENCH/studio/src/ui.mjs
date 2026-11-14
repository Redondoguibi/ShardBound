import { TYPES, BODY_PARTS, assertId, defaultEffect } from './core.mjs';
import { createEffectInspector } from './effect-inspector.mjs';

export function createUI(g, studio, exporter) {
  const actions = [], dialogs = new Set(), attachments = [], restores = [], toolbarItems = [];
  let busy = false, disposed = false, inspector;
  const error = e => { g.Blockbench.showMessageBox({ title: 'ShardBound Studio', icon: 'error', message: String(e.message || e) }); return false; };
  const run = fn => { try { const r = fn(); return r?.catch ? r.catch(error) : r; } catch (e) { return error(e); } };
  const input = (label, value) => ({ type: 'input', label, value });
  const number = (label, value, min, max, step = 0.1) => ({ type: 'number', label, value, min, max, step });
  const select = (label, value, options) => ({ type: 'select', label, value, options });
  function dialog(title, form, confirm) {
    const d = new g.Dialog({ id: 'sbstudio_' + g.guid(), title, width: 520, form, onConfirm: r => run(() => confirm(r)),
      onButton() { g.setTimeout(() => { dialogs.delete(d); d.delete(); }, 0); } }); dialogs.add(d); d.show(); return d;
  }
  function model() {
    if (!studio.isStudio()) return null;
    let node = g.Group.first_selected || g.Outliner.selected[0];
    while (node && node !== 'root') { const m = studio.state().models.find(m => m.roots.includes(node.uuid)); if (m) return m; node = node.parent; }
    return studio.state().models[0];
  }
  const hasPlayer = () => studio.isStudio() && studio.state().models.some(m => m.player);
  const canBind = () => studio.isStudio() && ['item', 'melee', 'ranged', 'armor'].includes(model()?.type);
  const inAnimation = () => studio.isStudio() && !!g.Modes.animate && !!g.Animation.selected;
  function uniqueId(prefix) { const s = studio.state(); let n = 1, id = prefix; while ([...s.models, ...s.effects, ...s.sounds].some(r => r.id === id)) id = prefix + '_' + n++; return id; }
  function newProject() {
    dialog('Novo projeto ShardBound Studio', { type: select('Conteúdo', 'monster', TYPES), name: input('Nome', 'Meu modelo'),
      variant: { ...select('Braços do player', 'wide', { wide: 'Padrão (4 px)', slim: 'Fino (3 px)' }), condition: r => ['player', 'melee', 'ranged', 'armor'].includes(r.type) },
      id: input('ID do recurso', 'custom:model') }, r => { studio.createProject(r.type, r); studio.selectModel(studio.state().models[0]); });
  }
  function addModel() { dialog('Modelo independente', { type: select('Conteúdo', 'item', TYPES), name: input('Nome', 'Modelo relacionado'),
    id: input('ID do recurso', uniqueId(studio.state().id + '_modelo')) }, r => studio.selectModel(studio.addModel(r.type, r))); }
  function modelProperties() { const m = model(); if (m) dialog('Propriedades do modelo', { name: input('Nome', m.name), id: input('ID do recurso', m.id) }, r => studio.renameModel(m, r.id, r.name)); }
  function bind() { const m = model(); if (m) dialog('Encaixe do equipamento', {
    root: select('Pasta', m.roots[0], Object.fromEntries(m.roots.map(id => [id, studio.find(id)?.name || id]))),
    part: select('Parte do player', 'RightArm', Object.fromEntries(BODY_PARTS.map(p => [p, p]))) }, r => studio.bind(m, r.part, r.root)); }
  function material(texture = g.Texture.selected) {
    if (!texture) return; const old = studio.state().materials[texture.uuid] || { alphaMode: 'MASK', alphaCutoff: 0.5, doubleSided: true };
    dialog('Material — ' + texture.name, { alphaMode: select('Transparência', old.alphaMode, { OPAQUE: 'Opaco', MASK: 'Recorte alpha' }),
      alphaCutoff: number('Corte alpha', old.alphaCutoff, 0, 1, 0.01), doubleSided: { type: 'checkbox', label: 'Renderizar os dois lados', value: old.doubleSided } },
    r => studio.edit('Studio: material', () => { studio.state().materials[texture.uuid] = r; }));
  }
  function importSkin() { const project = g.Project;
    g.Blockbench.import({ resource_id: 'sbstudio_skin', type: 'Skin PNG 64×64', extensions: ['png'], readtype: 'binary' }, files => run(async () => {
      if (!files[0] || disposed) return; if (g.Project !== project) throw new Error('O projeto mudou.');
      await studio.importSkin('data:image/png;base64,' + g.Buffer.from(files[0].content).toString('base64'), () => { if (disposed) throw new Error('O plugin foi desativado durante a importação.'); });
    }));
  }
  const origin = () => g.Locator.selected[0]?.uuid || g.Group.first_selected?.uuid || model()?.roots[0] || '';
  function addResource(animation, time, resource, newDefinition = null) {
    const definition = newDefinition || studio.state().effects.find(e => e.id === resource);
    const added = studio.addEvent(animation, time, { type: definition?.kind || 'sound', resource, target: definition ? origin() : '', action: 'burst' }, newDefinition);
    g.Timeline.setTime(time); added.keyframe.select(); inspector.vm.eventId = `${added.keyframe.uuid}:${added.event.id}`; inspector.refresh(true); return added;
  }
  function newEffect(animation = g.Animation.selected, time = g.Timeline.time) {
    if (!animation || time < 0 || time > animation.length) throw new Error('Posicione o cursor dentro de uma animação.');
    const id = uniqueId(studio.state().id.split(':')[0] + ':particula');
    const r = addResource(animation, time, id, defaultEffect('particle', id)); inspector.vm.startPreview(); return r;
  }
  async function audioFiles(files, animation, time, replaceId = null) {
    const project = g.Project;
    for (const file of files) {
      if (disposed || g.Project !== project) throw new Error('O projeto mudou durante a escolha do áudio.');
      const ext = file.name.split('.').at(-1).toLowerCase(), mime = { wav: 'audio/wav', ogg: 'audio/ogg', mp3: 'audio/mpeg' }[ext];
      if (!mime) throw new Error('Escolha WAV, OGG ou MP3.');
      const base = file.name.toLowerCase().replace(/\.[^.]+$/, '').replace(/[^a-z0-9_/-]+/g, '_') || 'som';
      const id = uniqueId(studio.state().id.split(':')[0] + ':' + base), data = 'data:' + mime + ';base64,' + g.Buffer.from(file.content).toString('base64');
      await studio.addSound(id, data, file.name, () => { if (disposed || g.Project !== project || !g.Animation.all.includes(animation)) throw new Error('A animação foi fechada durante a importação.'); },
        { animation, time: Math.min(time, animation.length), eventId: replaceId });
      const e = studio.events(animation).findLast(e => e.resource === id); if (e) { inspector.vm.eventId = e.id; inspector.refresh(true); }
      replaceId = null;
    }
  }
  function addAudio(animation = g.Animation.selected, time = g.Timeline.time, replaceId = null) {
    if (!animation) throw new Error('Selecione uma animação.'); const project = g.Project;
    g.Blockbench.import({ resource_id: 'sbstudio_sound', type: 'Som neste frame', extensions: ['wav', 'ogg', 'mp3'], readtype: 'binary', multiple: true },
      files => run(() => { if (disposed || g.Project !== project) throw new Error('O projeto mudou.'); return audioFiles(files, animation, time, replaceId); }));
  }
  function chooseResource(animation = g.Animation.selected, time = g.Timeline.time, event) {
    new g.Menu('sbstudio_frame_resources', [
      { name: 'Nova partícula', icon: 'auto_awesome', click: () => run(() => newEffect(animation, time)) },
      { name: 'Escolher áudio…', icon: 'audio_file', click: () => run(() => addAudio(animation, time)) }, '_',
      ...[...studio.state().effects, ...studio.state().sounds].map(r => ({ name: r.id, icon: r.kind ? 'auto_awesome' : 'audio_file', click: () => run(() => addResource(animation, time, r.id)) })),
    ]).open(event || g.Toolbars.timeline.node);
  }
  function paintSprite(id) {
    const e = studio.state().effects.find(e => e.id === id); if (!e) return;
    let texture = g.Texture.all.find(t => t.uuid === e.texture);
    if (!texture) { const textures = []; studio.edit('Studio: sprite para pintar', () => {
      const canvas = g.document.createElement('canvas'); canvas.width = canvas.height = 32;
      texture = new g.Texture({ name: id.split(':')[1], width: 32, height: 32, uv_width: 32, uv_height: 32 }); texture.canvas.width = texture.canvas.height = 32;
      texture.fromDataURL(canvas.toDataURL()).add(); textures.push(texture); e.texture = texture.uuid;
    }, { textures, bitmap: true }); }
    texture.select(); studio.preview.stopSolo(); g.Modes.options.paint.select();
  }
  function validate() { const report = exporter.validate(); validation.update(); g.Validator.validate(); g.Validator.openDialog(); return report; }
  async function exportFolder() {
    if (busy) return; if (!exporter.validate().valid) return validate();
    const directory = g.Blockbench.pickDirectory({ resource_id: 'sbstudio_export', title: 'Exportar pacote ShardBound' }); if (!directory) return;
    busy = true; try { const r = await exporter.write(directory); g.Blockbench.showQuickMessage(`Pacote exportado: ${r.directory}`, 6000); } finally { busy = false; }
  }
  const handlers = { newProject, addModel, modelProperties, bind, material, skin: importSkin, newEffect, audio: addAudio, event: chooseResource,
    sprite: () => paintSprite(inspector.vm.definition?.id), validate, exportFolder, save: () => studio.codec.export() };
  function action(id, name, icon, click, condition = studio.isStudio) {
    const a = new g.Action('sbstudio_' + id, { name, icon, condition, category: 'animation', click: event => run(() => click(event)) }); actions.push(a); return a;
  }
  function attach(menu, entry, path) { menu.addAction(entry, path); attachments.push([menu, (path ? path + '.' : '') + (typeof entry === 'string' ? entry : entry.id)]); }
  function wrap(id, fn) { const a = g.BarItems[id], original = a.click; const replacement = function(...args) { return studio.isStudio() ? run(() => fn(original.bind(this), ...args)) : original.apply(this, args); };
    a.click = replacement; restores.push(() => { if (a.click === replacement) a.click = original; }); }
  attach(g.MenuBar.menus.file, action('exportFolder', 'Pacote ShardBound (.glb + recursos)', 'folder_zip', exportFolder, () => studio.isStudio() && !busy), 'export');
  // The format already appears in File > New. Native Save / Save As own editable files.
  wrap('save_project', () => { if (!g.Project.save_path) return studio.codec.export(); const p = g.Project;
    g.Blockbench.writeFile(p.save_path, { content: studio.codec.compile() }, () => { p.saved = true; }); });
  wrap('save_project_as', () => studio.codec.export()); wrap('export_over', () => g.BarItems.save_project.click());
  wrap('add_animation', () => { let name = 'idle', n = 1; while (g.Animation.all.some(a => a.name === name)) name = 'animation_' + n++;
    studio.createClip(name, 1, 'loop', 24).propertiesDialog(); });
  const originalAnimationProperties = g.Animation.prototype.propertiesDialog;
  const animationProperties = function(...args) {
    const result = originalAnimationProperties.apply(this, args); if (!studio.isStudio()) return result;
    const animation = this, d = g.Dialog.open, values = d.getFormResult();
    // The native dialog contains Molang clocks/blending for other runtimes. Studio exposes its actual contract.
    d.content_vue?.$el?.setAttribute('hidden', '');
    d.form_config.override.condition = false;
    d.form_config.snapping.min = 1; d.form_config.snapping.max = 120;
    d.form_config.studio_length = number('Duração (segundos)', animation.length, 0.01, 1000);
    d.form.form_config = d.form_config; d.form.buildForm(); d.form.setValues(values, false); d.form.updateValues({ cause: 'setup' });
    const confirm = d.onConfirm; d.onConfirm = function(r, event) { return run(() => {
      if (!(r.studio_length > 0 && r.studio_length <= 1000) || !(r.snapping >= 1 && r.snapping <= 120)) throw new Error('Use duração até 1000 s e 1–120 FPS.');
      if (Object.values(animation.animators).some(a => a.keyframes.some(k => k.time > r.studio_length))) throw new Error('A duração deve incluir todos os keyframes.');
      if (!r.name.trim() || g.Animation.all.some(a => a !== animation && a.name === r.name)) throw new Error('Nome de animação vazio ou repetido.');
      const result = confirm.call(this, r, event); if (result === false) return false;
      if (animation.length !== r.studio_length) studio.edit('Studio: duração da animação', () => animation.setLength(r.studio_length), { animations: [animation] });
    }); }; return result;
  };
  g.Animation.prototype.propertiesDialog = animationProperties;
  restores.push(() => { if (g.Animation.prototype.propertiesDialog === animationProperties) g.Animation.prototype.propertiesDialog = originalAnimationProperties; });
  wrap('save_project_incremental', () => { const p = g.Project, path = p.save_path.replace(/\.(sbstudio|bbmodel)$/i, '_' + Date.now() + '.sbstudio');
    if (path === p.save_path) throw new Error('Salve o projeto Studio antes de criar uma versão incremental.');
    g.Blockbench.writeFile(path, { content: studio.codec.compile() }, () => { p.save_path = path; p.saved = true; }); });
  wrap('project_window', original => {
    original(); const d = g.Dialog.open, values = d.getFormResult(), s = studio.state(), project = g.Project;
    Object.assign(d.form_config, { studio_id: input('ID ShardBound', s.id), studio_units: number('Unidades por metro', s.unitsPerMeter, 0.001, 100000),
      studio_samples: number('Amostragem da exportação (FPS)', s.sampleRate, 1, 240, 1),
      studio_projectile: { ...input('Projétil visual compartilhado', s.projectileVisual || ''), condition: () => s.models.some(m => m.type === 'ranged') } });
    d.form.form_config = d.form_config; d.form.buildForm(); d.form.setValues(values, false); d.form.updateValues({ cause: 'setup' });
    const confirm = d.onConfirm; d.onConfirm = function(r, event) { return run(() => {
      if (g.Project !== project) throw new Error('O projeto mudou.'); assertId(r.studio_id); if (r.studio_projectile) assertId(r.studio_projectile);
      if (!(r.studio_units > 0) || !(r.studio_samples >= 1 && r.studio_samples <= 240)) throw new Error('Escala ou FPS inválido.');
      const result = confirm.call(this, r, event); if (result === false) return false;
      studio.edit('Studio: propriedades', () => Object.assign(studio.state(), { id: r.studio_id, unitsPerMeter: r.studio_units, sampleRate: r.studio_samples, projectileVisual: r.studio_projectile || null }));
    }); };
  });
  const particle = action('newEffect', 'Partícula neste frame', 'auto_awesome', () => newEffect(), inAnimation);
  const audio = action('audio', 'Som neste frame…', 'audio_file', () => addAudio(), inAnimation);
  const resources = action('resources', 'Recurso neste frame', 'add', event => chooseResource(g.Animation.selected, g.Timeline.time, event), inAnimation);
  for (const a of [particle, audio, resources]) { g.Toolbars.timeline.add(a, 6); toolbarItems.push([g.Toolbars.timeline, a]); attach(g.MenuBar.menus.animation, a); attach(g.Animation.prototype.menu, a); attach(g.Timeline.menu, a); }
  const modelAction = action('addModel', 'Modelo independente', 'library_add', addModel); attach(g.Panels.outliner.menu, modelAction); attach(g.BarItems.add_element.side_menu, modelAction);
  attach(g.Group.prototype.menu, { id: 'sbstudio_model_properties', name: 'Propriedades do modelo', icon: 'settings', condition: studio.isStudio, click: () => run(modelProperties) });
  attach(g.Group.prototype.menu, { id: 'sbstudio_bind', name: 'Encaixe do equipamento', icon: 'pan_tool', condition: canBind, click: () => run(bind) });
  attach(g.Texture.prototype.menu, { id: 'sbstudio_texture_material', name: 'Material ShardBound', icon: 'texture', condition: studio.isStudio, click: t => run(() => material(t)) });
  const skin = action('skin', 'Substituir skin 64×64…', 'image', importSkin, hasPlayer); attach(g.Texture.prototype.menu, skin); attach(g.MenuBar.menus.file, skin, 'import');
  const layer = action('layer', 'Mostrar / ocultar camada externa', 'layers', () => { const elements = studio.state().models.flatMap(m => m.player?.layers || []).map(studio.find).filter(Boolean); studio.setSkinLayers(!elements.every(e => e.visibility)); }, hasPlayer); attach(g.Group.prototype.menu, layer);
  const validation = new g.ValidatorCheck('sbstudio_contract', { condition: studio.isStudio, update_triggers: ['finished_edit', 'select_project', 'select_animation'],
    run() { const r = exporter.validate(); r.errors.forEach(message => this.fail({ message })); r.warnings.forEach(message => this.warn({ message })); } });
  const drop = g.Blockbench.addDragHandler('sbstudio_audio', { extensions: ['wav', 'ogg', 'mp3'], readtype: 'binary', propagate: true, element: '#panel_timeline', condition: inAnimation },
    files => run(() => audioFiles(files, g.Animation.selected, g.Timeline.time)));
  inspector = createEffectInspector(g, studio, { paintSprite, addAudio, chooseResource });
  const refresh = () => inspector.refresh(); studio.onRefresh = refresh; studio.onRequestEvent = (animation, time) => run(() => chooseResource(animation, time));
  attach(g.MenuBar.menus.animation, { id: 'sbstudio_library', name: 'Recursos de efeitos e sons', icon: 'library_books', condition: inAnimation, children: () =>
    [...studio.state().effects, ...studio.state().sounds].map(r => ({ id: 'sbstudio_resource_' + r.id, name: r.id, icon: r.kind ? 'auto_awesome' : 'audio_file', children: [
      { name: 'Inserir neste frame', icon: 'add', click: () => run(() => addResource(g.Animation.selected, g.Timeline.time, r.id)) },
      { name: 'Remover recurso', icon: 'delete', click: () => run(() => r.kind ? studio.removeEffect(r.id) : studio.removeSound(r.id)) },
    ] })) });
  const boneLabel = g.Panels.bone?.handle?.querySelector('span'), originalLabel = boneLabel?.textContent;
  const vocabulary = () => { if (boneLabel) boneLabel.textContent = studio.isStudio() ? 'Pasta / pivô' : originalLabel; };
  const mode = () => { if (!g.Modes.animate) { studio.preview.clearDraft(); studio.preview.stopSolo(); } };
  let shownPreviewError = '';
  const previewError = () => { const message = studio.preview.lastError; if (message && message !== shownPreviewError) g.Blockbench.showQuickMessage(message, 6000); shownPreviewError = message; };
  g.Blockbench.on('select_project', vocabulary); g.Blockbench.on('select_mode', mode); g.Blockbench.on('render_frame', previewError); vocabulary(); refresh();
  return { handlers, inspector, audioFiles, refresh, dispose() {
    disposed = true; studio.onRefresh = studio.onRequestEvent = null; inspector.dispose(); drop.delete(); validation.delete();
    g.Blockbench.removeListener('select_project', vocabulary); g.Blockbench.removeListener('select_mode', mode); if (boneLabel) boneLabel.textContent = originalLabel;
    g.Blockbench.removeListener('render_frame', previewError);
    for (const d of dialogs) { if (g.Dialog.stack.includes(d)) d.hide(); d.delete(); } dialogs.clear();
    restores.reverse().forEach(fn => fn()); attachments.forEach(([menu, id]) => menu.removeAction(id)); toolbarItems.forEach(([bar, a]) => bar.remove(a)); actions.forEach(a => a.delete());
  } };
}
