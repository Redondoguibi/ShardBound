(() => {
const VERSION = '1.3.0';
const FORMAT = 'shardbound_studio';
const CHANNEL = 'shardbound_events';
const SCHEMA_VERSION = 1;
const MAX_PARTICLES = 4096;
const TYPES = Object.freeze({ player: 'Player / skin', item: 'Item', melee: 'Arma corpo a corpo', ranged: 'Arma de longa distância',
  block: 'Bloco de cenário', armor: 'Armadura', monster: 'Monstro', effect: 'Partículas / VFX' });
const EFFECT_TYPES = Object.freeze({ particle: 'Partículas', trail: 'Rastro', flash: 'Flash', glow: 'Brilho / luz' });
const BODY_PARTS = ['Head', 'Body', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg'];
const clone = value => structuredClone(value);
function resourceId(value) {
  return typeof value === 'string' && /^[a-z0-9_.-]+:[a-z0-9_./-]+$/.test(value) &&
    value.replace(':', '/').split('/').every(part => part && part !== '.' && part !== '..');
}
function assertId(value) { if (!resourceId(value)) throw new Error('Use um ID namespace:path, em minúsculas, sem . ou .. como segmentos.'); return value; }
function assetPath(id, extension, directory) { assertId(id); const [namespace, name] = id.split(':'); return `${directory}/${namespace}/${name}.${extension}`; }
function projectState(type = 'monster', id = 'custom:model') {
  if (!TYPES[type]) throw new Error('Tipo de projeto desconhecido.');
  assertId(id);
  return { schema: 'shardbound.studio.project', version: SCHEMA_VERSION, contentType: type, id, unitsPerMeter: 16,
    sampleRate: 60, skinVariant: 'wide', models: [], effects: [], sounds: [], materials: {}, context: null,
    projectileVisual: type === 'ranged' ? 'core:arrow' : null, nextEventOrder: 0,
    renderContract: { version: 1, filtering: 'nearest', colorSpace: 'srgb', particleLighting: 'unlit',
      transparency: 'alpha-or-additive', gravitySpace: 'emitter-local', particleMotion: 'analytic-linear-drag',
      spriteOrientation: 'camera-facing', trail: 'world-space-ribbon', glow: 'additive-sprite-and-point-light' } };
}
function checkState(state) {
  if (!state || state.schema !== 'shardbound.studio.project' || state.version !== SCHEMA_VERSION)
    throw new Error('Projeto Studio ausente ou versão não suportada. O arquivo original não foi alterado.');
  if (!TYPES[state.contentType]) throw new Error('Tipo de conteúdo não suportado.');
  assertId(state.id);
  if (!(state.unitsPerMeter > 0 && Number.isFinite(state.unitsPerMeter))) throw new Error('Escala de exportação inválida.');
  if (!Number.isFinite(state.sampleRate) || state.sampleRate < 1 || state.sampleRate > 240) throw new Error('Amostragem deve ficar entre 1 e 240 FPS.');
  for (const field of ['models', 'effects', 'sounds']) if (!Array.isArray(state[field])) throw new Error(`Campo ${field} inválido.`);
  if (!state.materials || typeof state.materials !== 'object' || Array.isArray(state.materials)) throw new Error('Materiais inválidos.');
  if (!['wide', 'slim'].includes(state.skinVariant)) throw new Error('Variante de skin inválida.');
  if (state.projectileVisual !== null && !resourceId(state.projectileVisual)) throw new Error('ID de projétil visual inválido.');
  if (!Number.isInteger(state.nextEventOrder) || state.nextEventOrder < 0) throw new Error('Ordem de eventos inválida.');
  for (const model of state.models) {
    if (!model || !TYPES[model.type] || !resourceId(model.id) || !Array.isArray(model.roots) || !model.roots.every(id => typeof id === 'string') || !Array.isArray(model.bindings)) throw new Error('Declaração de modelo inválida.');
    if (model.bindings.some(binding => !binding || !['group', 'referenceGroup', 'target'].every(field => typeof binding[field] === 'string') || !BODY_PARTS.includes(binding.target))) throw new Error('Vínculo de modelo inválido.');
  }
  if ([...state.effects, ...state.sounds].some(resource => !resource || typeof resource !== 'object' || !resourceId(resource.id))) throw new Error('Recurso de efeito/som inválido.');
  if (state.context && (typeof state.context.root !== 'string' || !state.context.groups || !BODY_PARTS.every(part => typeof state.context.groups[part] === 'string') || !Array.isArray(state.context.geometry))) throw new Error('Contexto de player inválido.');
  return state;
}
function defaultEffect(kind = 'particle', id = 'custom:effect') {
  if (!EFFECT_TYPES[kind]) throw new Error('Tipo de efeito desconhecido.');
  assertId(id);
  return { id, kind, duration: 1, lifetime: 0.7, burst: 16, rate: 16, seed: 1, shape: 'point', shapeSize: [1, 1, 1],
    speed: 5, speedSpread: 1, direction: [0, 1, 0], spread: 45, gravity: [0, -9.81, 0], drag: 0,
    size: 0.5, sizeEnd: 0, color: '#ffffff', colorEnd: '#ff9b33', opacity: 1, opacityEnd: 0,
    rotation: 0, angularVelocity: 0, texture: '', blend: 'alpha', columns: 1, rows: 1, frames: 1, fps: 12,
    curves: {}, trailLength: 0.35, trailSamples: 24, width: 1, startOffset: [0, 0, 0], endOffset: [0, 5, 0],
    intensity: 2, distance: 12, follow: false };
}
function validateEffect(effect) {
  const errors = [];
  const bad = message => errors.push(`${effect?.id || 'Efeito'}: ${message}`);
  if (!resourceId(effect?.id)) bad('ID inválido.');
  if (!EFFECT_TYPES[effect?.kind]) bad('tipo desconhecido.');
  const ranges = { duration: [0.001, 3600], lifetime: [0.001, 120], burst: [0, MAX_PARTICLES], rate: [0, MAX_PARTICLES],
    seed: [0, 0xffffffff], speed: [0, 10000], speedSpread: [0, 10000], spread: [0, 180], drag: [0, 100],
    size: [0, 10000], sizeEnd: [0, 10000], opacity: [0, 1], opacityEnd: [0, 1], rotation: [-36000, 36000],
    angularVelocity: [-36000, 36000], columns: [1, 256], rows: [1, 256], frames: [1, 65536], fps: [0, 240],
    trailLength: [0.001, 30], trailSamples: [2, 240], width: [0.001, 10000], intensity: [0, 10000], distance: [0, 10000] };
  for (const [key, [min, max]] of Object.entries(ranges)) if (!Number.isFinite(effect?.[key]) || effect[key] < min || effect[key] > max) bad(`${key} deve ficar entre ${min} e ${max}.`);
  for (const key of ['burst', 'seed', 'columns', 'rows', 'frames', 'trailSamples']) if (!Number.isInteger(effect?.[key])) bad(`${key} deve ser inteiro.`);
  for (const key of ['shapeSize', 'direction', 'gravity', 'startOffset', 'endOffset'])
    if (!Array.isArray(effect?.[key]) || effect[key].length !== 3 || !effect[key].every(v => Number.isFinite(v) && Math.abs(v) <= 1e6)) bad(`${key} deve ter três números finitos.`);
  if (!['point', 'box', 'sphere', 'cone'].includes(effect?.shape)) bad('forma de emissão inválida.');
  if (Array.isArray(effect?.shapeSize) && effect.shapeSize.some(value => value < 0)) bad('dimensões da emissão não podem ser negativas.');
  if (!['alpha', 'additive'].includes(effect?.blend)) bad('mistura inválida.');
  if (!/^#[a-f0-9]{6}$/i.test(effect?.color) || !/^#[a-f0-9]{6}$/i.test(effect?.colorEnd)) bad('cor inválida.');
  if (effect?.frames > effect?.columns * effect?.rows) bad('flipbook excede a grade do sprite.');
  if (effect?.kind === 'particle' && effect.burst + Math.ceil(effect.rate * Math.min(effect.duration, effect.lifetime)) > MAX_PARTICLES)
    bad(`emissão excede o limite explícito de ${MAX_PARTICLES} partículas simultâneas por efeito.`);
  if (!effect?.curves || typeof effect.curves !== 'object' || Array.isArray(effect.curves)) bad('curvas inválidas.');
  else for (const [key, points] of Object.entries(effect.curves)) {
    if (!['size', 'opacity', 'rotation', 'color'].includes(key)) { bad(`curva ${key} desconhecida.`); continue; }
    if (!Array.isArray(points) || points.length < 2 || points.length > 64) { bad(`curva ${key} exige de 2 a 64 pontos.`); continue; }
    const times = new Set();
    for (const p of points) {
      if (!Number.isFinite(p?.age) || p.age < 0 || p.age > 1 || times.has(p.age)) bad(`tempo inválido ou repetido na curva ${key}.`);
      times.add(p?.age);
      if (key === 'color' ? !/^#[a-f0-9]{6}$/i.test(p?.value) : !Number.isFinite(p?.value)) bad(`valor inválido na curva ${key}.`);
      if (key === 'opacity' && (p?.value < 0 || p?.value > 1)) bad('opacidade da curva fora de 0–1.');
      if (key === 'size' && p?.value < 0) bad('tamanho da curva negativo.');
    }
  }
  return errors;
}
function stableJson(value) {
  const sort = v => Array.isArray(v) ? v.map(sort) : v && typeof v === 'object'
    ? Object.fromEntries(Object.keys(v).sort().filter(k => v[k] !== undefined).map(k => [k, sort(v[k])])) : v;
  return JSON.stringify(sort(value), null, 2) + '\n';
}
function orderedEvents(events) { return events.slice().sort((a, b) => a.time - b.time || (a.order ?? 0) - (b.order ?? 0) || String(a.id).localeCompare(String(b.id))); }
function eventIntervals(events, definitions, end) {
  const result = [], active = new Map();
  for (const event of orderedEvents(events)) {
    const definition = definitions.find(e => e.id === event.resource);
    const key = event.instance || `${event.resource}|${event.target || ''}`;
    if (event.action === 'stop') { const entry = active.get(key); if (entry) { entry.end = Math.min(entry.end, event.time); active.delete(key); } continue; }
    if (!definition || event.type === 'sound') continue;
    if (event.action === 'start' && active.has(key)) { active.get(key).end = Math.min(active.get(key).end, event.time); active.delete(key); }
    const entry = { event, definition, start: event.time,
      end: Math.min(end, event.time + definition.duration), continuous: event.action === 'start' };
    result.push(entry); if (entry.continuous) active.set(key, entry);
  }
  return result;
}
function crossedEvents(events, previous, now, { length = 0, looped = false, initial = false } = {}) {
  const sorted = orderedEvents(events);
  if (initial) return sorted.filter(e => e.time <= now && e.time >= 0);
  if (looped && length > 0) return sorted.filter(e => e.time > previous && e.time <= length).concat(sorted.filter(e => e.time <= now));
  if (now < previous) return [];
  return sorted.filter(e => e.time > previous && e.time <= now);
}


// Original geometry, standard 64x64 skin layout. No third-party images are included.
function playerParts(variant = 'wide') {
  if (!['wide', 'slim'].includes(variant)) throw new Error('Variante de braço inválida.');
  const w = variant === 'slim' ? 3 : 4;
  return [
    { name: 'Head', origin: [0, 24, 0], from: [-4, 24, -4], to: [4, 32, 4], uv: [0, 0], outerUv: [32, 0], inflate: 0.5 },
    { name: 'Body', origin: [0, 24, 0], from: [-4, 12, -2], to: [4, 24, 2], uv: [16, 16], outerUv: [16, 32], inflate: 0.25 },
    { name: 'LeftArm', origin: [5, 22, 0], from: [4, 12, -2], to: [4 + w, 24, 2], uv: [32, 48], outerUv: [48, 48], inflate: 0.25 },
    { name: 'RightArm', origin: [-5, 22, 0], from: [-4 - w, 12, -2], to: [-4, 24, 2], uv: [40, 16], outerUv: [40, 32], inflate: 0.25 },
    { name: 'LeftLeg', origin: [2, 12, 0], from: [0, 0, -2], to: [4, 12, 2], uv: [16, 48], outerUv: [0, 48], inflate: 0.25 },
    { name: 'RightLeg', origin: [-2, 12, 0], from: [-4, 0, -2], to: [0, 12, 2], uv: [0, 16], outerUv: [0, 32], inflate: 0.25 },
  ];
}
function skinFaces([u, v], [w, h, d]) {
  return {
    east: [u, v + d, u + d, v + d + h],
    north: [u + d, v + d, u + d + w, v + d + h],
    west: [u + d + w, v + d, u + 2 * d + w, v + d + h],
    south: [u + 2 * d + w, v + d, u + 2 * d + 2 * w, v + d + h],
    up: [u + d + w, v + d, u + d, v],
    down: [u + d + 2 * w, v, u + d + w, v + d],
  };
}
const defaultClipNames = type => type === 'player' ? ['idle', 'walk', 'run', 'jump']
  : type === 'ranged' ? ['idle', 'run', 'jump', 'fire', 'reload'] : type === 'melee' ? ['idle', 'run', 'jump', 'attack'] : ['idle'];
function neutralSkinCanvas(g) {
  const canvas = g.document.createElement('canvas'); canvas.width = canvas.height = 64;
  const ctx = canvas.getContext('2d'); ctx.clearRect(0, 0, 64, 64);
  for (const part of playerParts()) for (const rect of Object.values(skinFaces(part.uv, part.to.map((n, i) => n - part.from[i])))) {
    ctx.fillStyle = part.name === 'Head' ? '#bbbbbb' : '#999999';
    ctx.fillRect(rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1]);
  }
  return canvas;
}
function neutralSkin(g) { return neutralSkinCanvas(g).toDataURL('image/png'); }
function makePlayer(g, { variant = 'wide', reference = false, rootName = 'Player' } = {}) {
  const root = new g.Group({ name: rootName, origin: [0, 0, 0] }).init();
  const groups = {}, geometry = [], layers = [];
  const texture = new g.Texture({ name: reference ? 'Referência neutra' : 'Skin 64x64', width: 64, height: 64, uv_width: 64, uv_height: 64 });
  const skin = neutralSkinCanvas(g); texture.canvas.width = texture.canvas.height = 64; texture.ctx.drawImage(skin, 0, 0);
  texture.fromDataURL(skin.toDataURL('image/png')).add();
  for (const part of playerParts(variant)) {
    const group = new g.Group({ name: part.name, origin: part.origin }).addTo(root).init(); groups[part.name] = group.uuid;
    for (const layer of reference ? [false] : [false, true]) {
      const faces = Object.fromEntries(Object.entries(skinFaces(layer ? part.outerUv : part.uv, part.to.map((n, i) => n - part.from[i])))
        .map(([direction, uv]) => [direction, { uv, texture: texture.uuid }]));
      const cube = new g.Cube({ name: part.name + (layer ? '_layer' : ''), from: part.from, to: part.to,
        origin: part.origin, faces, box_uv: false, inflate: layer ? part.inflate : 0 }).addTo(group).init();
      geometry.push(cube.uuid); if (layer) layers.push(cube.uuid);
    }
  }
  if (!BODY_PARTS.every(name => groups[name])) throw new Error('Template de player incompleto.');
  return { root: root.uuid, groups, geometry, layers, texture: texture.uuid, variant };
}




function createStudio(g) {
  const disposables = [], listeners = [], dataProperties = new Set();
  const listen = (event, fn) => { g.Blockbench.on(event, fn); listeners.push([event, fn]); };
  const property = (target, type, name, options) => { const p = target.properties[name] || new g.Property(target, type, name, options); disposables.push(p); dataProperties.add(p); return p; };
  const isStudio = () => g.Format?.id === FORMAT;
  const state = () => {
    if (!isStudio()) throw new Error('Abra um projeto ShardBound Studio.');
    // Blockbench selects a fresh tab before calling ModelFormat.onSetup.
    if (!g.Project.shardbound_studio || !Object.keys(g.Project.shardbound_studio).length) g.Project.shardbound_studio = projectState();
    return checkState(g.Project.shardbound_studio);
  };
  property(g.ModelProject, 'object', 'shardbound_studio', { default: () => ({}), condition: () => isStudio(), exposed: false });
  property(g.KeyframeDataPoint, 'object', 'studio_event', { default: () => ({}), exposed: false,
    condition: point => point.keyframe.channel === CHANNEL });
  // The 5.2.2 helper addChannel fails to initialize some existing projects. Register the public channel descriptor explicitly.
  const channelDescriptor = { name: 'ShardBound: efeitos e sons', mutable: true, max_data_points: 1, condition: isStudio };
  const syncChannel = () => { if (isStudio()) g.EffectAnimator.prototype.channels[CHANNEL] = channelDescriptor; else delete g.EffectAnimator.prototype.channels[CHANNEL]; };
  syncChannel();
  for (const project of g.ModelProject.all.filter(project => project.format?.id === FORMAT)) for (const animation of project.animations || []) {
    const animator = animation.animators.effects;
    if (animator && !Array.isArray(animator[CHANNEL])) { g.Vue.set(animator, CHANNEL, []); g.Vue.set(animator.muted, CHANNEL, false); }
  }
  const format = new g.ModelFormat(FORMAT, { name: 'ShardBound Studio', description: 'Modelos, skins, animações e efeitos ShardBound',
    icon: 'auto_awesome', category: 'general', target: ['ShardBound'], meshes: true, rotate_cubes: true, bone_rig: true,
    armature_rig: false, centered_grid: true, optional_box_uv: true, box_uv_float_size: true,
    per_texture_uv_size: true, uv_rotation: true, animation_mode: true, locators: true,
    quaternion_interpolation: false, euler_order: 'ZYX', animation_files: false, can_convert_to: false,
    new() { studio.ui?.handlers.newProject(); return true; },
    onSetup(project) { if (!project.shardbound_studio?.schema) project.shardbound_studio = projectState(); } });
  disposables.push(format);
  const codec = new g.Codec(FORMAT, { name: 'Projeto ShardBound Studio', extension: 'sbstudio', remember: true, format,
    load_filter: { type: 'json', extensions: ['sbstudio'], condition: data => !!data?.shardbound_studio },
    compile(options = {}) { state(); return g.Codecs.project.compile({ ...options, bitmaps: true, absolute_paths: false }); },
    load(data, file = { path: '', no_file: true }) {
      checkState(data?.shardbound_studio);
      const effectErrors = data.shardbound_studio.effects.flatMap(validateEffect); if (effectErrors.length) throw new Error(effectErrors.join('\n'));
      if (data.meta?.model_format !== FORMAT) throw new Error('O arquivo não contém um projeto Studio.');
      g.Codecs.project.load(clone(data), { ...file, no_file: file.no_file ?? !file.path });
      for (const animation of g.Animation.all) effectsFor(animation);
      studio.refresh();
    },
    export() {
      const project = g.Project;
      g.Blockbench.export({ resource_id: FORMAT, type: 'Projeto ShardBound Studio', extensions: ['sbstudio'],
        name: g.Project.name || 'studio', startpath: g.Project.save_path, content: this.compile(),
        custom_writer: (content, path) => { g.Blockbench.writeFile(path, { content }, () => { project.save_path = path; project.saved = true; }); } });
    } });
  disposables.push(codec);
  format.codec = codec;
  function effectsFor(animation) {
    const animator = animation.animators.effects ||= new g.EffectAnimator(animation);
    if (animator.studioController === studio) return animator;
    animator.studioController = studio;
    // Native effect channels target other runtimes. Studio owns one combined authoring channel.
    animator.channels = { [CHANNEL]: channelDescriptor };
    for (const channel of ['particle', 'sound', 'timeline', CHANNEL]) if (!Array.isArray(animator[channel])) animator[channel] = [];
    animator.createKeyframe = function(value, time, channel, ...args) {
      if (channel === CHANNEL) return studio.onRequestEvent?.(animation, time ?? g.Timeline.time);
      return g.EffectAnimator.prototype.createKeyframe.call(this, value, time, channel, ...args);
    };
    return animator;
  }
  const find = uuid => g.Group.all.find(e => e.uuid === uuid) || g.Outliner.elements.find(e => e.uuid === uuid);
  function repairTemplateUV() {
    if (!isStudio()) return;
    const s = state(), repaired = [];
    for (const player of [s.context, ...s.models.map(m => m.player)].filter(Boolean)) {
      const texture = g.Texture.all.find(t => t.uuid === player.texture);
      if (!texture || texture.uv_width !== 32 || texture.uv_height !== 32) continue;
      const parts = playerParts(player.variant || s.skinVariant);
      // Only repair the exact generated skin layout. Authored/custom UVs are never rewritten.
      const knownLayout = player.geometry.length && player.geometry.every(id => {
        const cube = find(id), part = parts.find(p => p.name === cube?.name?.replace(/_layer$/, ''));
        if (!part || cube.box_uv) return false;
        const expected = skinFaces(cube.name.endsWith('_layer') ? part.outerUv : part.uv, part.to.map((v, i) => v - part.from[i]));
        return Object.entries(expected).every(([face, uv]) => cube.faces[face]?.texture === texture.uuid && uv.every((v, i) => v === cube.faces[face].uv[i]));
      });
      if (knownLayout) repaired.push(texture);
    }
    if (repaired.length) {
      edit('Studio: corrigir escala UV do template 64×64', () => repaired.forEach(t => { t.uv_width = t.uv_height = 64; }), { textures: repaired, uv_only: true });
      g.Blockbench.showQuickMessage('Escala UV 64×64 corrigida nos templates do player. A alteração pode ser desfeita.', 6000);
    }
  }
  function edit(label, fn, aspects = {}) {
    state();
    if (g.Undo.current_save) throw new Error('Conclua a edição ativa primeiro.');
    g.Undo.initEdit({ studio: true, ...aspects });
    try { const result = fn(); g.Undo.finishEdit(label); studio.refresh(); return result; }
    catch (error) { g.Undo.cancelEdit(true); studio.refresh(); throw error; }
  }
  listen('create_undo_save', ({ save, aspects }) => { if (isStudio() && aspects.studio) save.shardbound_studio = clone(g.Project.shardbound_studio); });
  listen('load_undo_save', ({ save }) => { if (isStudio() && save.shardbound_studio) { g.Project.shardbound_studio = clone(save.shardbound_studio); studio.refresh(); } });
  function context() {
    const s = state();
    if (!s.context) s.context = makePlayer(g, { variant: s.skinVariant, reference: true, rootName: 'Player de referência' });
    return s.context;
  }
  function addModel(type = 'item', options = {}) {
    const s = state(); if (!TYPES[type]) throw new Error('Tipo de modelo inválido.');
    const id = assertId(options.id || `${s.id}_${s.models.length + 1}`);
    if (s.models.some(m => m.id === id)) throw new Error('Já existe um modelo com esse ID.');
    const model = { id, type, name: options.name || TYPES[type], roots: [], bindings: [] };
    if (type === 'player') {
      const player = makePlayer(g, { variant: s.skinVariant }); model.roots = [player.root]; model.player = player;
    } else if (type === 'armor') {
      const ref = context();
      for (const part of playerParts(s.skinVariant)) {
        const root = new g.Group({ name: `Armadura_${part.name}`, origin: part.origin }).addTo(find(ref.groups[part.name])).init();
        model.roots.push(root.uuid); model.bindings.push({ group: root.uuid, target: part.name, referenceGroup: ref.groups[part.name] });
      }
    } else {
      let parent = 'root', origin = [0, 0, 0];
      if (['melee', 'ranged'].includes(type)) { const ref = context(); parent = find(ref.groups.RightArm); origin = [-5, 16, 0]; }
      const root = new g.Group({ name: options.name || type, origin }).addTo(parent).init(); model.roots.push(root.uuid);
      if (parent !== 'root') model.bindings.push({ group: root.uuid, target: 'RightArm', referenceGroup: parent.uuid });
      if (type === 'block') new g.Cube({ name: 'Bloco', from: [0, 0, 0], to: [16, 16, 16] }).addTo(root).init();
    }
    s.models.push(model); return model;
  }
  const studio = {
    version: VERSION, apiVersion: 1, format, codec, isStudio, state, find, edit, onRefresh: null, onRequestEvent: null, preview: null,
    refresh() { if (isStudio()) { g.Canvas.updateAll(); g.updateSelection(); } studio.preview?.invalidate(); studio.onRefresh?.(); },
    createProject(type = 'monster', options = {}) {
      if (!TYPES[type]) throw new Error('Tipo de projeto inválido.');
      assertId(options.id || 'custom:model');
      if (!g.newProject(format)) throw new Error('Não foi possível criar o projeto.');
      g.Project.name = options.name || TYPES[type]; g.Project.texture_width = g.Project.texture_height = type === 'player' ? 64 : 32;
      g.Project.shardbound_studio = projectState(type, options.id || 'custom:model');
      state().skinVariant = options.variant || 'wide';
      const model = addModel(type, { ...options, id: state().id });
      studio.refresh(); return model;
    },
    addModel(type, options) {
      const elements = [], groups = [], textures = [];
      return edit('Studio: adicionar modelo', () => {
        const before = new Set(g.Outliner.elements), beforeG = new Set(g.Group.all), beforeT = new Set(g.Texture.all);
        const result = addModel(type, options);
        elements.push(...g.Outliner.elements.filter(e => !before.has(e))); groups.push(...g.Group.all.filter(e => !beforeG.has(e)));
        textures.push(...g.Texture.all.filter(e => !beforeT.has(e))); return result;
      }, { elements, groups, textures, outliner: true });
    },
    selectModel(model) { model.roots.map(find).filter(Boolean).forEach((root, i) => i ? root.multiSelect?.() : root.select()); },
    renameModel(model, id, name) { assertId(id); if (state().models.some(m => m !== model && m.id === id)) throw new Error('ID de modelo repetido.'); return edit('Studio: propriedades do modelo', () => { model.id = id; model.name = name; }); },
    removeModel(model) { return edit('Studio: remover modelo', () => { model.roots.map(find).filter(Boolean).forEach(root => root.remove()); state().models = state().models.filter(m => m.id !== model.id); }, { elements: g.Outliner.elements.slice(), groups: g.Group.all.slice(), outliner: true }); },
    createClip(name = 'idle', length = 1, loop = 'loop', fps = 24) {
      if (!name.trim() || g.Animation.all.some(a => a.name === name)) throw new Error('Nome de clip vazio ou repetido.');
      if (!(length > 0 && length <= 1000) || !Number.isFinite(fps) || fps < 1 || fps > 120 || !['once', 'loop', 'hold'].includes(loop)) throw new Error('Use duração até 1000 s e timeline de 1–120 FPS, limites nativos do Blockbench 5.2.2.');
      const animations = [];
      return edit('Studio: criar clip', () => {
        const animation = new g.Animation({ name, length, loop, snapping: fps }).add(); animations.push(animation);
        g.Modes.options.animate.select(); animation.select();
        effectsFor(animation).addToTimeline(true);
        return animation;
      }, { animations });
    },
    addEffect(kind, id, values = {}) {
      const effect = Object.assign(defaultEffect(kind, id), clone(values));
      const errors = validateEffect(effect); if (errors.length) throw new Error(errors.join('\n'));
      if ([...state().effects, ...state().sounds].some(e => e.id === effect.id)) throw new Error('ID de efeito/som repetido.');
      return edit('Studio: criar efeito', () => { state().effects.push(effect); return effect; });
    },
    updateEffect(id, values) {
      const index = state().effects.findIndex(e => e.id === id); if (index < 0) throw new Error('Efeito não encontrado.');
      const effect = Object.assign(clone(state().effects[index]), clone(values));
      const errors = validateEffect(effect); if (errors.length) throw new Error(errors.join('\n'));
      if (effect.id !== id && [...state().effects, ...state().sounds].some(e => e.id === effect.id)) throw new Error('ID de efeito/som repetido.');
      // Changing the visual application keeps every existing timeline event compatible.
      for (const event of studio.allEvents().filter(e => e.resource === id)) studio.validateEvent({ ...event, resource: effect.id, type: effect.kind }, effect);
      return edit('Studio: editar efeito', () => {
        state().effects[index] = effect;
        for (const animation of g.Animation.all) for (const kf of animation.animators.effects?.[CHANNEL] || []) for (const point of kf.data_points)
          if (point.studio_event.resource === id) { point.studio_event.resource = effect.id; point.studio_event.type = effect.kind; }
        return effect;
      }, { animations: g.Animation.all.slice() });
    },
    removeEffect(id) { if (studio.allEvents().some(e => e.resource === id)) throw new Error('Remova os keyframes que referenciam esse efeito primeiro.');
      return edit('Studio: remover efeito', () => { state().effects = state().effects.filter(e => e.id !== id); }); },
    async addSound(id, data, filename = '', beforeCommit = () => {}, placement = null) {
      assertId(id); if ([...state().effects, ...state().sounds].some(e => e.id === id)) throw new Error('ID de efeito/som repetido.');
      if (!/^data:audio\/(wav|x-wav|ogg|mpeg);base64,[A-Za-z0-9+/=]+$/.test(data) || data.length > 32 * 1024 * 1024)
        throw new Error('Use áudio WAV, OGG ou MP3, até 24 MiB.');
      const project = g.Project, audio = new g.AudioContext(); let decoded;
      try { decoded = await audio.decodeAudioData(Uint8Array.from(g.atob(data.split(',')[1]), c => c.charCodeAt(0)).buffer); }
      catch { throw new Error('O áudio não pode ser decodificado pelo Blockbench. Verifique o arquivo WAV, OGG ou MP3.'); }
      finally { await audio.close(); }
      if (g.Project !== project) throw new Error('O projeto mudou durante a importação do áudio.');
      if ([...state().effects, ...state().sounds].some(e => e.id === id)) throw new Error('ID de efeito/som repetido.');
      beforeCommit();
      const sound = { id, data, filename, duration: decoded.duration };
      if (placement?.eventId) {
        const point = (placement.animation?.animators.effects?.[CHANNEL] || []).flatMap(k => k.data_points.map(p => ({ point: p, keyframe: k })))
          .find(({ point, keyframe }) => point.studio_event.id === placement.eventId || `${keyframe.uuid}:${point.studio_event.id}` === placement.eventId)?.point;
        if (!point) throw new Error('O evento de áudio não existe mais.');
        const event = { ...clone(point.studio_event), resource: id, type: 'sound' };
        studio.validateEvent(event, sound);
        return edit('Studio: substituir áudio no evento', () => { state().sounds.push(sound); point.studio_event = event; return sound; }, { animations: [placement.animation] });
      }
      if (placement) {
        studio.addEvent(placement.animation, placement.time, { type: 'sound', resource: id, target: '', action: 'burst' }, sound);
        return sound;
      }
      return edit('Studio: importar som', () => { state().sounds.push(sound); return sound; });
    },
    removeSound(id) { if (studio.allEvents().some(e => e.resource === id)) throw new Error('Remova os keyframes desse som primeiro.');
      return edit('Studio: remover som', () => { state().sounds = state().sounds.filter(s => s.id !== id); }); },
    addEvent(animation, time, values, newDefinition = null) {
      state(); if (!animation || !g.Animation.all.includes(animation)) throw new Error('Selecione um clip.');
      if (!Number.isFinite(time) || time < 0 || time > animation.length) throw new Error('O frame deve estar dentro da duração do clip.');
      const data = { id: g.guid(), type: 'particle', action: 'burst', target: '', offset: [0, 0, 0], follow: false,
        instance: '', volume: 1, pitch: 1, loop: false, ...clone(values), order: state().nextEventOrder };
      if (newDefinition) {
        if (newDefinition.kind) { const errors = validateEffect(newDefinition); if (errors.length) throw new Error(errors.join('\n')); }
        else if (data.type !== 'sound' || !(newDefinition.duration > 0) || !/^data:audio\/(wav|ogg|mpeg);base64,/.test(newDefinition.data)) throw new Error('Áudio novo inválido.');
        if (data.resource !== newDefinition.id || [...state().effects, ...state().sounds].some(e => e.id === newDefinition.id)) throw new Error('Recurso novo inválido ou repetido.');
      }
      studio.validateEvent(data, newDefinition);
      const keyframes = [], animations = [animation];
      return edit('Studio: evento na timeline', () => {
        if (newDefinition) state()[newDefinition.kind ? 'effects' : 'sounds'].push(clone(newDefinition));
        state().nextEventOrder++;
        const animator = effectsFor(animation); animator.addToTimeline(true);
        let keyframe = animator[CHANNEL].find(k => Math.abs(k.time - time) < 1e-8);
        if (keyframe) {
          const point = new g.KeyframeDataPoint(keyframe); point.extend({ studio_event: data }); keyframe.data_points.push(point);
        } else { keyframe = animator.addKeyframe({ channel: CHANNEL, time, data_points: [{ studio_event: data }] }); keyframes.push(keyframe); }
        keyframe.select(); return { keyframe, event: data };
      }, { keyframes, animations });
    },
    validateEvent(event, resourceOverride = null) {
      if (typeof event.id !== 'string' || !event.id || !Number.isInteger(event.order) || event.order < 0 || typeof event.target !== 'string' || typeof event.instance !== 'string' || typeof event.follow !== 'boolean' || typeof event.loop !== 'boolean') throw new Error('Identificação ou configuração de evento inválida.');
      if (!['particle', 'trail', 'flash', 'glow', 'sound'].includes(event.type)) throw new Error('Tipo de evento desconhecido.');
      if (!['burst', 'start', 'stop'].includes(event.action)) throw new Error('Ação de evento inválida.');
      const resource = resourceOverride || (event.type === 'sound' ? state().sounds : state().effects).find(r => r.id === event.resource);
      if (!resource || (event.type !== 'sound' && resource.kind !== event.type)) throw new Error('Recurso não encontrado ou incompatível com o tipo do evento.');
      if (event.target && !find(event.target)) throw new Error('Pasta/marcador do evento não encontrado.');
      if (['trail', 'flash'].includes(event.type) && !event.target) throw new Error('Escolha uma pasta ou marcador para o rastro/flash.');
      if (event.type === 'flash') {
        const target = find(event.target), node = target instanceof g.Locator ? target.parent : target;
        const geometry = candidate => candidate instanceof g.Cube || candidate instanceof g.Mesh || candidate?.children?.some(geometry);
        if (!geometry(node)) throw new Error('O alvo do flash deve conter geometria.');
      }
      if (!Array.isArray(event.offset) || event.offset.length !== 3 || !event.offset.every(Number.isFinite)) throw new Error('Offset do evento inválido.');
      if (!Number.isFinite(event.volume) || event.volume < 0 || event.volume > 1 || !Number.isFinite(event.pitch) || event.pitch < 0.1 || event.pitch > 4) throw new Error('Volume/pitch inválido.');
      return event;
    },
    events(animation = g.Animation.selected) {
      return orderedEvents((animation?.animators.effects?.[CHANNEL] || []).flatMap(keyframe => keyframe.data_points.map(point => ({ ...clone(point.studio_event), id: `${keyframe.uuid}:${point.studio_event.id}`, time: keyframe.time, frame: Math.round(keyframe.time * animation.snapping), fps: animation.snapping, clip: animation.name, keyframe: keyframe.uuid }))));
    },
    allEvents() { return g.Animation.all.flatMap(animation => studio.events(animation)); },
    updateEvent(eventId, values, time) {
      for (const animation of g.Animation.all) for (const keyframe of animation.animators.effects?.[CHANNEL] || []) {
        const point = keyframe.data_points.find(p => p.studio_event.id === eventId || `${keyframe.uuid}:${p.studio_event.id}` === eventId); if (!point) continue;
        const event = { ...clone(point.studio_event), ...clone(values) }; studio.validateEvent(event);
        if (time !== undefined && (!Number.isFinite(time) || time < 0 || time > animation.length)) throw new Error('Frame fora do clip.');
        return edit('Studio: editar evento', () => {
          point.studio_event = event;
          if (time === undefined || Math.abs(time - keyframe.time) < 1e-8) return;
          const animator = effectsFor(animation), destination = animator[CHANNEL].find(k => k !== keyframe && Math.abs(k.time - time) < 1e-8);
          if (destination) { const added = new g.KeyframeDataPoint(destination); added.extend({ studio_event: event }); destination.data_points.push(added); }
          else animator.addKeyframe({ channel: CHANNEL, time, data_points: [{ studio_event: event }] });
          keyframe.data_points = keyframe.data_points.filter(p => p !== point); if (!keyframe.data_points.length) keyframe.remove();
        }, { animations: [animation] });
      }
      throw new Error('Evento não encontrado.');
    },
    removeEvent(id) {
      return edit('Studio: remover evento', () => {
        for (const animation of g.Animation.all) for (const keyframe of (animation.animators.effects?.[CHANNEL] || []).slice()) {
          keyframe.data_points = keyframe.data_points.filter(p => p.studio_event.id !== id && `${keyframe.uuid}:${p.studio_event.id}` !== id); if (!keyframe.data_points.length) keyframe.remove();
        }
      }, { animations: g.Animation.all.slice() });
    },
    addLocator(name, parent = g.Group.first_selected) {
      if (!parent) throw new Error('Selecione uma pasta para o marcador.');
      const elements = [];
      return edit('Studio: marcador', () => { const locator = new g.Locator({ name, position: parent.origin.slice() }).addTo(parent).init(); elements.push(locator); return locator; }, { elements, outliner: true });
    },
    bind(model, part = 'RightArm', rootUuid = model.roots[0]) {
      if (!BODY_PARTS.includes(part)) throw new Error('Parte corporal inválida.');
      const elements = [], groups = g.Group.all.slice(), textures = [];
      return edit('Studio: encaixar equipamento', () => {
        const before = new Set(g.Outliner.elements), beforeGroups = new Set(g.Group.all), beforeTextures = new Set(g.Texture.all);
        const ref = context(), root = find(rootUuid); if (!root || !model.roots.includes(rootUuid)) throw new Error('Pasta do modelo inválida.');
        root.addTo(find(ref.groups[part])); model.bindings = model.bindings.filter(b => b.group !== rootUuid);
        model.bindings.push({ group: rootUuid, target: part, referenceGroup: ref.groups[part] });
        elements.push(...g.Outliner.elements.filter(e => !before.has(e))); groups.push(...g.Group.all.filter(e => !beforeGroups.has(e))); textures.push(...g.Texture.all.filter(e => !beforeTextures.has(e)));
      }, { elements, groups, textures, outliner: true });
    },
    importSkin(data, beforeCommit = () => {}) {
      const project = g.Project, model = state().models.find(m => m.type === 'player'); if (!model) throw new Error('Crie um projeto/modelo de player para importar a skin.');
      return new Promise((resolve, reject) => { const image = new g.Image(); image.onerror = () => reject(new Error('PNG inválido.'));
        image.onload = () => {
          if (g.Project !== project) return reject(new Error('O projeto mudou durante a importação da skin.'));
          if (image.width !== 64 || image.height !== 64) return reject(new Error('A skin deve ser PNG 64×64.'));
          const texture = g.Texture.all.find(t => t.uuid === model.player.texture); if (!texture) return reject(new Error('Textura da skin não encontrada.'));
          try { beforeCommit(); edit('Studio: importar skin', () => {
            // The decoded pixels are available before Undo captures the resulting bitmap.
            texture.fromDataURL(data); texture.canvas.width = 64; texture.canvas.height = 64; texture.ctx.drawImage(image, 0, 0); texture.updateChangesAfterEdit();
          }, { textures: [texture], bitmap: true }); resolve(texture); } catch (e) { reject(e); }
        }; image.src = data; });
    },
    setSkinLayers(visible) { const ids = state().models.flatMap(m => m.player?.layers || []), elements = ids.map(find).filter(Boolean);
      return edit('Studio: camadas da skin', () => elements.forEach(e => { e.visibility = !!visible; }), { elements }); },
    dispose() {
      studio.preview?.dispose();
      for (const [event, fn] of listeners) g.Blockbench.removeListener(event, fn);
      const hasOpenProjects = g.ModelProject.all.some(project => project.format?.id === FORMAT);
      // Unloading must never strip serializable metadata/events from a still-open project.
      for (const disposable of disposables.reverse()) if (!hasOpenProjects || !dataProperties.has(disposable)) disposable.delete();
      delete g.EffectAnimator.prototype.channels[CHANNEL];
    },
  };
  listen('select_project', () => { syncChannel(); if (isStudio()) for (const animation of g.Animation.all) effectsFor(animation); studio.preview?.reset(); studio.onRefresh?.(); });
  disposables.push(g.Codecs.project.on('parsed', () => { if (isStudio()) { for (const animation of g.Animation.all) effectsFor(animation); repairTemplateUV(); studio.refresh(); } }));
  listen('finished_edit', () => { studio.preview?.invalidate(); studio.onRefresh?.(); });
  for (const event of ['undo', 'redo']) listen(event, () => { if (isStudio()) { for (const animation of g.Animation.all) if (animation.animators.effects) effectsFor(animation); studio.refresh(); } });
  listen('update_keyframe_selection', () => studio.onRefresh?.());
  listen('select_animation', () => { if (isStudio() && g.Animation.selected) effectsFor(g.Animation.selected); studio.preview?.reset(); studio.onRefresh?.(); });
  for (const project of g.ModelProject.all) if (project.format?.id === FORMAT) project.format = format;
  if (isStudio()) { format.select(); for (const animation of g.Animation.all) effectsFor(animation); repairTemplateUV(); }
  return studio;
}

// Native buffer groups use different material indexing for cuboids and polygon meshes.
function faceTextures(g, node) {
  const faces = node instanceof g.Cube ? g.Canvas.face_order.map(key => node.faces[key]).filter(face => face.texture !== null)
    : Object.values(node.faces).filter(face => face.vertices.length >= 3);
  const textures = faces.map(face => face.getTexture());
  return node instanceof g.Mesh ? textures.filter((texture, index, all) => !index || texture !== all[index - 1]) : textures;
}
// Keep native painting, selection and lighting; apply the exported alpha/face contract to its shader.
function createMaterialPreview(g, studio) {
  const originals = new Map();
  function restore() {
    for (const [material, old] of originals) {
      Object.assign(material, old); delete material.uniforms.STUDIO_OPAQUE; delete material.uniforms.STUDIO_CUTOFF; material.needsUpdate = true;
    }
    originals.clear();
  }
  function update() {
    if (!studio.isStudio()) { restore(); return; }
    for (const texture of g.Texture.all) {
      const material = texture.material, settings = studio.state().materials[texture.uuid] || { alphaMode: 'MASK', alphaCutoff: 0.5, doubleSided: true };
      if (!material?.isShaderMaterial) continue;
      if (!originals.has(material)) {
        originals.set(material, { fragmentShader: material.fragmentShader, side: material.side, transparent: material.transparent, blending: material.blending, depthWrite: material.depthWrite });
        const shader = material.fragmentShader.replace(/\bdiscard\s*;/g, ';'), end = shader.lastIndexOf('}');
        material.fragmentShader = 'uniform bool STUDIO_OPAQUE;\nuniform float STUDIO_CUTOFF;\n' + shader.slice(0, end) +
          '\nif (!STUDIO_OPAQUE && gl_FragColor.a < STUDIO_CUTOFF) discard;\ngl_FragColor.a = 1.0;\n' + shader.slice(end);
        material.uniforms.STUDIO_OPAQUE = { value: false }; material.uniforms.STUDIO_CUTOFF = { value: 0.5 }; material.needsUpdate = true;
      }
      const side = settings.doubleSided ? g.THREE.DoubleSide : g.THREE.FrontSide;
      if (material.side !== side) { material.side = side; material.needsUpdate = true; }
      material.uniforms.STUDIO_OPAQUE.value = settings.alphaMode === 'OPAQUE'; material.uniforms.STUDIO_CUTOFF.value = settings.alphaCutoff;
      material.transparent = false; material.depthWrite = true; material.blending = g.THREE.NoBlending;
    }
  }
  g.Blockbench.on('render_frame', update);
  update();
  return { update, dispose() { g.Blockbench.removeListener('render_frame', update); restore(); } };
}


function validateProject(g, studio) {
  const errors = [], warnings = [], error = message => errors.push(message);
  let state; try { state = checkState(studio.state()); } catch (e) { return { errors: [e.message], warnings, valid: false }; }
  const groups = g.Group.all, elements = g.Outliner.elements;
  if (!state.models.length) error('O projeto ainda não contém modelos declarados. Adicione um modelo independente.');
  const byId = new Map([...groups, ...elements].map(n => [n.uuid, n]));
  const uniqueIds = (items, label) => {
    const used = new Set(); for (const item of items) {
      if (!resourceId(item.id)) error(`${label}: ID inválido ${item.id}.`);
      if (used.has(item.id)) error(`${label}: ID repetido ${item.id}.`); used.add(item.id);
    }
  };
  uniqueIds(state.models, 'Modelo'); uniqueIds(state.effects, 'Efeito'); uniqueIds(state.sounds, 'Som');
  for (const sound of state.sounds) {
    if (!/^data:audio\/(wav|x-wav|ogg|mpeg);base64,[A-Za-z0-9+/=]+$/.test(sound.data) || !(sound.duration > 0 && Number.isFinite(sound.duration))) error(`${sound.id}: áudio importado inválido. Importe o arquivo novamente.`);
    if (state.effects.some(effect => effect.id === sound.id)) error(`${sound.id}: efeito e som devem ter IDs distintos para seleção na timeline.`);
  }
  const roots = new Set();
  if (state.context) {
    if (!(byId.get(state.context.root) instanceof g.Group)) error('Raiz do player de referência removida.');
    for (const part of BODY_PARTS) if (!(byId.get(state.context.groups[part]) instanceof g.Group)) error(`Player de referência: pasta ${part} removida.`);
  }
  for (const model of state.models) {
    if (!model.roots?.length) error(`${model.id}: modelo sem pasta raiz.`);
    for (const uuid of model.roots || []) {
      const root = byId.get(uuid); if (!(root instanceof g.Group)) { error(`${model.id}: pasta raiz removida.`); continue; }
      if (root.export === false) error(`${model.id}: pasta raiz desativada para exportação.`);
      if (roots.has(uuid)) error(`${model.id}: pasta atribuída a mais de um modelo.`); roots.add(uuid);
    }
    for (const binding of model.bindings || []) {
      if (!model.roots.includes(binding.group) || !byId.has(binding.referenceGroup)) error(`${model.id}: vínculo de equipamento inválido.`);
      if (byId.get(binding.group)?.parent?.uuid !== binding.referenceGroup) error(`${model.id}: a pasta do equipamento foi movida para fora do vínculo. Use Encaixar para atualizar.`);
    }
    if (model.type === 'player') {
      const texture = g.Texture.all.find(t => t.uuid === model.player?.texture);
      if (!texture || texture.width !== 64 || texture.height !== 64) error(`${model.id}: skin exige uma textura 64×64.`);
    }
  }
  for (const group of groups) {
    if (!group.name?.trim()) error('Existe uma pasta sem nome.');
    if (!group.origin.every(Number.isFinite) || !group.rotation.every(Number.isFinite)) error(`${group.name}: pivô/rotação inválidos.`);
    const visited = new Set([group]); let parent = group.parent;
    while (parent instanceof g.Group) { if (visited.has(parent)) { error(`${group.name}: ciclo na hierarquia.`); break; } visited.add(parent); parent = parent.parent; }
    if (roots.has(group.uuid)) {
      for (const p of visited) if (p !== group && roots.has(p.uuid)) error(`${group.name}: modelos independentes não podem ser aninhados um dentro do outro.`);
    }
  }
  const names = groups.map(g => g.name);
  if (new Set(names).size !== names.length) warnings.push('Há nomes de pastas repetidos. O Studio usa UUIDs estáveis para distinguir os nós.');
  const isReference = node => state.context?.geometry?.includes(node.uuid);
  const owned = node => {
    let p = node; const seen = new Set();
    while (p && !seen.has(p)) { if (roots.has(p.uuid)) return true; seen.add(p); p = p.parent; }
    return false;
  };
  for (const element of elements) {
    if (isReference(element) || element instanceof g.Locator || element.export === false) continue;
    if (!owned(element)) { error(`${element.name}: geometria fora dos modelos declarados. Mova-a para uma pasta de modelo.`); continue; }
    if (!(element instanceof g.Cube || element instanceof g.Mesh)) { error(`${element.name}: tipo de geometria não suportado pelo Studio.`); continue; }
    if (element instanceof g.Cube && element.from.some((v, i) => !Number.isFinite(v) || !Number.isFinite(element.to[i]) || v > element.to[i])) error(`${element.name}: dimensões de cubo inválidas.`);
    if (element instanceof g.Mesh) {
      if (Object.values(element.vertices).some(v => !v.every(Number.isFinite))) error(`${element.name}: vértice inválido.`);
      for (const face of Object.values(element.faces)) if (face.vertices.some(id => !Object.hasOwn(element.vertices, id))) error(`${element.name}: face referencia vértice removido.`);
    }
    for (const [direction, face] of Object.entries(element.faces || {})) {
      if (face.texture === null || face.enabled === false) continue;
      const texture = face.getTexture();
      if (!texture) { error(`${element.name}/${direction}: textura ausente. Crie/aplique uma textura.`); continue; }
      if (texture.render_mode !== 'default') error(`${texture.name}: converta o material do modelo para o modo Default; efeitos aditivos/emissivos pertencem aos VFX Studio.`);
      if (!texture.img?.complete || !texture.img.naturalWidth) error(`${texture.name}: aguarde o carregamento da imagem ou recarregue a textura.`);
      const uvs = Array.isArray(face.uv) ? [face.uv.slice(0, 2), face.uv.slice(2, 4)] : Object.values(face.uv || {});
      if (uvs.some(v => v.some(n => !Number.isFinite(n)))) error(`${element.name}/${direction}: UV inválido.`);
      // UV wrapping is intentional in native Blockbench; diagnose it rather than silently clamp it.
      if (uvs.some(v => v[0] < 0 || v[1] < 0 || v[0] > texture.getUVWidth() || v[1] > texture.getUVHeight())) warnings.push(`${element.name}/${direction}: UV fora da imagem (wrap ${texture.wrap_mode}).`);
    }
  }
  const renderable = element => {
    for (let p = element; p && p !== 'root'; p = p.parent) if (p.export === false) return false;
    return element instanceof g.Cube ? Object.values(element.faces).some(face => face.texture !== null)
      : element instanceof g.Mesh && Object.values(element.faces).some(face => face.vertices.length >= 3);
  };
  for (const model of state.models) if (model.type !== 'effect' && !elements.some(e => owned(e) && !isReference(e) && renderable(e) && model.roots.some(root => {
    let p = e; while (p && p !== 'root') { if (p.uuid === root) return true; p = p.parent; } return false;
  }))) error(`${model.id}: modelo sem geometria exportável. Modele e habilite suas faces antes de exportar.`);
  for (const effect of state.effects) {
    errors.push(...validateEffect(effect));
    if (effect.texture && !g.Texture.all.some(t => t.uuid === effect.texture)) error(`${effect.id}: textura de sprite removida.`);
    const texture = g.Texture.all.find(t => t.uuid === effect.texture);
    if (texture && (!texture.img.complete || !texture.img.naturalWidth || texture.width % effect.columns || texture.height % effect.rows)) error(`${effect.id}: sprite não carregado ou dimensões incompatíveis com a grade de frames.`);
  }
  for (const texture of g.Texture.all) {
    const material = state.materials[texture.uuid];
    if (material && !['OPAQUE', 'MASK'].includes(material.alphaMode)) error(`${texture.name}: o perfil de modelo aceita OPAQUE ou MASK.`);
    if (material && (!Number.isFinite(material.alphaCutoff) || material.alphaCutoff < 0 || material.alphaCutoff > 1)) error(`${texture.name}: corte alpha inválido.`);
  }
  const clips = new Set();
  for (const animation of g.Animation.all) {
    if (!animation.name.trim() || clips.has(animation.name)) error(`Clip sem nome ou repetido: ${animation.name}.`); clips.add(animation.name);
    if (!(animation.length > 0 && animation.length <= 1000 && Number.isFinite(animation.length))) error(`${animation.name}: duração deve ficar entre 0 e 1000 s, limite da timeline nativa 5.2.2.`);
    if (!Number.isFinite(animation.snapping) || animation.snapping < 1 || animation.snapping > 120) error(`${animation.name}: a timeline nativa 5.2.2 suporta 1–120 FPS.`);
    for (const property of ['anim_time_update', 'start_delay', 'loop_delay']) if (animation[property] && animation[property] !== '0') error(`${animation.name}: ${property} altera o relógio do clip e não pertence ao contrato Studio. Use tempos e eventos explícitos.`);
    if (animation.blend_weight && !Number.isFinite(Number(animation.blend_weight))) error(`${animation.name}: peso de blend dinâmico não exportável. Use um valor numérico.`);
    if (animation.length * state.sampleRate * Math.max(groups.length, 1) > 2000000) error(`${animation.name}: exportação excede 2 milhões de amostras de grupos. Reduza duração/FPS.`);
    for (const [uuid, animator] of Object.entries(animation.animators)) {
      if (uuid === 'effects') {
        for (const channel of ['particle', 'sound', 'timeline']) {
          if (channel !== CHANNEL && animator[channel]?.length) error(`${animation.name}: use os eventos Studio em vez da trilha legada ${channel}.`);
        }
        continue;
      }
      if (!groups.some(group => group.uuid === uuid)) error(`${animation.name}: canal de animação referencia pasta removida.`);
      for (const kf of animator.keyframes) {
        if (!Number.isFinite(kf.time) || kf.time < 0 || kf.time > animation.length) error(`${animation.name}: keyframe de transformação fora da duração do clip.`);
        if (!['position', 'rotation', 'scale'].includes(kf.channel)) error(`${animation.name}: canal ${kf.channel} não exportável.`);
        if (!['linear', 'step', 'bezier', 'catmullrom'].includes(kf.interpolation)) error(`${animation.name}: interpolação ${kf.interpolation} desconhecida.`);
        for (const point of kf.data_points) if (['x', 'y', 'z'].some(axis => point[axis] === '' || !Number.isFinite(Number(point[axis]))))
          error(`${animation.name}: expressões dinâmicas não são um contrato de exportação. Use valores numéricos nos keyframes.`);
      }
    }
    for (const event of studio.events(animation)) {
      try { studio.validateEvent(event); } catch (e) { error(`${animation.name} / frame ${event.frame}: ${e.message}`); }
      if (!Number.isFinite(event.time) || event.time < 0 || event.time > animation.length) error(`${animation.name}: evento fora da duração do clip.`);
    }
  }
  return { valid: !errors.length, errors: [...new Set(errors)], warnings: [...new Set(warnings)] };
}





function createExporter(g, studio) {
  const T = g.THREE;
  function materialFor(texture, cache, state) {
    if (!texture) throw new Error('Textura ausente ao compilar geometria.');
    if (cache.has(texture.uuid)) return cache.get(texture.uuid);
    const settings = state.materials[texture.uuid] || { alphaMode: 'MASK', alphaCutoff: 0.5, doubleSided: true };
    const canvas = g.document.createElement('canvas'); canvas.width = texture.img.naturalWidth; canvas.height = texture.img.naturalHeight;
    canvas.getContext('2d').drawImage(texture.img, 0, 0);
    const map = new T.Texture(canvas); map.needsUpdate = true; map.magFilter = map.minFilter = T.NearestFilter; map.generateMipmaps = false;
    map.wrapS = map.wrapT = texture.wrap_mode === 'repeat' ? T.RepeatWrapping : T.ClampToEdgeWrapping;
    const material = new T.MeshStandardMaterial({ name: texture.name, map, color: 0xffffff, roughness: 1, metalness: 0,
      alphaTest: settings.alphaMode === 'MASK' ? settings.alphaCutoff : 0, transparent: false,
      side: settings.doubleSided ? T.DoubleSide : T.FrontSide });
    cache.set(texture.uuid, material); return material;
  }
  function sceneFor(model, state) {
    const scene = new T.Scene(), unitRoot = new T.Group(); unitRoot.name = 'ShardBound_metres'; unitRoot.scale.setScalar(1 / state.unitsPerMeter); scene.add(unitRoot);
    const clones = new Map(), materials = new Map(), roots = new Set(model.roots), referenceGeometry = new Set(state.context?.geometry || []);
    const belongs = node => { let p = node; while (p && p !== 'root') { if (roots.has(p.uuid)) return true; p = p.parent; } return false; };
    function visit(node) {
      if (node instanceof g.Group) {
        if (node.export === false) return null;
        const children = node.children.map(visit).filter(Boolean);
        if (!belongs(node) && !children.length) return null;
        const object = new T.Group(); object.name = node.name; object.uuid = node.mesh.uuid;
        object.position.copy(node.mesh.position); object.quaternion.copy(node.mesh.quaternion); object.scale.copy(node.mesh.scale);
        object.userData = { studio_uuid: node.uuid, studio_role: belongs(node) ? 'model' : 'reference_transform' };
        children.forEach(child => object.add(child)); clones.set(node.uuid, object); return object;
      }
      if (!belongs(node) || node.export === false || referenceGeometry.has(node.uuid)) return null;
      if (node instanceof g.Locator) {
        const object = new T.Group(); object.name = node.name; object.position.copy(node.mesh.position); object.quaternion.copy(node.mesh.quaternion);
        object.userData = { studio_uuid: node.uuid, studio_role: 'locator' }; return object;
      }
      if (!(node instanceof g.Cube || node instanceof g.Mesh)) return null;
      const geometry = node.mesh.geometry.clone();
      // Match 5.2.2 buffer material indices independently of the viewport's solid/UV/wireframe mode.
      const textures = faceTextures(g, node);
      if (!textures.length) { geometry.dispose(); return null; }
      const materialsOut = textures.map(texture => materialFor(texture, materials, state));
      const object = new T.Mesh(geometry, materialsOut.every(m => m === materialsOut[0]) ? materialsOut[0] : materialsOut); object.name = node.name; object.uuid = node.mesh.uuid;
      object.position.copy(node.mesh.position); object.quaternion.copy(node.mesh.quaternion); object.scale.copy(node.mesh.scale);
      object.userData = { studio_uuid: node.uuid }; return object;
    }
    for (const node of g.Outliner.root) { const object = visit(node); if (object) unitRoot.add(object); }
    scene.updateMatrixWorld(true); return { scene, clones, materials };
  }
  function samples(animation, groups, rate) {
    const times = new Set([0, animation.length]);
    for (let i = 1; i < Math.ceil(animation.length * rate); i++) times.add(i / rate);
    for (const animator of Object.values(animation.animators)) for (const keyframe of animator.keyframes || []) {
      if (keyframe.time >= 0 && keyframe.time <= animation.length) times.add(keyframe.time);
    }
    for (const animator of Object.values(animation.animators)) for (const channel of ['position', 'rotation', 'scale']) {
      const keys = (animator[channel] || []).slice().sort((a, b) => a.time - b.time);
      const boundary = time => {
        const precision = Math.max(1e-6, Math.abs(time) * 2 ** -21);
        for (const t of [time - precision, time + precision]) if (t > 0 && t < animation.length) times.add(t);
      };
      for (let i = 0; i < keys.length; i++) {
        // Native 5.2.2 snaps to a key within 1/1200s. Preserve both sides of its step boundary.
        if (i && keys[i - 1].interpolation === 'step') boundary(keys[i].time - 1 / 1200);
        if (keys[i].data_points.length > 1) boundary(keys[i].time + 1 / 1200);
      }
    }
    const sorted = [...new Set([...times].map(Math.fround))].sort((a, b) => a - b);
    const tracks = new Map(groups.map(group => [group.uuid, { position: [], quaternion: [], scale: [] }]));
    animation.select(); g.Animation.all.forEach(a => { a.playing = a === animation; });
    for (const time of sorted) {
      g.Timeline.time = time; g.Animator.preview(false); g.Project.model_3d?.updateMatrixWorld(true);
      for (const group of groups) {
        const values = tracks.get(group.uuid), mesh = group.mesh;
        mesh.position.toArray(values.position, values.position.length); mesh.quaternion.toArray(values.quaternion, values.quaternion.length); mesh.scale.toArray(values.scale, values.scale.length);
      }
    }
    return { times: sorted, tracks };
  }
  const exporter = {
    validate: () => validateProject(g, studio),
    async compile() {
      if (studio.exporting) throw new Error('Já existe uma exportação em andamento.');
      const report = exporter.validate(); if (!report.valid) throw new Error(report.errors.join('\n'));
      const state = clone(studio.state()), files = new Map(), packageModels = [];
      const previous = { project: g.Project, animation: g.Animation.selected, time: g.Timeline.time,
        playing: g.Timeline.playing, viewMode: g.Project.view_mode, flags: g.Animation.all.map(a => [a, a.playing]) };
      const scenes = [], baked = [], playerChannels = [];
      const clips = g.Animation.all.map(animation => ({ name: animation.name, length: animation.length,
        loop: animation.loop, fps: animation.snapping, events: studio.events(animation).map(({ keyframe, clip, ...event }) => event) }));
      const locators = g.Locator.all.map(locator => ({ uuid: locator.uuid, name: locator.name, parent: locator.parent?.uuid || null, position: locator.position.slice(), rotation: locator.rotation.slice() }));
      const definitions = clone(state.effects);
      for (const effect of definitions) if (effect.texture) {
        const texture = g.Texture.all.find(t => t.uuid === effect.texture); if (!texture) throw new Error('Sprite não encontrado.');
        const filename = assetPath(effect.id, 'png', 'effects');
        files.set(filename, Uint8Array.from(g.atob(texture.getBase64()), c => c.charCodeAt(0))); effect.texture = filename;
      }
      const originalAnimators = new Map(g.Animation.all.map(a => [a, new Set(Object.keys(a.animators))]));
      studio.exporting = true; if (studio.preview) studio.preview.suspended = true;
      try {
      g.Timeline.pause();
      try {
        g.Project.view_mode = 'textured'; g.Animator.showDefaultPose(); g.Canvas.updateAll();
        for (const model of state.models) {
          const built = sceneFor(model, state); scenes.push(built);
          packageModels.push({ id: model.id, type: model.type, file: assetPath(model.id, 'glb', 'models'), roots: model.roots,
            bindings: model.bindings, skin: model.player ? { variant: state.skinVariant, width: 64, height: 64, layers: true } : undefined });
        }
        const allGroups = g.Group.all.slice();
        for (const animation of g.Animation.all) {
          const hasTransforms = Object.values(animation.animators).some(a => a.type !== 'effect' && a.keyframes.length);
          const sampled = samples(animation, allGroups, state.sampleRate); baked.push({ name: animation.name, length: animation.length, hasTransforms, ...sampled });
          if (state.context) playerChannels.push({ name: animation.name, length: animation.length, loop: animation.loop, fps: state.sampleRate,
            root: { times: sampled.times, ...sampled.tracks.get(state.context.root) },
            channels: Object.fromEntries(Object.entries(state.context.groups).map(([name, uuid]) => [name, { times: sampled.times,
              ...sampled.tracks.get(uuid) }])) });
        }
      } finally {
        if (g.Project === previous.project) {
          g.Project.view_mode = previous.viewMode; g.Canvas.updateAll();
          previous.animation?.select(); previous.flags.forEach(([animation, playing]) => { animation.playing = playing; });
          g.Timeline.setTime(previous.time); if (g.Modes.animate) g.Animator.preview(false); else g.Animator.showDefaultPose();
          for (const [animation, keys] of originalAnimators) for (const key of Object.keys(animation.animators))
            if (!keys.has(key) && !animation.animators[key].keyframes.length) delete animation.animators[key];
        }
        if (studio.preview) studio.preview.suspended = false;
      }
        for (let i = 0; i < state.models.length; i++) {
          const built = scenes[i], animations = [];
          for (const clip of baked) {
            const tracks = [];
            for (const [uuid, object] of built.clones) {
              const values = clip.tracks.get(uuid); if (!values) continue;
              for (const channel of ['position', 'quaternion', 'scale']) {
                if (values[channel].some(v => !Number.isFinite(v))) throw new Error(`${clip.name}: a animação produziu valores inválidos.`);
                const Track = channel === 'quaternion' ? T.QuaternionKeyframeTrack : T.VectorKeyframeTrack;
                tracks.push(new Track(`${object.uuid}.${channel}`, clip.times, values[channel], T.InterpolateLinear));
              }
            }
            if (tracks.length && clip.hasTransforms) animations.push(new T.AnimationClip(clip.name, clip.length, tracks));
          }
          const bytes = await new Promise((resolve, reject) => {
            const timer = setTimeout(() => reject(new Error('O exportador GLB não respondeu em 30 segundos.')), 30000);
            try { new T.GLTFExporter().parse(built.scene, result => { clearTimeout(timer); resolve(result); },
              { binary: true, animations, trs: true, onlyVisible: false, embedImages: true, forcePowerOfTwoTextures: false, exportFaceColors: false }); }
            catch (error) { clearTimeout(timer); reject(error); }
          });
          if (!(bytes instanceof ArrayBuffer)) throw new Error('O exportador não retornou GLB binário.');
          files.set(packageModels[i].file, new Uint8Array(bytes));
        }
        const sounds = state.sounds.map(sound => {
          const [, mime, encoded] = /^data:(audio\/[^;]+);base64,(.*)$/.exec(sound.data);
          const extension = mime.includes('ogg') ? 'ogg' : mime.includes('mpeg') ? 'mp3' : 'wav';
          const filename = assetPath(sound.id, extension, 'audio'); files.set(filename, Uint8Array.from(g.atob(encoded), c => c.charCodeAt(0)));
          return { id: sound.id, file: filename, duration: sound.duration };
        });
        const envelope = schema => ({ schema, version: SCHEMA_VERSION, generator: `ShardBound Studio ${VERSION}`, project: state.id });
        files.set('studio.effects.json', stableJson({ ...envelope('shardbound.studio.effects'), renderContract: state.renderContract, definitions }));
        files.set('studio.events.json', stableJson({ ...envelope('shardbound.studio.events'), clips, sounds, locators }));
        files.set('studio.player-animations.json', stableJson({ ...envelope('shardbound.studio.player_animations'), context: state.context ? { variant: state.skinVariant, geometryIncluded: false,
          root: state.context.root, groups: state.context.groups } : null, clips: playerChannels }));
        files.set('studio.manifest.json', stableJson({ ...envelope('shardbound.studio.package'), editorVersion: g.Blockbench.version,
          unitsPerMeter: state.unitsPerMeter, models: packageModels, effects: 'studio.effects.json', events: 'studio.events.json',
          playerAnimations: 'studio.player-animations.json', projectileVisual: state.projectileVisual, runtimeIntegration: 'not-yet-implemented' }));
        return { files, report, state };
      } finally {
        if (studio.preview) studio.preview.suspended = false;
        for (const built of scenes) { built.scene.traverse(node => { node.geometry?.dispose(); }); for (const material of built.materials.values()) { material.map?.dispose(); material.dispose(); } }
        studio.exporting = false;
        if (previous.playing && g.Project === previous.project) g.Timeline.start();
      }
    },
    async write(directory) {
      const compiled = await exporter.compile();
      if (!g.isApp) throw new Error('Exportação de pasta disponível no Blockbench Desktop.');
      const path = studio.nativeRequire('path'), parent = path.resolve(directory);
      const nativeFS = studio.nativeRequire('fs', { scope: parent, message: 'Salvar o pacote ShardBound Studio somente na pasta de destino escolhida.' });
      if (!nativeFS) throw new Error('O Blockbench não concedeu acesso à pasta de exportação. Permita o acesso ao exportar novamente.');
      const fs = nativeFS.promises, slug = compiled.state.id.replace(/[^a-z0-9_.-]/g, '_');
      let target;
      for (let suffix = 0; suffix < 10000; suffix++) {
        target = path.join(parent, slug + (suffix ? `-${suffix}` : ''));
        try { await fs.mkdir(target); break; } catch (e) { if (e.code !== 'EEXIST') throw e; if (suffix === 9999) throw new Error('Não foi possível reservar uma pasta nova.'); }
      }
      try {
        for (const [relative, content] of compiled.files) {
          const destination = path.resolve(target, ...relative.split('/'));
          if (!destination.startsWith(target + path.sep)) throw new Error('Caminho de pacote inválido.');
          await fs.mkdir(path.dirname(destination), { recursive: true });
          await fs.writeFile(destination, typeof content === 'string' ? content : g.Buffer.from(content), { flag: 'wx' });
        }
      } catch (error) { throw new Error(`A exportação falhou. Arquivos parciais estão em ${target}. ${error.message}`); }
      return { directory: target, files: [...compiled.files.keys()], warnings: compiled.report.warnings };
    },
  };
  return exporter;
}


window.createEmbeddedStudio = () => { const studio = createStudio(window); studio.exporter = createExporter(window, studio); return studio; };
})();
