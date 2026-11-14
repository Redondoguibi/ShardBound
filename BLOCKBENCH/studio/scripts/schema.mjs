import { mkdir, writeFile } from 'node:fs/promises';
import { TYPES, defaultEffect, stableJson } from '../src/core.mjs';
const ref = name => ({ $ref: `#/$defs/${name}` });
const str = { type: 'string' }, num = { type: 'number' }, bool = { type: 'boolean' };
const array = items => ({ type: 'array', items });
const object = (properties, required = Object.keys(properties), additionalProperties = false) => ({ type: 'object', properties, required, additionalProperties });
const id = { type: 'string', pattern: '^[a-z0-9_.-]+:[a-z0-9_./-]+$', not: { pattern: '(^|[:/])\\.\\.?([:/]|$)' } };
const vec = { type: 'array', items: num, minItems: 3, maxItems: 3 };
const nonnegative = { type: 'number', minimum: 0 }, positive = { type: 'number', exclusiveMinimum: 0 };
const color = { type: 'string', pattern: '^#[a-fA-F0-9]{6}$' };
const bodyNames = ['Head', 'Body', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg'];
const bodyGroups = object(Object.fromEntries(bodyNames.map(name => [name, str])));
const numericCurve = { type: 'array', minItems: 2, maxItems: 64, items: object({ age: { type: 'number', minimum: 0, maximum: 1 }, value: num }) };
const colorCurve = { ...numericCurve, items: object({ age: { type: 'number', minimum: 0, maximum: 1 }, value: color }) };
const effectProperties = Object.fromEntries(Object.entries(defaultEffect()).map(([key, value]) => [key, Array.isArray(value) ? vec : typeof value === 'number' ? num : typeof value === 'boolean' ? bool : str]));
Object.assign(effectProperties, { id, kind: { enum: ['particle', 'trail', 'flash', 'glow'] }, duration: positive, lifetime: positive,
  burst: { type: 'integer', minimum: 0, maximum: 4096 }, rate: { ...nonnegative, maximum: 4096 }, seed: { type: 'integer', minimum: 0, maximum: 4294967295 },
  shape: { enum: ['point', 'box', 'sphere', 'cone'] }, blend: { enum: ['alpha', 'additive'] }, color, colorEnd: color,
  size: nonnegative, sizeEnd: nonnegative, opacity: { type: 'number', minimum: 0, maximum: 1 }, opacityEnd: { type: 'number', minimum: 0, maximum: 1 },
  columns: { type: 'integer', minimum: 1, maximum: 256 }, rows: { type: 'integer', minimum: 1, maximum: 256 }, frames: { type: 'integer', minimum: 1, maximum: 65536 },
  curves: object({ size: numericCurve, opacity: numericCurve, rotation: numericCurve, color: colorCurve }, []),
  trailLength: positive, trailSamples: { type: 'integer', minimum: 2, maximum: 240 } });
const binding = object({ group: str, target: { enum: bodyNames }, referenceGroup: str });
const model = object({ id, type: { enum: Object.keys(TYPES) }, file: str, roots: array(str), bindings: array(binding),
  skin: object({ variant: { enum: ['wide', 'slim'] }, width: { const: 64 }, height: { const: 64 }, layers: { const: true } }) }, ['id', 'type', 'file', 'roots', 'bindings']);
const renderContract = object({ version: { const: 1 }, filtering: { const: 'nearest' }, colorSpace: { const: 'srgb' }, particleLighting: { const: 'unlit' },
  transparency: { const: 'alpha-or-additive' }, gravitySpace: { const: 'emitter-local' }, particleMotion: { const: 'analytic-linear-drag' }, spriteOrientation: { const: 'camera-facing' },
  trail: { const: 'world-space-ribbon' }, glow: { const: 'additive-sprite-and-point-light' } });
const event = object({ id: str, type: { enum: ['particle', 'trail', 'flash', 'glow', 'sound'] }, resource: id, action: { enum: ['burst', 'start', 'stop'] },
  target: str, offset: vec, follow: bool, instance: str, volume: { type: 'number', minimum: 0, maximum: 1 }, pitch: { type: 'number', minimum: 0.1, maximum: 4 }, loop: bool,
  order: { type: 'integer', minimum: 0 }, time: nonnegative, frame: { type: 'integer', minimum: 0 }, fps: positive });
const track = object({ times: array(nonnegative), position: array(num), quaternion: array(num), scale: array(num) });
const envelope = (schema, properties) => object({ schema: { const: schema }, version: { const: 1 }, generator: str, project: id, ...properties });
const definitions = {
  effect: object(effectProperties), event, track, renderContract,
  package: envelope('shardbound.studio.package', { editorVersion: str, unitsPerMeter: positive, models: array(model), effects: str, events: str, playerAnimations: str,
    projectileVisual: { anyOf: [id, { type: 'null' }] }, runtimeIntegration: { const: 'not-yet-implemented' } }),
  effects: envelope('shardbound.studio.effects', { renderContract: ref('renderContract'), definitions: array(ref('effect')) }),
  events: envelope('shardbound.studio.events', { clips: array(object({ name: str, length: positive, loop: { enum: ['once', 'loop', 'hold'] }, fps: positive, events: array(ref('event')) })),
    sounds: array(object({ id, file: str, duration: positive })), locators: array(object({ uuid: str, name: str, parent: { type: ['string', 'null'] }, position: vec, rotation: vec })) }),
  playerAnimations: envelope('shardbound.studio.player_animations', { context: { anyOf: [{ type: 'null' }, object({ variant: { enum: ['wide', 'slim'] }, geometryIncluded: { const: false }, root: str, groups: bodyGroups })] },
    clips: array(object({ name: str, length: positive, loop: { enum: ['once', 'loop', 'hold'] }, fps: positive, root: ref('track'), channels: object(Object.fromEntries(bodyNames.map(name => [name, ref('track')]))) })) }),
};
const schema = { $schema: 'http://json-schema.org/draft-07/schema#', $id: 'https://shardbound.local/schemas/studio-v1',
  title: 'ShardBound Studio companion contracts v1', $defs: definitions, oneOf: ['package', 'effects', 'events', 'playerAnimations'].map(ref) };
const directory = new URL('../schemas/', import.meta.url); await mkdir(directory, { recursive: true });
await writeFile(new URL('studio-v1.schema.json', directory), stableJson(schema));
console.log('Generated schemas/studio-v1.schema.json');
