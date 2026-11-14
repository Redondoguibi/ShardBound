import { z } from 'zod';

const id = z.string().min(1).max(200);
const name = z.string().min(1).max(160);
const n = z.number().finite().min(-1e6).max(1e6);
const vec = z.array(n).length(3);
const resource = id.regex(/^[a-z0-9_.-]+:[a-z0-9_./-]+$/).refine(v => v.replace(':', '/').split('/').every(p => p && p !== '.' && p !== '..'));
const asset = z.string().min(1).max(240).refine(v => v.startsWith('assets/') && !v.includes('\\') && !v.includes(':') && v.split('/').every(p => p && p !== '.' && p !== '..'));
const guard = { project_id: id, expected_revision: z.number().int().nonnegative() };
const type = z.enum(['player', 'item', 'melee', 'ranged', 'block', 'armor', 'monster', 'effect']);
const kind = z.enum(['particle', 'trail', 'flash', 'glow']);
const part = z.enum(['Head', 'Body', 'LeftArm', 'RightArm', 'LeftLeg', 'RightLeg']);
const color = z.string().regex(/^#[a-f0-9]{6}$/i);
const curve = value => z.array(z.object({ age: z.number().min(0).max(1), value }).strict()).min(2).max(64);
// A complete typed patch, with final cross-field validation by Studio itself.
export const effectPatch = z.object({
  id: resource.optional(), duration: z.number().min(.001).max(3600).optional(), lifetime: z.number().min(.001).max(120).optional(),
  burst: z.number().int().min(0).max(4096).optional(), rate: z.number().min(0).max(4096).optional(), seed: z.number().int().min(0).max(0xffffffff).optional(),
  shape: z.enum(['point', 'box', 'sphere', 'cone']).optional(), shapeSize: vec.optional(), speed: z.number().min(0).max(10000).optional(),
  speedSpread: z.number().min(0).max(10000).optional(), direction: vec.optional(), spread: z.number().min(0).max(180).optional(), gravity: vec.optional(), drag: z.number().min(0).max(100).optional(),
  size: z.number().min(0).max(10000).optional(), sizeEnd: z.number().min(0).max(10000).optional(), color: color.optional(), colorEnd: color.optional(),
  opacity: z.number().min(0).max(1).optional(), opacityEnd: z.number().min(0).max(1).optional(), rotation: z.number().min(-36000).max(36000).optional(), angularVelocity: z.number().min(-36000).max(36000).optional(),
  texture: z.string().max(200).optional(), blend: z.enum(['alpha', 'additive']).optional(), columns: z.number().int().min(1).max(256).optional(), rows: z.number().int().min(1).max(256).optional(),
  frames: z.number().int().min(1).max(65536).optional(), fps: z.number().min(0).max(240).optional(),
  curves: z.object({ size: curve(z.number().finite().min(0)).optional(), opacity: curve(z.number().min(0).max(1)).optional(), rotation: curve(n).optional(), color: curve(color).optional() }).strict().optional(),
  trailLength: z.number().min(.001).max(30).optional(), trailSamples: z.number().int().min(2).max(240).optional(), width: z.number().min(.001).max(10000).optional(),
  startOffset: vec.optional(), endOffset: vec.optional(), intensity: z.number().min(0).max(10000).optional(), distance: z.number().min(0).max(10000).optional(), follow: z.boolean().optional(),
}).strict();
const eventPatch = z.object({ type: z.enum(['particle', 'trail', 'flash', 'glow', 'sound']).optional(), action: z.enum(['burst', 'start', 'stop']).optional(), resource: resource.optional(),
  target: z.string().max(200).optional(), offset: vec.optional(), follow: z.boolean().optional(), instance: z.string().max(200).optional(), volume: z.number().min(0).max(1).optional(), pitch: z.number().min(.1).max(4).optional(), loop: z.boolean().optional() }).strict();
const tool = (description, shape, read = false) => ({ description, read, schema: z.object(shape).strict() });
export const studioTools = {
  studio_read: tool('Read Studio capabilities, version, metadata, models/bindings, effects, sound metadata, native clips, timeline events, locators and validation. Never returns audio/image blobs. Optional clip UUID includes paginated native transform keyframes.', {
    clip: id.optional(), offset: z.number().int().min(0).default(0), limit: z.number().int().min(1).max(200).default(100),
  }, true),
  studio_create_project: tool('Create a new Studio project tab with a real content template; existing tabs stay open. Requires ShardBound Studio 1.1+ loaded.', { type, id: resource, name, variant: z.enum(['wide', 'slim']).default('wide') }),
  studio_project_settings: tool('Update Studio project ID, units per meter, export sampling and shared projectile visual. Does not convert formats or change gameplay.', { ...guard, id: resource.optional(), unitsPerMeter: z.number().min(.001).max(100000).optional(), sampleRate: z.number().min(1).max(240).optional(), projectileVisual: resource.nullable().optional() }),
  studio_model: tool('Manage independent Studio models. add requires type and id; update requires model and id/name; bind requires model and body part; select/remove require model. Removal rejects referenced animation/effect targets.', { ...guard, action: z.enum(['add', 'update', 'select', 'bind', 'remove']), model: resource.optional(), type: type.optional(), id: resource.optional(), name: name.optional(), part: part.optional(), root: id.optional() }),
  studio_locator: tool('Create/update/remove an attachment locator. Create needs name and parent group UUID; update/remove need locator UUID. Position uses native editor units. Referenced locators cannot be deleted.', { ...guard, action: z.enum(['add', 'update', 'remove']), id: id.optional(), name: name.optional(), parent: id.optional(), position: vec.optional() }),
  studio_clip: tool('Create/update/select/remove a native Studio animation clip. Create needs name; other actions need clip UUID. Length 0.001–1000s, snapping 1–120 FPS. Shortening past existing transform/events is rejected.', { ...guard, action: z.enum(['add', 'update', 'select', 'remove']), clip: id.optional(), name: name.optional(), length: z.number().min(.001).max(1000).optional(), loop: z.enum(['once', 'loop', 'hold']).optional(), fps: z.number().int().min(1).max(120).optional() }),
  studio_keyframes: tool('Atomically add/update/remove native group position/rotation/scale keyframes. Numeric vectors only. Rotation in degrees. Each upsert needs group, channel, time, value; optional id targets an existing keyframe, otherwise same group/channel/time is replaced. Native interpolation and Bezier handles supported.', { ...guard, clip: id,
    remove: z.array(id).max(500).default([]), keyframes: z.array(z.object({ id: id.optional(), group: id, channel: z.enum(['position', 'rotation', 'scale']), time: z.number().min(0).max(1000), value: vec,
      interpolation: z.enum(['linear', 'step', 'catmullrom', 'bezier']).default('linear'), bezier_linked: z.boolean().optional(), bezier_left_time: vec.optional(), bezier_left_value: vec.optional(), bezier_right_time: vec.optional(), bezier_right_value: vec.optional() }).strict()).max(500).default([]) }),
  studio_effect: tool('Add/update/remove a real particle/trail/flash/glow definition. Add needs kind and id; update/remove need id. values exposes all emitter, atlas, life curves, ribbon, flash and glow parameters. Texture is an existing project texture UUID. Remove rejects timeline references.', { ...guard, action: z.enum(['add', 'update', 'remove']), id: resource, kind: kind.optional(), values: effectPatch.default({}) }),
  studio_event: tool('Add/update/remove a synchronized Studio event. Add needs clip UUID, time in seconds and values.type/resource. Update/remove need the composite event id returned by studio_read. Values supports action, attachment UUID, offset, follow, instance and audio settings.', { ...guard, action: z.enum(['add', 'update', 'remove']), clip: id.optional(), id: id.optional(), time: z.number().min(0).max(1000).optional(), values: eventPatch.default({}) }),
  studio_sound: tool('Import a real WAV/OGG/MP3 from assets/ or remove an unreferenced sound. Import decodes bytes in Blockbench, maximum 24 MiB. No fabricated sound placeholders.', { ...guard, action: z.enum(['import', 'remove']), id: resource, path: asset.optional() }),
  studio_skin: tool('Import a real 64x64 PNG skin from assets/ or an existing texture UUID, or set outer skin layer visibility. Does not change wide/slim geometry after creation.', { ...guard, action: z.enum(['import', 'layers']), path: asset.optional(), texture: id.optional(), visible: z.boolean().optional() }),
  studio_material: tool('Set a Studio texture material, preserving native painting and GLB export. Only OPAQUE or MASK with optional cutoff and double-sided rendering.', { ...guard, texture: id, alphaMode: z.enum(['OPAQUE', 'MASK']), alphaCutoff: z.number().min(0).max(1).default(.5), doubleSided: z.boolean().default(true) }),
  paint_texture: tool('Paint/erase real texture pixels atomically with native bitmap Undo. Rectangles use integer pixel coordinates and #RRGGBB or #RRGGBBAA. Works in Generic Model and Studio, including skins and effect sprite atlases.', { ...guard, texture: id, rectangles: z.array(z.object({ x: z.number().int().min(0).max(2047), y: z.number().int().min(0).max(2047), width: z.number().int().min(1).max(2048), height: z.number().int().min(1).max(2048), color: z.string().regex(/^#[a-f0-9]{6}([a-f0-9]{2})?$/i), erase: z.boolean().default(false) }).strict()).min(1).max(10000) }),
  studio_timeline: tool('Select a Studio clip, seek, play or pause the actual native timeline and synchronized FX/audio. time is seconds; speed is percent, 10–400. Scrubbing does not replay sounds.', { ...guard, action: z.enum(['seek', 'play', 'pause']), clip: id.optional(), time: z.number().min(0).max(1000).optional(), speed: z.number().min(10).max(400).optional() }),
  studio_validate: tool('Validate the complete active Studio project against its real authoring/export contract. Returns concrete errors and warnings.', {}, true),
  studio_export: tool('Validate and export a complete Studio package: independent animated GLBs, embedded model textures, sprite PNGs, audio, versioned effects/events/player-animation companions. Saves to a unique new folder under outputs/ or the supplied assets/ directory; never overwrites mods/scripts. Returns local paths, not a download URL.', { ...guard, directory: asset.optional() }),
};
