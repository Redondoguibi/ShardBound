import { MAX_PARTICLES } from './core.mjs';
export const clamp = (x, a = 0, b = 1) => Math.max(a, Math.min(b, x));
export const mix = (a, b, t) => a + (b - a) * t;
export function random(seed) { let n = seed >>> 0; return () => { n += 0x6d2b79f5; let t = n; t = Math.imul(t ^ t >>> 15, t | 1); t ^= t + Math.imul(t ^ t >>> 7, t | 61); return ((t ^ t >>> 14) >>> 0) / 4294967296; }; }
export function color(value) { return [1, 3, 5].map(i => parseInt(value.slice(i, i + 2), 16) / 255); }
export function curve(points, age, start, end, isColor = false) {
  const list = points?.length ? points.slice().sort((a, b) => a.age - b.age) : [{ age: 0, value: start }, { age: 1, value: end }];
  let a = list[0], b = list.at(-1);
  if (age <= a.age) b = a;
  else if (age >= b.age) a = b;
  else for (let i = 1; i < list.length; i++) if (age <= list[i].age) { a = list[i - 1]; b = list[i]; break; }
  const t = a.age === b.age ? 0 : clamp((age - a.age) / (b.age - a.age));
  return isColor ? color(a.value).map((v, i) => mix(v, color(b.value)[i], t)) : mix(a.value, b.value, t);
}
function unit(v) { const length = Math.hypot(...v); return length ? v.map(n => n / length) : [0, 1, 0]; }
function cross(a, b) { return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]; }
function direction(axis, spread, r) {
  const n = unit(axis), u = unit(cross(Math.abs(n[1]) < 0.99 ? [0, 1, 0] : [1, 0, 0], n)), v = cross(n, u);
  const cos = mix(1, Math.cos(spread * Math.PI / 180), r()), sin = Math.sqrt(Math.max(0, 1 - cos * cos)), angle = r() * Math.PI * 2;
  return n.map((x, i) => x * cos + sin * (Math.cos(angle) * u[i] + Math.sin(angle) * v[i]));
}
function spawnPosition(effect, r) {
  if (effect.shape === 'point') return [0, 0, 0];
  if (effect.shape === 'box') return effect.shapeSize.map(n => (r() - 0.5) * n);
  const v = direction([0, 1, 0], 180, r), radius = Math.cbrt(r());
  if (effect.shape === 'sphere') return v.map((n, i) => n * radius * effect.shapeSize[i] / 2);
  const angle = r() * Math.PI * 2, height = Math.cbrt(r()), radial = Math.sqrt(r()) * height;
  return [Math.cos(angle) * radial * effect.shapeSize[0] / 2, height * effect.shapeSize[1], Math.sin(angle) * radial * effect.shapeSize[2] / 2];
}
// Analytic time evaluation: scrubbing, frame drops and deterministic replays have no accumulated simulation drift.
export function particles(interval, time) {
  const { definition: effect, start, end, continuous } = interval;
  if (time < start || time > end + effect.lifetime) return [];
  const records = [], elapsed = time - start;
  const births = [];
  for (let i = 0; i < effect.burst; i++) births.push([i, 0]);
  if (continuous && effect.rate > 0) {
    const first = Math.max(1, Math.ceil((elapsed - effect.lifetime) * effect.rate));
    const last = Math.floor((Math.min(time, end) - start) * effect.rate + 1e-8);
    for (let i = first; i <= last; i++) births.push([effect.burst + i, i / effect.rate]);
  }
  for (const [index, birth] of births) {
    const age = elapsed - birth;
    if (age < 0 || age >= effect.lifetime) continue;
    const r = random((effect.seed + Math.imul(index + 1, 2654435761)) >>> 0);
    const origin = spawnPosition(effect, r), heading = direction(effect.direction, effect.spread, r);
    const speed = Math.max(0, effect.speed + (r() * 2 - 1) * effect.speedSpread), velocity = heading.map(n => n * speed);
    const d = effect.drag, velocityTime = d > 0 ? (1 - Math.exp(-d * age)) / d : age;
    const gravityTime = d > 0 ? (age - velocityTime) / d : age * age / 2;
    const position = origin.map((n, i) => n + velocity[i] * velocityTime + effect.gravity[i] * gravityTime);
    const progress = age / effect.lifetime;
    records.push({ index, birth: start + birth, age, position,
      size: curve(effect.curves.size, progress, effect.size, effect.sizeEnd),
      opacity: curve(effect.curves.opacity, progress, effect.opacity, effect.opacityEnd),
      color: curve(effect.curves.color, progress, effect.color, effect.colorEnd, true),
      rotation: curve(effect.curves.rotation, progress, effect.rotation, effect.rotation + effect.angularVelocity * effect.lifetime) * Math.PI / 180,
      frame: Math.min(effect.frames - 1, Math.floor(age * effect.fps) % effect.frames) });
  }
  if (records.length > MAX_PARTICLES) throw new Error('Efeito excedeu o limite validado de partículas.');
  return records;
}
export function trailTimes(interval, time) {
  if (time < interval.start || time > interval.end + interval.definition.trailLength) return [];
  const end = Math.min(time, interval.end), start = Math.max(interval.start, time - interval.definition.trailLength);
  if (start >= end) return [];
  const count = interval.definition.trailSamples;
  return Array.from({ length: count }, (_, i) => mix(start, end, i / (count - 1)));
}
export function impulse(interval, time) {
  const age = (time - interval.start) / interval.definition.duration;
  if (age < 0 || time >= interval.end) return null;
  const e = interval.definition;
  return { size: curve(e.curves.size, age, e.size, e.sizeEnd), opacity: curve(e.curves.opacity, age, e.opacity, e.opacityEnd),
    color: curve(e.curves.color, age, e.color, e.colorEnd, true), intensity: e.intensity * (1 - age) };
}
