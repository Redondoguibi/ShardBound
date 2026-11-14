// Original authoring fixture executed inside an isolated, real Blockbench renderer.
export async function buildFixture() {
  const s = window.ShardBoundStudio;
  const ranged = s.createProject('ranged', { id: 'studio_demo:bow', name: 'Studio authoring validation', variant: 'slim' });
  const canvas = document.createElement('canvas'); canvas.width = canvas.height = 32;
  const ctx = canvas.getContext('2d'); ctx.fillStyle = '#7c4e25'; ctx.fillRect(0, 0, 32, 32);
  ctx.fillStyle = '#ffe5a3'; ctx.fillRect(0, 0, 16, 16);
  const wood = new Texture({ name: 'Authorial wood', width: 32, height: 32 }).fromDataURL(canvas.toDataURL()).add();
  ctx.fillStyle = '#87bace'; ctx.fillRect(0, 0, 32, 32);
  const metal = new Texture({ name: 'Authorial metal', width: 32, height: 32 }).fromDataURL(canvas.toDataURL()).add();
  function cube(root, name, from = [0, 0, 0], to = [3, 6, 3]) {
    const node = new Cube({ name, from, to }).addTo(root).init();
    Canvas.face_order.forEach((face, i) => { node.faces[face].texture = i % 2 ? metal.uuid : wood.uuid; node.faces[face].uv = [0, 0, 16, 16]; });
    return node;
  }
  cube(s.find(ranged.roots[0]), 'Bow', [-6, 12, -1], [-4, 22, 1]);
  const melee = s.addModel('melee', { id: 'studio_demo:sword' });
  const blade = cube(s.find(melee.roots[0]), 'Blade', [-5.5, 12, -1], [-4.5, 24, 1]);
  blade.faces.north.texture = null;
  cube(s.find(melee.roots[0]), 'Guard', [-8, 13, -2], [-2, 14, 2]);
  cube(s.find(melee.roots[0]), 'Handle', [-5.5, 9, -0.5], [-4.5, 13, 0.5]);
  cube(s.find(melee.roots[0]), 'Pommel', [-6, 8, -1], [-4, 9, 1]);
  const armor = s.addModel('armor', { id: 'studio_demo:armor' });
  armor.roots.forEach((uuid, index) => cube(s.find(uuid), 'Overlay_' + index));
  const monster = s.addModel('monster', { id: 'studio_demo:monster' });
  const jaw = new Group({ name: 'Jaw', origin: [0, 3, 0] }).addTo(s.find(monster.roots[0])).init(); cube(jaw, 'Monster jaw');
  const related = s.addModel('monster', { id: 'studio_demo:related_monster' }); cube(s.find(related.roots[0]), 'Independent monster');
  const block = s.addModel('block', { id: 'studio_demo:block' });
  for (const face of Object.values(s.find(block.roots[0]).children[0].faces)) face.texture = wood.uuid;
  const item = s.addModel('item', { id: 'studio_demo:item' });
  const mesh = new Mesh({ name: 'Triangle mesh', vertices: {}, faces: {} }).addTo(s.find(item.roots[0])).init();
  const vertices = mesh.addVertices([0, 0, 0], [4, 0, 0], [0, 4, 0], [0, 0, 4]);
  mesh.addFaces(new MeshFace(mesh, { vertices: vertices.slice(0, 3), texture: wood.uuid, uv: Object.fromEntries(vertices.slice(0, 3).map((v, i) => [v, [[0, 0], [16, 0], [0, 16]][i]])) }),
    new MeshFace(mesh, { vertices: [vertices[0], vertices[3], vertices[1]], texture: metal.uuid, uv: Object.fromEntries([vertices[0], vertices[3], vertices[1]].map((v, i) => [v, [[0, 0], [16, 0], [0, 16]][i]])) }));
  const player = s.addModel('player', { id: 'studio_demo:player' });
  s.addModel('effect', { id: 'studio_demo:effects' });
  const context = s.state().context, anchor = s.addLocator('blade_tip', s.find(melee.roots[0])); anchor.position = [-5, 24, 0];
  const clips = ['run', 'jump', 'attack', 'fire'].map((name, clipIndex) => {
    const clip = s.createClip(name, 1, 'once', 24);
    for (const [part, uuid] of Object.entries(context.groups)) {
      const group = s.find(uuid), amount = part.includes('Leg') ? 35 : part.includes('Arm') ? 65 : 8;
      const animator = clip.getBoneAnimator(group);
      animator.createKeyframe({ x: 0, y: 0, z: 0 }, 0, 'rotation', false, false);
      animator.createKeyframe({ x: amount * (part.startsWith('Left') ? -1 : 1), y: clipIndex * 10, z: 0 }, 0.5, 'rotation', false, false);
      animator.createKeyframe({ x: 0, y: 0, z: 0 }, 1, 'rotation', false, false);
    }
    clip.getBoneAnimator(s.find(context.root)).createKeyframe({ x: 0, y: 0, z: 0 }, 0, 'position', false, false);
    clip.getBoneAnimator(s.find(context.root)).createKeyframe({ x: 0, y: 4, z: 0 }, 0.5, 'position', false, false);
    clip.getBoneAnimator(s.find(context.root)).createKeyframe({ x: 0, y: 0, z: 0 }, 1, 'position', false, false);
    clip.getBoneAnimator(jaw).createKeyframe({ x: 25, y: 0, z: 0 }, 0.5, 'rotation', false, false);
    clip.getBoneAnimator(jaw).createKeyframe({ x: 1, y: 1, z: 1 }, 0, 'scale', false, false);
    clip.getBoneAnimator(jaw).createKeyframe({ x: 2, y: 1, z: 1 }, 0.5, 'scale', false, false);
    clip.getBoneAnimator(jaw).createKeyframe({ x: 0, y: 0, z: 0 }, 0, 'position', false, false).interpolation = 'step';
    clip.getBoneAnimator(jaw).createKeyframe({ x: 6, y: 0, z: 0 }, 0.5, 'position', false, false);
    clip.getBoneAnimator(s.find(armor.roots[0])).createKeyframe({ x: 0, y: 5, z: 0 }, 0.5, 'rotation', false, false);
    return clip;
  });
  ctx.clearRect(0, 0, 32, 32); ctx.fillStyle = '#ffb23f'; ctx.fillRect(0, 0, 16, 32); ctx.fillStyle = '#87dbff'; ctx.fillRect(16, 0, 16, 32);
  const atlas = new Texture({ name: 'Authorial effect atlas', width: 32, height: 32 }).fromDataURL(canvas.toDataURL()).add();
  s.addEffect('particle', 'studio_demo:sparks', { burst: 20, size: 2, sizeEnd: 1, lifetime: 1, texture: atlas.uuid, columns: 2, frames: 2, fps: 4 });
  s.addEffect('trail', 'studio_demo:trail', { startOffset: [0, 0, 0], endOffset: [0, 8, 0], color: '#00ccff' });
  s.addEffect('flash', 'studio_demo:flash', { duration: 0.5 });
  s.addEffect('glow', 'studio_demo:glow', { size: 8, sizeEnd: 4, duration: 0.6 });
  const sampleRate = 22050, count = Math.round(sampleRate * 0.3), buffer = new ArrayBuffer(44 + count * 2), view = new DataView(buffer);
  const ascii = (offset, text) => [...text].forEach((letter, i) => view.setUint8(offset + i, letter.charCodeAt(0)));
  ascii(0, 'RIFF'); view.setUint32(4, buffer.byteLength - 8, true); ascii(8, 'WAVE'); ascii(12, 'fmt '); view.setUint32(16, 16, true);
  view.setUint16(20, 1, true); view.setUint16(22, 1, true); view.setUint32(24, sampleRate, true); view.setUint32(28, sampleRate * 2, true);
  view.setUint16(32, 2, true); view.setUint16(34, 16, true); ascii(36, 'data'); view.setUint32(40, count * 2, true);
  for (let i = 0; i < count; i++) view.setInt16(44 + i * 2, Math.sin(i * Math.PI * 2 * 440 / sampleRate) * 3000 * (1 - i / count), true);
  await s.addSound('studio_demo:chime', 'data:audio/wav;base64,' + btoa(String.fromCharCode(...new Uint8Array(buffer))), 'authorial-chime.wav');
  const clip = clips[2]; clip.select();
  for (const [type, resource, target] of [['particle', 'sparks', anchor.uuid], ['trail', 'trail', s.find(melee.roots[0]).uuid], ['flash', 'flash', s.find(melee.roots[0]).uuid], ['glow', 'glow', anchor.uuid], ['sound', 'chime', anchor.uuid]])
    s.addEvent(clip, 0, { type, resource: 'studio_demo:' + resource, target, action: type === 'sound' ? 'burst' : 'start', instance: type });
  s.addEvent(clip, 0.4, { type: 'particle', resource: 'studio_demo:sparks', target: anchor.uuid, action: 'stop', instance: 'particle' });
  s.refresh(); Timeline.setTime(0.25); Animator.preview(false);
  return { models: s.state().models.length, clips: clips.length, anchor: anchor.uuid, weapon: melee.roots[0], armor: armor.roots[0], player: player.id, events: s.events().length,
    report: s.exporter.validate(), preview: { ...s.preview.stats }, context: context.groups, arrow: s.state().projectileVisual };
}
