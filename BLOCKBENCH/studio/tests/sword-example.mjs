// Authored example poses, not a procedural replacement for the native animation editor.
export async function buildSwordExample() {
  const s = ShardBoundStudio, sourceSound = structuredClone(s.state().sounds[0]);
  const model = s.createProject('melee', { id: 'studio_demo:crystal_sword', name: 'Espada de cristal — exemplo' });
  const root = s.find(model.roots[0]), blade = new Group({ name: 'Lâmina', origin: [-5, 16, 0] }).addTo(root).init();
  const grip = new Group({ name: 'Empunhadura', origin: [-5, 16, 0] }).addTo(root).init();
  const canvas = document.createElement('canvas'); canvas.width = canvas.height = 32;
  const ctx = canvas.getContext('2d'); ctx.fillStyle = '#65ccea'; ctx.fillRect(0, 0, 32, 32);
  ctx.fillStyle = '#c6f8ff'; ctx.fillRect(0, 0, 16, 16); ctx.fillStyle = '#305b88'; ctx.fillRect(16, 16, 16, 16);
  const texture = new Texture({ name: 'Cristal original', width: 32, height: 32 }).fromDataURL(canvas.toDataURL()).add();
  function cube(parent, name, from, to, uv) {
    const node = new Cube({ name, from, to }).addTo(parent).init();
    for (const face of Object.values(node.faces)) { face.texture = texture.uuid; face.uv = uv; }
    return node;
  }
  cube(blade, 'Lâmina de cristal', [-6, 17, -0.6], [-4, 28, 0.6], [0, 0, 16, 16]);
  cube(blade, 'Ponta', [-5.5, 28, -0.5], [-4.5, 30, 0.5], [0, 0, 16, 16]);
  cube(grip, 'Guarda', [-8, 16, -1], [-2, 17, 1], [16, 0, 32, 16]);
  cube(grip, 'Cabo', [-5.6, 12, -0.6], [-4.4, 16, 0.6], [16, 16, 32, 32]);
  cube(grip, 'Pomo', [-6, 11, -1], [-4, 12, 1], [16, 0, 32, 16]);
  const context = s.state().context;
  const sequences = {
    idle: { loop: 'loop', legs: [0, 0, 0], rightArm: [-10, -12, -10], leftArm: [0, 2, 0], body: [0, 0, 0], rootY: [0, 0, 0] },
    run: { loop: 'loop', legs: [-38, 38, -38], rightArm: [-25, -55, -25], leftArm: [38, -38, 38], body: [4, -4, 4], rootY: [0, 1, 0] },
    jump: { loop: 'once', legs: [0, -25, 0], rightArm: [-10, -80, -10], leftArm: [0, -65, 0], body: [0, 5, 0], rootY: [0, 9, 0] },
    attack: { loop: 'once', legs: [0, 8, 0], rightArm: [-15, -125, 35], leftArm: [0, -25, 15], body: [0, -20, 15], rootY: [0, 0, 0] },
  };
  for (const [name, pose] of Object.entries(sequences)) {
    const clip = s.createClip(name, 1, pose.loop, 24);
    for (const [part, uuid] of Object.entries(context.groups)) for (const [i, time] of [0, 0.5, 1].entries()) {
      const angle = part === 'RightArm' ? pose.rightArm[i] : part === 'LeftArm' ? pose.leftArm[i] : part.includes('Leg') ? pose.legs[i] * (part === 'LeftLeg' ? -1 : 1) : 0;
      clip.getBoneAnimator(s.find(uuid)).createKeyframe({ x: angle, y: part === 'Body' || part === 'Head' ? pose.body[i] : 0, z: 0 }, time, 'rotation', false, false);
      if (part === 'Body') clip.getBoneAnimator(s.find(context.root)).createKeyframe({ x: 0, y: pose.rootY[i], z: 0 }, time, 'position', false, false);
    }
  }
  const tip = s.addLocator('Ponta da lâmina', blade); tip.position = [-5, 30, 0];
  s.addEffect('particle', 'studio_demo:crystal_sparks', { burst: 18, rate: 0, lifetime: 0.6, size: 0.6, sizeEnd: 0, color: '#72eaff', colorEnd: '#ffffff', spread: 180, speed: 10, speedSpread: 2, gravity: [0, -12, 0] });
  s.addEffect('trail', 'studio_demo:crystal_trail', { color: '#44d9ff', colorEnd: '#c8f9ff', startOffset: [0, 1, 0], endOffset: [0, 14, 0], size: 1, sizeEnd: 0.7, duration: 0.65, trailLength: 0.25, opacity: 0.8 });
  s.addEffect('flash', 'studio_demo:crystal_flash', { color: '#d9ffff', colorEnd: '#65ccea', opacity: 0.7, duration: 0.16 });
  s.addEffect('glow', 'studio_demo:crystal_glow', { color: '#5adfff', colorEnd: '#c6f8ff', size: 4, sizeEnd: 1, duration: 0.4, intensity: 2 });
  await s.addSound('studio_demo:crystal_chime', sourceSound.data, 'authorial-chime.wav');
  const attack = Animation.all.find(a => a.name === 'attack'); attack.select();
  const event = (time, type, resource, target, action = 'burst', instance = '') => s.addEvent(attack, time, { type, resource: 'studio_demo:' + resource, target, action, instance, follow: ['flash', 'glow'].includes(type) });
  event(0.25, 'trail', 'crystal_trail', blade.uuid, 'start', 'blade_trail'); event(0.75, 'trail', 'crystal_trail', blade.uuid, 'stop', 'blade_trail');
  event(0.5, 'particle', 'crystal_sparks', tip.uuid); event(0.5, 'flash', 'crystal_flash', root.uuid); event(0.5, 'glow', 'crystal_glow', tip.uuid);
  event(0.5, 'sound', 'crystal_chime', tip.uuid);
  root.select(); Timeline.setTime(0.6); Animator.preview(false); s.refresh();
  return { report: s.exporter.validate(), saved: s.codec.compile({ raw: true }) };
}
