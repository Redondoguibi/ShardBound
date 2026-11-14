import { BODY_PARTS } from './core.mjs';
// Original geometry, standard 64x64 skin layout. No third-party images are included.
export function playerParts(variant = 'wide') {
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
export function skinFaces([u, v], [w, h, d]) {
  return {
    east: [u, v + d, u + d, v + d + h],
    north: [u + d, v + d, u + d + w, v + d + h],
    west: [u + d + w, v + d, u + 2 * d + w, v + d + h],
    south: [u + 2 * d + w, v + d, u + 2 * d + 2 * w, v + d + h],
    up: [u + d + w, v + d, u + d, v],
    down: [u + d + 2 * w, v, u + d + w, v + d],
  };
}
export const defaultClipNames = type => type === 'player' ? ['idle', 'walk', 'run', 'jump']
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
export function neutralSkin(g) { return neutralSkinCanvas(g).toDataURL('image/png'); }
export function makePlayer(g, { variant = 'wide', reference = false, rootName = 'Player' } = {}) {
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
