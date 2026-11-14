export const VERSION = '1.3.0';
export const FORMAT = 'shardbound_studio';
export const CHANNEL = 'shardbound_events';
export const SCHEMA_VERSION = 1;
export const MAX_PARTICLES = 4096;
export const TYPES = Object.freeze({ player: 'Player / skin', item: 'Item', melee: 'Arma corpo a corpo', ranged: 'Arma de longa distância',
  block: 'Bloco de cenário', armor: 'Armadura', monster: 'Monstro', effect: 'Partículas / VFX' });
export const EFFECT_TYPES = Object.freeze({ particle: 'Partículas', trail: 'Rastro', flash: 'Flash', glow: 'Brilho / luz' });
export const BODY_PARTS = ['Head', 'Body', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg'];
export const clone = value => structuredClone(value);
export function resourceId(value) {
  return typeof value === 'string' && /^[a-z0-9_.-]+:[a-z0-9_./-]+$/.test(value) &&
    value.replace(':', '/').split('/').every(part => part && part !== '.' && part !== '..');
}
export function assertId(value) { if (!resourceId(value)) throw new Error('Use um ID namespace:path, em minúsculas, sem . ou .. como segmentos.'); return value; }
export function assetPath(id, extension, directory) { assertId(id); const [namespace, name] = id.split(':'); return `${directory}/${namespace}/${name}.${extension}`; }
export function projectState(type = 'monster', id = 'custom:model') {
  if (!TYPES[type]) throw new Error('Tipo de projeto desconhecido.');
  assertId(id);
  return { schema: 'shardbound.studio.project', version: SCHEMA_VERSION, contentType: type, id, unitsPerMeter: 16,
    sampleRate: 60, skinVariant: 'wide', models: [], effects: [], sounds: [], materials: {}, context: null,
    projectileVisual: type === 'ranged' ? 'core:arrow' : null, nextEventOrder: 0,
    renderContract: { version: 1, filtering: 'nearest', colorSpace: 'srgb', particleLighting: 'unlit',
      transparency: 'alpha-or-additive', gravitySpace: 'emitter-local', particleMotion: 'analytic-linear-drag',
      spriteOrientation: 'camera-facing', trail: 'world-space-ribbon', glow: 'additive-sprite-and-point-light' } };
}
export function checkState(state) {
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
export function defaultEffect(kind = 'particle', id = 'custom:effect') {
  if (!EFFECT_TYPES[kind]) throw new Error('Tipo de efeito desconhecido.');
  assertId(id);
  return { id, kind, duration: 1, lifetime: 0.7, burst: 16, rate: 16, seed: 1, shape: 'point', shapeSize: [1, 1, 1],
    speed: 5, speedSpread: 1, direction: [0, 1, 0], spread: 45, gravity: [0, -9.81, 0], drag: 0,
    size: 0.5, sizeEnd: 0, color: '#ffffff', colorEnd: '#ff9b33', opacity: 1, opacityEnd: 0,
    rotation: 0, angularVelocity: 0, texture: '', blend: 'alpha', columns: 1, rows: 1, frames: 1, fps: 12,
    curves: {}, trailLength: 0.35, trailSamples: 24, width: 1, startOffset: [0, 0, 0], endOffset: [0, 5, 0],
    intensity: 2, distance: 12, follow: false };
}
export function validateEffect(effect) {
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
export function stableJson(value) {
  const sort = v => Array.isArray(v) ? v.map(sort) : v && typeof v === 'object'
    ? Object.fromEntries(Object.keys(v).sort().filter(k => v[k] !== undefined).map(k => [k, sort(v[k])])) : v;
  return JSON.stringify(sort(value), null, 2) + '\n';
}
export function orderedEvents(events) { return events.slice().sort((a, b) => a.time - b.time || (a.order ?? 0) - (b.order ?? 0) || String(a.id).localeCompare(String(b.id))); }
export function eventIntervals(events, definitions, end) {
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
export function crossedEvents(events, previous, now, { length = 0, looped = false, initial = false } = {}) {
  const sorted = orderedEvents(events);
  if (initial) return sorted.filter(e => e.time <= now && e.time >= 0);
  if (looped && length > 0) return sorted.filter(e => e.time > previous && e.time <= length).concat(sorted.filter(e => e.time <= now));
  if (now < previous) return [];
  return sorted.filter(e => e.time > previous && e.time <= now);
}
