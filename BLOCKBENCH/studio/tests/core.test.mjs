import test from 'node:test';
import assert from 'node:assert/strict';
import { projectState, checkState, resourceId, defaultEffect, validateEffect, orderedEvents, eventIntervals, crossedEvents, stableJson, assetPath } from '../src/core.mjs';
import { particles, curve, trailTimes, impulse } from '../src/effects.mjs';
import { playerParts, skinFaces } from '../src/templates.mjs';

test('versioned project contract and namespace paths reject unsafe inputs', () => {
  assert.equal(checkState(projectState('ranged', 'test:bow')).projectileVisual, 'core:arrow');
  for (const id of ['..:secret', '.:secret', 'x:../secret', 'x:/abs', 'x:a//b', 'X:asset', 'x:a\\b', 'no_namespace']) assert.equal(resourceId(id), false);
  assert.throws(() => checkState({ ...projectState(), version: 999 }), /versão/);
  assert.equal(assetPath('mod:weapons/bow', 'glb', 'models'), 'models/mod/weapons/bow.glb');
});
test('every effect type has a valid authored definition; broken values are diagnosed', () => {
  for (const kind of ['particle', 'trail', 'flash', 'glow']) assert.deepEqual(validateEffect(defaultEffect(kind)), []);
  for (const changes of [{ lifetime: 0 }, { frames: 2 }, { burst: 4096, rate: 100 }, { direction: [1, NaN, 3] }, { opacity: 2 }, { kind: 'unknown' }])
    assert.ok(validateEffect({ ...defaultEffect(), ...changes }).length);
  for (const changes of [{ shapeSize: {} }, { curves: null }, { curves: { size: [null, null] } }]) assert.ok(validateEffect({ ...defaultEffect(), ...changes }).length);
});
test('particle preview is deterministic, bounded and time-based rather than frame-accumulated', () => {
  const e = { ...defaultEffect(), burst: 3, rate: 10, lifetime: 1, speed: 0, speedSpread: 0, gravity: [0, -10, 0], duration: 2 };
  const interval = { definition: e, start: 0, end: 2, continuous: false };
  const a = particles(interval, 0.5), b = particles(interval, 0.5);
  assert.deepEqual(a, b); assert.equal(a.length, 3); assert.equal(a[0].position[1], -1.25);
  assert.equal(particles(interval, -1).length, 0); assert.equal(particles(interval, 1.1).length, 0);
  assert.equal(particles({ ...interval, continuous: true }, 0.5).length, 8);
  const directional = { ...e, speed: 10, speedSpread: 3, direction: [1, 1, 1], spread: 0, gravity: [0, 0, 0] };
  for (const p of particles({ ...interval, definition: directional }, 0.5)) { assert.equal(p.position[0], p.position[1]); assert.equal(p.position[1], p.position[2]); }
});
test('curves, gradients and sprite animation evaluate at the requested age', () => {
  assert.equal(curve([{ age: 0, value: 1 }, { age: 0.5, value: 3 }, { age: 1, value: 0 }], 0.25), 2);
  assert.deepEqual(curve(null, 0.5, '#000000', '#ffffff', true), [0.5, 0.5, 0.5]);
  const effect = { ...defaultEffect(), frames: 4, columns: 4, fps: 10, burst: 1 };
  assert.equal(particles({ definition: effect, start: 0, end: 1, continuous: false }, 0.25)[0].frame, 2);
});
test('coincident events are stable and start/stop limits continuous emission', () => {
  const events = [{ id: 'start', type: 'particle', resource: 'x:p', action: 'start', time: 0.2, order: 2 },
    { id: 'stop', type: 'particle', resource: 'x:p', action: 'stop', time: 0.5, order: 3 },
    { id: 'first', type: 'sound', resource: 'x:s', action: 'burst', time: 0.2, order: 1 }];
  assert.deepEqual(orderedEvents(events).map(e => e.id), ['first', 'start', 'stop']);
  assert.equal(eventIntervals(events, [{ ...defaultEffect(), id: 'x:p' }], 2)[0].end, 0.5);
  assert.equal(eventIntervals([{ ...events[0], time: 0 }, { ...events[1], time: 5 }], [{ ...defaultEffect(), id: 'x:p' }], 10)[0].end, 1);
  assert.deepEqual(crossedEvents(events, 0.1, 0.6).map(e => e.id), ['first', 'start', 'stop']);
  assert.deepEqual(crossedEvents(events, 0.6, 0.1), []);
  const looping = [...events, { id: 'zero', time: 0, order: 0 }];
  assert.deepEqual(crossedEvents(looping, 0.4, 0.25, { looped: true, length: 1 }).map(e => e.id), ['stop', 'zero', 'first', 'start']);
});
test('trails and flash/glow have finite lifetimes', () => {
  const interval = { definition: defaultEffect('trail'), start: 1, end: 2 };
  assert.equal(trailTimes(interval, 0.5).length, 0); assert.equal(trailTimes(interval, 1.1).length, 24);
  assert.equal(trailTimes(interval, 3).length, 0); assert.equal(impulse(interval, 3), null);
});
test('Minecraft 64x64 skin template contains six parts and both arm variants with valid UVs', () => {
  assert.deepEqual(skinFaces([0, 0], [8, 8, 8]).up, [16, 8, 8, 0]);
  assert.deepEqual(skinFaces([0, 0], [8, 8, 8]).down, [24, 0, 16, 8]);
  for (const variant of ['wide', 'slim']) {
    const parts = playerParts(variant); assert.equal(parts.length, 6);
    const arm = parts.find(p => p.name === 'RightArm'); assert.equal(arm.to[0] - arm.from[0], variant === 'wide' ? 4 : 3);
    for (const part of parts) for (const uv of [part.uv, part.outerUv]) for (const rect of Object.values(skinFaces(uv, part.to.map((v, i) => v - part.from[i]))))
      assert.ok(rect.every(n => n >= 0 && n <= 64));
  }
});
test('companion JSON encoding has a stable order', () => {
  assert.equal(stableJson({ b: 1, a: { z: 2, x: 0 } }), stableJson({ a: { x: 0, z: 2 }, b: 1 }));
});
