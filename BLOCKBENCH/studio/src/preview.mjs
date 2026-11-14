import { CHANNEL, eventIntervals, crossedEvents } from './core.mjs';
import { particles, trailTimes, impulse, curve, clamp } from './effects.mjs';
import { faceTextures } from './materials.mjs';

export function createPreview(g, studio) {
  const T = g.THREE, root = new T.Group(); root.name = 'ShardBound Studio preview'; root.userData.no_export = true;
  const pool = new Map(), textureCache = new Map(), anchorCache = new Map(), audio = new Map();
  let rendering = false, lastTime = null, lastAnimation = null, epoch = 0, dirty = true, lastFrame = '';
  let solo = null, soloStart = 0;
  let draft = null;
  const definitions = () => studio.state().effects.map(e => draft?.id === e.id ? draft : e);
  const white = g.document.createElement('canvas'); white.width = white.height = 32;
  const whiteCtx = white.getContext('2d'), radial = whiteCtx.createRadialGradient(16, 16, 0, 16, 16, 16);
  radial.addColorStop(0, '#ffffff'); radial.addColorStop(0.35, '#ffffffff'); radial.addColorStop(1, '#ffffff00');
  whiteCtx.fillStyle = radial; whiteCtx.fillRect(0, 0, 32, 32);
  const whiteTexture = new T.Texture(white); whiteTexture.needsUpdate = true;
  whiteTexture.magFilter = whiteTexture.minFilter = T.NearestFilter; whiteTexture.generateMipmaps = false;
  function cancelAudio(media) { media.studioCanceled = true; media.pause(); media.src = ''; }
  function stopAudio() { for (const media of audio.values()) cancelAudio(media); audio.clear(); }
  function texture(effect, frame = 0) {
    const key = `${effect.texture || 'radial'}:${effect.columns}:${effect.rows}:${frame}`;
    if (textureCache.has(key)) return textureCache.get(key);
    const source = g.Texture.all.find(t => t.uuid === effect.texture)?.img;
    if (!source?.complete || !source.naturalWidth) return whiteTexture;
    const width = source.naturalWidth / effect.columns, height = source.naturalHeight / effect.rows;
    const canvas = g.document.createElement('canvas'); canvas.width = Math.max(1, Math.round(width)); canvas.height = Math.max(1, Math.round(height));
    const col = frame % effect.columns, row = Math.floor(frame / effect.columns);
    canvas.getContext('2d').drawImage(source, col * width, row * height, width, height, 0, 0, canvas.width, canvas.height);
    const result = new T.Texture(canvas); result.needsUpdate = true; result.magFilter = result.minFilter = T.NearestFilter; result.generateMipmaps = false;
    textureCache.set(key, result); return result;
  }
  function matrixAt(target, time, animation) {
    const key = `${animation?.uuid || 'solo'}:${target || 'world'}:${time.toFixed(6)}`;
    if (anchorCache.has(key)) return anchorCache.get(key).clone();
    const current = g.Timeline.time;
    try {
      if (animation && Math.abs(time - current) > 1e-8) { g.Timeline.time = time; g.Animator.preview(false); }
      g.Canvas.scene.updateMatrixWorld(true);
      const node = studio.find(target), matrix = node?.mesh ? node.mesh.matrixWorld.clone() : new T.Matrix4();
      matrix.premultiply(g.Canvas.scene.matrixWorld.clone().invert()); anchorCache.set(key, matrix); return matrix.clone();
    } finally {
      if (g.Timeline.time !== current) { g.Timeline.time = current; g.Animator.preview(false); }
    }
  }
  function clearObject(object) {
    root.remove(object); if (object.userData.studioOwnGeometry) object.geometry?.dispose();
    const materials = Array.isArray(object.material) ? object.material : [object.material]; new Set(materials).forEach(m => m?.dispose());
  }
  function sprite(key, effect) {
    let object = pool.get(key);
    if (!object) {
      const material = new T.SpriteMaterial({ map: whiteTexture, transparent: true, depthWrite: false,
        blending: effect.blend === 'additive' ? T.AdditiveBlending : T.NormalBlending });
      object = new T.Sprite(material); root.add(object); pool.set(key, object);
    }
    return object;
  }
  function sound(event, time, initial = false) {
    const key = event.instance || `${event.resource}|${event.target || ''}`;
    if (event.action === 'stop') { for (const [id, media] of audio) if (media.studioInstance === key) { cancelAudio(media); audio.delete(id); } return; }
    const resource = studio.state().sounds.find(s => s.id === event.resource); if (!resource) return;
    let age = time - event.time; if (age < 0) return;
    if (!event.loop && resource.duration && age * event.pitch >= resource.duration) { if (initial) return; age = 0; }
    const audioKey = event.action === 'start' ? key : `${event.id}:${epoch}`;
    if (audio.has(audioKey)) { if (event.action !== 'start') return; cancelAudio(audio.get(audioKey)); audio.delete(audioKey); }
    const media = new g.Audio(resource.data); media.volume = Math.max(0, Math.min(1, event.volume * (g.settings.volume.value / 100)));
    try { media.playbackRate = event.pitch * (g.Timeline.playback_speed || 100) / 100; }
    catch { preview.lastError = 'A combinação de pitch e velocidade da timeline excedeu a faixa de reprodução do áudio no Blockbench.'; return; }
    media.studioPitch = event.pitch; media.studioInstance = key; media.preservesPitch = false; media.loop = !!event.loop; audio.set(audioKey, media);
    media.onloadedmetadata = () => { if (age > 0 && Number.isFinite(media.duration)) media.currentTime = event.loop ? (age * event.pitch) % media.duration : Math.min(age * event.pitch, media.duration); };
    media.onended = () => { if (audio.get(audioKey) === media) audio.delete(audioKey); };
    media.play().then(() => { if (!media.studioCanceled) preview.stats.audioStarted++; }).catch(error => {
      if (!media.studioCanceled) preview.lastError = `Áudio: ${error.message}`;
      if (audio.get(audioKey) === media) audio.delete(audioKey);
    });
  }
  function ribbon(key, interval, time, animation) {
    const times = trailTimes(interval, time); if (times.length < 2) return null;
    const e = interval.definition, position = [], uv = [], opacity = [], indices = [];
    const age = clamp((time - interval.start) / e.duration), width = e.width * curve(e.curves.size, age, e.size, e.sizeEnd);
    const alpha = curve(e.curves.opacity, age, e.opacity, e.opacityEnd), tint = curve(e.curves.color, age, e.color, e.colorEnd, true);
    for (let i = 0; i < times.length; i++) {
      const matrix = matrixAt(interval.event.target, times[i], animation), offset = new T.Vector3(...interval.event.offset);
      const a = new T.Vector3(...e.startOffset).add(offset).applyMatrix4(matrix), b = new T.Vector3(...e.endOffset).add(offset);
      b.sub(new T.Vector3(...e.startOffset).add(offset)).multiplyScalar(width).add(new T.Vector3(...e.startOffset).add(offset)).applyMatrix4(matrix);
      position.push(...a.toArray(), ...b.toArray()); uv.push(i / (times.length - 1), 0, i / (times.length - 1), 1);
      const fade = Math.max(0, 1 - (time - times[i]) / e.trailLength) * alpha; opacity.push(fade, fade);
      if (i) { const j = i * 2; indices.push(j - 2, j - 1, j, j - 1, j + 1, j); }
    }
    let object = pool.get(key);
    if (!object) {
      const material = new T.ShaderMaterial({ transparent: true, depthWrite: false, side: T.DoubleSide,
        blending: e.blend === 'additive' ? T.AdditiveBlending : T.NormalBlending,
        uniforms: { tint: { value: new T.Color(e.color) }, map: { value: texture(e) } },
        vertexShader: 'attribute float opacity; varying float vAlpha; varying vec2 vUv; void main(){vAlpha=opacity;vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}',
        fragmentShader: 'uniform vec3 tint; uniform sampler2D map; varying float vAlpha; varying vec2 vUv; void main(){vec4 c=texture2D(map,vUv);gl_FragColor=vec4(c.rgb*tint,c.a*vAlpha);}' });
      object = new T.Mesh(new T.BufferGeometry(), material); object.userData.studioOwnGeometry = true; root.add(object); pool.set(key, object);
    }
    object.geometry.setAttribute('position', new T.Float32BufferAttribute(position, 3)); object.geometry.setAttribute('uv', new T.Float32BufferAttribute(uv, 2));
    object.geometry.setAttribute('opacity', new T.Float32BufferAttribute(opacity, 1)); object.geometry.setIndex(indices); object.geometry.computeBoundingSphere();
    object.material.uniforms.tint.value.setRGB(...tint); object.material.uniforms.map.value = texture(e); return object;
  }
  function flashes(key, interval, time, seen, animation) {
    const visual = impulse(interval, time); if (!visual) return;
    const target = studio.find(interval.event.target), candidates = [];
    function visit(node) { if (node instanceof g.Cube || node instanceof g.Mesh) candidates.push(node); for (const child of node?.children || []) visit(child); }
    visit(target instanceof g.Locator ? target.parent : target);
    for (const node of candidates) {
      let hidden = false; for (let p = node; p && p !== 'root'; p = p.parent) if (p.visibility === false) hidden = true;
      if (hidden) continue;
      const id = `${key}:${node.uuid}`; let object = pool.get(id);
      if (!object) {
        const textures = faceTextures(g, node), cache = new Map();
        const materials = textures.map(texture => {
          const uuid = texture?.uuid || ''; if (cache.has(uuid)) return cache.get(uuid);
          const settings = studio.state().materials[uuid] || { alphaMode: 'MASK', alphaCutoff: 0.5, doubleSided: true };
          const material = new T.ShaderMaterial({ transparent: true, blending: T.AdditiveBlending, depthWrite: false,
            side: settings.doubleSided ? T.DoubleSide : T.FrontSide, polygonOffset: true, polygonOffsetFactor: -1, polygonOffsetUnits: -1,
            uniforms: { map: { value: texture?.material?.map || whiteTexture }, tint: { value: new T.Color() }, opacity: { value: 0 },
              opaque: { value: settings.alphaMode === 'OPAQUE' }, cutoff: { value: settings.alphaCutoff } },
            vertexShader: 'varying vec2 vUv;void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}',
            fragmentShader: 'uniform sampler2D map;uniform vec3 tint;uniform float opacity;uniform float cutoff;uniform bool opaque;varying vec2 vUv;void main(){float a=texture2D(map,vUv).a;if(!opaque&&a<cutoff)discard;gl_FragColor=vec4(tint,opacity);}' });
          cache.set(uuid, material); return material;
        });
        if (!materials.length) continue;
        object = new T.Mesh(node.mesh.geometry, materials.every(m => m === materials[0]) ? materials[0] : materials);
        object.matrixAutoUpdate = false; root.add(object); pool.set(id, object); }
      for (const material of new Set(Array.isArray(object.material) ? object.material : [object.material])) {
        material.uniforms.tint.value.setRGB(...visual.color); material.uniforms.opacity.value = visual.opacity;
      }
      const at = interval.event.follow || interval.definition.follow ? time : interval.start;
      object.matrix.copy(matrixAt(node.uuid, at, animation));
      const anchor = matrixAt(interval.event.target, at, animation), delta = new T.Vector3(...interval.event.offset).applyMatrix4(anchor).sub(new T.Vector3().setFromMatrixPosition(anchor));
      object.matrix.setPosition(new T.Vector3().setFromMatrixPosition(object.matrix).add(delta)); seen.add(id);
    }
  }
  const preview = {
    root, enabled: true, suspended: false, stats: { particles: 0, trails: 0, flashes: 0, glows: 0, audioStarted: 0 }, lastError: '',
    get activeSounds() { return audio.size; },
    get soloId() { return solo?.id || ''; },
    setDraft(effect) { draft = effect; preview.invalidate(); },
    clearDraft() { if (draft) { draft = null; preview.invalidate(); } },
    invalidate() { dirty = true; preview.lastError = ''; anchorCache.clear(); for (const object of pool.values()) clearObject(object); pool.clear();
      for (const item of textureCache.values()) item.dispose(); textureCache.clear(); },
    reset() { lastTime = null; lastAnimation = null; lastFrame = ''; solo = null; draft = null; stopAudio(); for (const object of pool.values()) clearObject(object); pool.clear(); anchorCache.clear();
      for (const map of textureCache.values()) map.dispose(); textureCache.clear(); dirty = true; },
    solo(id, target = '', event = {}) { const effect = studio.state().effects.find(e => e.id === id); if (!effect) throw new Error('Efeito não encontrado.');
      if (['trail', 'flash'].includes(effect.kind) && !target) throw new Error('Selecione uma pasta ou marcador para pré-visualizar o rastro/flash.');
      if (solo?.id !== id || solo?.target !== target) soloStart = g.performance.now() / 1000;
      solo = { id, target, offset: event.offset?.slice() || [0, 0, 0], follow: event.follow ?? true }; preview.invalidate(); },
    stopSolo() { solo = null; preview.invalidate(); },
    render(animation = g.Animation.selected, time = g.Timeline.time, { playing = false, seek = false } = {}) {
      if (rendering || preview.suspended) return;
      if (!studio.isStudio() || !preview.enabled) { root.visible = false; return; }
      const events = solo ? [{ id: 'solo', type: definitions().find(e => e.id === solo.id)?.kind,
        resource: solo.id, time: 0, action: 'start', target: solo.target, offset: solo.offset, follow: solo.follow }]
        : studio.events(animation);
      const muted = animation?.animators.effects?.muted[CHANNEL];
      if (muted && !solo) { root.visible = false; stopAudio(); return; }
      root.visible = true; if (root.parent !== g.Canvas.scene) g.Canvas.scene.add(root);
      const signature = `${animation?.uuid}:${time}:${solo?.id || ''}`;
      if (lastFrame === signature && !dirty && !playing) return;
      rendering = true;
      try {
        if (playing && !solo) {
          const looped = lastAnimation === animation?.uuid && lastTime !== null && time < lastTime && !seek;
          if (looped || lastAnimation !== animation?.uuid) { stopAudio(); epoch++; }
          const initial = lastTime === null || lastAnimation !== animation?.uuid;
          for (const event of crossedEvents(events, lastTime ?? 0, time, { length: animation?.length, looped, initial }))
            if (event.type === 'sound') sound(event, time, initial);
          for (const media of audio.values()) {
            const rate = media.studioPitch * (g.Timeline.playback_speed || 100) / 100;
            if (media.playbackRate !== rate) try { media.playbackRate = rate; } catch { preview.lastError = 'Velocidade de áudio não suportada pelo Blockbench.'; }
          }
        }
        if (seek || !playing) { if (!g.Timeline.playing) stopAudio(); }
        lastTime = time; lastAnimation = animation?.uuid; lastFrame = signature; dirty = false;
        const intervals = eventIntervals(events, definitions(), solo ? 3600 : animation?.length || 3600), seen = new Set();
        preview.stats.particles = preview.stats.trails = preview.stats.flashes = preview.stats.glows = 0;
        for (const interval of intervals) {
          const e = interval.definition, key = `${interval.event.id}:${interval.start}`;
          if (e.kind === 'particle') for (const p of particles(interval, time)) {
            const id = `${key}:${p.index}`, object = sprite(id, e);
            const anchor = matrixAt(interval.event.target, interval.event.follow || e.follow ? time : p.birth, solo ? null : animation);
            object.position.set(...p.position).add(new T.Vector3(...interval.event.offset)).applyMatrix4(anchor);
            object.scale.setScalar(p.size); object.material.opacity = p.opacity; object.material.color.setRGB(...p.color);
            object.material.rotation = p.rotation; object.material.map = texture(e, p.frame); seen.add(id); preview.stats.particles++;
          } else if (e.kind === 'trail') {
            if (ribbon(key, interval, time, solo ? null : animation)) { seen.add(key); preview.stats.trails++; }
          } else if (e.kind === 'flash') { const count = seen.size; flashes(key, interval, time, seen, solo ? null : animation); if (seen.size > count) preview.stats.flashes++; }
          else if (e.kind === 'glow') {
            const visual = impulse(interval, time); if (!visual) continue;
            const object = sprite(key, { ...e, blend: 'additive' }), anchor = matrixAt(interval.event.target, interval.event.follow || e.follow ? time : interval.start, solo ? null : animation);
            object.position.set(...interval.event.offset).applyMatrix4(anchor); object.scale.setScalar(visual.size);
            object.material.color.setRGB(...visual.color); object.material.opacity = visual.opacity; object.material.map = texture(e); seen.add(key);
            const lightKey = key + ':light'; let light = pool.get(lightKey);
            if (!light) { light = new T.PointLight(0xffffff, 1, e.distance); root.add(light); pool.set(lightKey, light); }
            light.position.copy(object.position); light.color.setRGB(...visual.color); light.intensity = visual.intensity; light.distance = e.distance; seen.add(lightKey); preview.stats.glows++;
          }
        }
        for (const [key, object] of pool) if (!seen.has(key)) { clearObject(object); pool.delete(key); }
        while (anchorCache.size > 4096) anchorCache.delete(anchorCache.keys().next().value);
      } catch (error) { preview.lastError = error.message; throw error; }
      finally { rendering = false; }
    },
    dispose() { preview.reset(); root.parent?.remove(root); for (const map of textureCache.values()) map.dispose(); textureCache.clear(); whiteTexture.dispose();
      for (const [event, fn] of handlers) g.Blockbench.removeListener(event, fn); },
  };
  const handlers = [
    ['display_animation_frame', ({ in_loop }) => { if (studio.isStudio() && !solo) preview.render(g.Animation.selected, g.Animation.selected?.time ?? g.Timeline.time, { playing: !!in_loop && g.Timeline.playing, seek: !in_loop }); }],
    ['timeline_play', () => { solo = null; lastTime = null; lastAnimation = null; }],
    ['timeline_pause', () => stopAudio()],
    ['render_frame', () => {
      if (!studio.isStudio() || preview.suspended || rendering) return;
      if (solo) { const e = definitions().find(e => e.id === solo.id); if (e) preview.render(null, (g.performance.now() / 1000 - soloStart) % (e.duration + (e.kind === 'particle' ? e.lifetime : 0.25))); }
      else if (dirty && g.Modes.animate) preview.render();
      else if (!g.Modes.animate) root.visible = false;
    }],
  ];
  for (const [event, fn] of handlers) g.Blockbench.on(event, fn);
  return preview;
}
