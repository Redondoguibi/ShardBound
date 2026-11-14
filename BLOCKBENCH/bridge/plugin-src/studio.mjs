// Adapter to the real loaded Studio API. No duplicated editor or executable scripts.
export function createStudioAdapter(g, { requireProject, changed, toBase64 }) {
  const sessions = new Map();
  const expiration = g.setInterval(() => pruneSessions(), 30000);
  const api = (a = {}, edit = true) => {
    const s = g.ShardBoundStudio;
    if (!s || s.apiVersion !== 1) throw new Error('Carregue ShardBound Studio 1.1 ou superior junto com a ShardBound Bridge.');
    requireProject(a, false);
    if (!s.isStudio()) throw new Error('Abra/crie um projeto ShardBound Studio.');
    if (edit && (g.Dialog.open || g.Undo.current_save || s.exporting)) throw new Error('Conclua o diálogo, edição ou exportação ativa primeiro.');
    return s;
  };
  const required = (v, field) => { if (v === undefined || v === '') throw new Error(`Campo obrigatório: ${field}.`); return v; };
  const clip = id => { const a = g.Animation.all.find(a => a.uuid === id); if (!a) throw new Error('Clip UUID não encontrado.'); return a; };
  const node = id => { const n = g.ShardBoundStudio.find(id); if (!n) throw new Error('Pasta/elemento UUID não encontrado.'); return n; };
  const group = id => { const n = node(id); if (!(n instanceof g.Group)) throw new Error('Use o UUID de uma pasta.'); return n; };
  const model = (s, id) => { const m = s.state().models.find(m => m.id === id); if (!m) throw new Error('Modelo ID não encontrado.'); return m; };
  const texture = id => { const t = g.Texture.all.find(t => t.uuid === id); if (!t) throw new Error('Textura UUID não encontrada.'); return t; };
  const frames = animation => Object.values(animation.animators).filter(a => a.type !== 'effect').flatMap(a => a.keyframes);
  const describeClip = a => ({ id: a.uuid, name: a.name, length: a.length, loop: a.loop, fps: a.snapping, keyframe_count: frames(a).length });
  const describeKey = k => ({ id: k.uuid, group: k.animator.uuid, channel: k.channel, time: k.time, interpolation: k.interpolation,
    values: k.data_points.map(p => [p.x, p.y, p.z]), bezier_linked: k.bezier_linked, bezier_left_time: k.bezier_left_time, bezier_left_value: k.bezier_left_value, bezier_right_time: k.bezier_right_time, bezier_right_value: k.bezier_right_value });
  function ensureUnreferenced(ids) {
    const s = g.ShardBoundStudio, set = new Set(ids);
    if (s.allEvents().some(e => set.has(e.target))) throw new Error('Remova/realoque os eventos que usam este alvo primeiro.');
    if (g.Animation.all.some(a => frames(a).some(k => set.has(k.animator.uuid)))) throw new Error('Remova/realoque os keyframes das pastas primeiro.');
  }
  function pruneSessions() { for (const [key, session] of sessions) if (Date.now() - session.created > 300000) sessions.delete(key); }
  const handlers = {
    studio_read(a) {
      const s = api(a, false), state = structuredClone(s.state());
      state.sounds = state.sounds.map(({ data, ...metadata }) => ({ ...metadata, bytes: Math.floor(data.split(',')[1].length * .75) }));
      const keys = a.clip ? frames(clip(a.clip)).map(describeKey).sort((a, b) => a.time - b.time || a.id.localeCompare(b.id)) : [];
      return { studio: { installed: true, version: s.version, api_version: s.apiVersion }, state, clips: g.Animation.all.map(describeClip), events: s.allEvents(),
        locators: g.Locator.all.map(l => ({ id: l.uuid, name: l.name, parent: l.parent?.uuid, position: l.position })),
        timeline: { clip: g.Animation.selected?.uuid || null, time: g.Timeline.time, playing: !!g.Timeline.playing, speed: g.Timeline.playback_speed },
        keyframes: keys.slice(a.offset, a.offset + a.limit), total_keyframes: keys.length, next_offset: a.offset + a.limit < keys.length ? a.offset + a.limit : null,
        validation: s.exporter.validate(), preview: { ...s.preview.stats, error: s.preview.lastError } };
    },
    studio_create_project(a) {
      const s = g.ShardBoundStudio;
      if (!s || s.apiVersion !== 1) throw new Error('Carregue ShardBound Studio 1.1 ou superior.');
      if (g.Dialog.open || g.Undo?.current_save || s.exporting) throw new Error('Conclua a edição ativa primeiro.');
      return { model: s.createProject(a.type, { id: a.id, name: a.name, variant: a.variant }) };
    },
    studio_project_settings(a) { const s = api(a); const { project_id, expected_revision, ...patch } = a;
      s.edit('MCP: propriedades Studio', () => Object.assign(s.state(), patch)); return { state: { id: s.state().id, sampleRate: s.state().sampleRate, unitsPerMeter: s.state().unitsPerMeter, projectileVisual: s.state().projectileVisual } }; },
    studio_model(a) {
      const s = api(a);
      if (a.action === 'add') return { model: s.addModel(required(a.type, 'type'), { id: required(a.id, 'id'), name: a.name }) };
      const m = model(s, required(a.model, 'model'));
      if (a.action === 'select') s.selectModel(m);
      if (a.action === 'update') s.renameModel(m, a.id || m.id, a.name || m.name);
      if (a.action === 'bind') s.bind(m, required(a.part, 'part'), a.root || m.roots[0]);
      if (a.action === 'remove') {
        const descendants = n => [n.uuid, ...(n.children || []).flatMap(descendants)];
        ensureUnreferenced(m.roots.flatMap(id => descendants(node(id))));
        s.removeModel(m); return { removed: a.model };
      }
      return { model: structuredClone(m) };
    },
    studio_locator(a) {
      const s = api(a); let locator;
      if (a.action === 'add') {
        const parent = group(required(a.parent, 'parent')), elements = [];
        s.edit('MCP: criar marcador', () => { locator = new g.Locator({ name: required(a.name, 'name'), position: a.position || parent.origin.slice() }).addTo(parent).init(); elements.push(locator); }, { elements, outliner: true });
      } else {
        locator = node(required(a.id, 'id')); if (!(locator instanceof g.Locator)) throw new Error('Use o UUID de um marcador.');
        if (a.action === 'remove') { ensureUnreferenced([locator.uuid]); s.edit('MCP: remover marcador', () => locator.remove(), { elements: [locator], outliner: true }); return { removed: a.id }; }
        const parent = a.parent ? group(a.parent) : null;
        s.edit('MCP: editar marcador', () => { locator.extend({ ...(a.name ? { name: a.name } : {}), ...(a.position ? { position: a.position } : {}) }); if (parent) locator.addTo(parent); }, { elements: [locator], outliner: true });
      }
      return { locator: { id: locator.uuid, name: locator.name, parent: locator.parent.uuid, position: locator.position } };
    },
    studio_clip(a) {
      const s = api(a);
      if (a.action === 'add') return { clip: describeClip(s.createClip(required(a.name, 'name'), a.length ?? 1, a.loop || 'loop', a.fps || 24)) };
      const animation = clip(required(a.clip, 'clip'));
      if (a.action === 'select') { g.Modes.options.animate.select(); animation.select(); }
      if (a.action === 'remove') { s.edit('MCP: remover clip', () => { g.Timeline.pause(); animation.remove(false); }, { animations: [animation] }); return { removed: a.clip }; }
      if (a.action === 'update') {
        if (a.name && g.Animation.all.some(other => other !== animation && other.name === a.name)) throw new Error('Nome de clip repetido.');
        if (a.length !== undefined && [...frames(animation), ...s.events(animation)].some(k => k.time > a.length)) throw new Error('A duração excluiria keyframes/eventos existentes.');
        s.edit('MCP: propriedades do clip', () => { animation.extend({ name: a.name ?? animation.name, length: a.length ?? animation.length, loop: a.loop ?? animation.loop, snapping: a.fps ?? animation.snapping }); }, { animations: [animation] });
      }
      return { clip: describeClip(animation) };
    },
    studio_keyframes(a) {
      const s = api(a), animation = clip(a.clip), all = frames(animation), byId = new Map(all.map(k => [k.uuid, k]));
      if (!a.remove.length && !a.keyframes.length) throw new Error('Forneça keyframes ou remove.');
      for (const id of a.remove) if (!byId.has(id)) throw new Error(`Keyframe não encontrado: ${id}.`);
      const slots = new Set(), ids = new Set(a.remove);
      for (const k of a.keyframes) {
        group(k.group); if (k.time > animation.length) throw new Error('Keyframe fora da duração do clip.');
        if (k.id) { const old = byId.get(k.id); if (!old || old.animator.uuid !== k.group || old.channel !== k.channel || ids.has(k.id)) throw new Error('UUID de keyframe incompatível/repetido.'); ids.add(k.id); }
        const slot = `${k.group}|${k.channel}|${k.time}`; if (slots.has(slot)) throw new Error('Keyframes repetidos no mesmo canal/frame.'); slots.add(slot);
        const collision = all.find(old => old.animator.uuid === k.group && old.channel === k.channel && Math.abs(old.time - k.time) < 1e-8 && old.uuid !== k.id && !a.remove.includes(old.uuid));
        if (k.id && collision) throw new Error('Já existe um keyframe no frame de destino.');
        if (k.bezier_left_time?.some(v => v > 0) || k.bezier_right_time?.some(v => v < 0)) throw new Error('Handles de tempo Bezier: esquerda <= 0; direita >= 0.');
      }
      const updated = [];
      s.edit('MCP: keyframes de pastas', () => {
        a.remove.forEach(id => byId.get(id).remove());
        for (const k of a.keyframes) {
          const animator = animation.getBoneAnimator(group(k.group)), { id, group: _, value, ...data } = k;
          const old = id ? byId.get(id) : animator[k.channel].find(old => Math.abs(old.time - k.time) < 1e-8);
          const payload = { ...data, data_points: [{ x: value[0], y: value[1], z: value[2] }] };
          const result = old ? old.extend(payload) : animator.addKeyframe(payload); updated.push(describeKey(old || result));
        }
      }, { animations: [animation] });
      return { keyframes: updated, removed: a.remove };
    },
    studio_effect(a) {
      const s = api(a);
      if (a.values.texture) texture(a.values.texture);
      if (a.action === 'add') { if (a.values.id && a.values.id !== a.id) throw new Error('values.id deve coincidir com id ao criar.'); return { effect: s.addEffect(required(a.kind, 'kind'), a.id, a.values) }; }
      if (!s.state().effects.some(e => e.id === a.id)) throw new Error('Efeito ID não encontrado.');
      if (a.action === 'update') return { effect: s.updateEffect(a.id, a.values) };
      s.removeEffect(a.id); return { removed: a.id };
    },
    studio_event(a) {
      const s = api(a);
      if (a.action === 'add') { const animation = clip(required(a.clip, 'clip')); required(a.time, 'time'); required(a.values.type, 'values.type'); required(a.values.resource, 'values.resource');
        const added = s.addEvent(animation, a.time, a.values); return { event: s.events(animation).find(e => e.id === `${added.keyframe.uuid}:${added.event.id}`) }; }
      const id = required(a.id, 'id'); if (!s.allEvents().some(e => e.id === id)) throw new Error('Use o ID composto retornado por studio_read.');
      if (a.action === 'remove') { s.removeEvent(id); return { removed: id }; }
      s.updateEvent(id, a.values, a.time); return { events: s.allEvents() };
    },
    studio_sound(a) { const s = api(a); if (a.action !== 'remove') throw new Error('Importação deve passar pelo servidor MCP.');
      if (!s.state().sounds.some(sound => sound.id === a.id)) throw new Error('Som não encontrado.'); s.removeSound(a.id); return { removed: a.id }; },
    studio_skin(a) { const s = api(a); if (a.action === 'layers') { required(a.visible, 'visible'); s.setSkinLayers(a.visible); return { visible: a.visible }; }
      if (!a.texture || a.path) throw new Error('Forneça somente path ou texture.'); return s.importSkin(texture(a.texture).getDataURL(), () => api(a)).then(t => ({ texture_id: t.uuid })); },
    studio_material(a) { const s = api(a); texture(a.texture); const material = { alphaMode: a.alphaMode, alphaCutoff: a.alphaCutoff, doubleSided: a.doubleSided };
      s.edit('MCP: material', () => { s.state().materials[a.texture] = material; }); return { texture: a.texture, material }; },
    paint_texture(a) {
      requireProject(a, false); if (g.Dialog.open || g.Undo.current_save) throw new Error('Conclua a edição ativa primeiro.');
      const t = texture(a.texture); if (!t.img.complete || !t.img.naturalWidth) throw new Error('A textura ainda está carregando.');
      for (const r of a.rectangles) if (r.x + r.width > t.width || r.y + r.height > t.height) throw new Error('Retângulo fora da textura.');
      g.Undo.initEdit({ textures: [t], bitmap: true });
      try { for (const r of a.rectangles) { if (r.erase) t.ctx.clearRect(r.x, r.y, r.width, r.height); else { t.ctx.fillStyle = r.color; t.ctx.fillRect(r.x, r.y, r.width, r.height); } }
        t.updateChangesAfterEdit(); g.Undo.finishEdit('MCP: pintar textura'); g.ShardBoundStudio?.preview?.invalidate(); return { texture: t.uuid, rectangles: a.rectangles.length };
      } catch (error) { g.Undo.cancelEdit(true); throw error; }
    },
    studio_timeline(a) {
      const s = api(a); const animation = a.clip ? clip(a.clip) : g.Animation.selected; if (!animation) throw new Error('Selecione um clip.');
      if (a.time !== undefined && a.time > animation.length) throw new Error('Tempo fora do clip.');
      g.Timeline.pause(); g.Modes.options.animate.select(); animation.select();
      if (a.speed !== undefined) g.Timeline.playback_speed = a.speed;
      if (a.time !== undefined) g.Timeline.setTime(a.time);
      g.Animator.preview(false); if (a.action === 'play') g.Timeline.start(); else s.preview.render(animation, g.Timeline.time, { playing: false });
      return { clip: animation.uuid, time: g.Timeline.time, playing: !!g.Timeline.playing, speed: g.Timeline.playback_speed };
    },
    studio_validate(a) { return { validation: api(a, false).exporter.validate() }; },
    async studio_export(a) {
      const s = api(a); pruneSessions(); if (sessions.size) throw new Error('Uma transferência de pacote está ativa. Aguarde terminar (ou expirar em 5 minutos).');
      const compiled = await s.exporter.compile(); api(a, false);
      const files = new Map([...compiled.files].map(([name, data]) => [name, typeof data === 'string' ? new TextEncoder().encode(data) : data]));
      const manifest = [...files].map(([name, data]) => ({ name, bytes: data.byteLength }));
      if (manifest.reduce((sum, f) => sum + f.bytes, 0) > 256 * 1024 * 1024) throw new Error('Pacote excede o limite MCP de 256 MiB. Exporte pelo painel Studio.');
      const session = g.guid(); sessions.set(session, { created: Date.now(), files });
      return { session, id: compiled.state.id, files: manifest, warnings: compiled.report.warnings };
    },
  };
  const internal = {
    async __studio_import_sound(a) { const s = api(a); const sound = await s.addSound(a.id, a.data, a.filename, () => api(a)); return { sound: { id: sound.id, duration: sound.duration, filename: sound.filename } }; },
    async __studio_import_skin(a) { const s = api(a); const t = await s.importSkin(a.data, () => api(a)); return { texture_id: t.uuid }; },
    __studio_export_chunk(a) { pruneSessions(); const session = sessions.get(a.session), bytes = session?.files.get(a.file);
      if (!bytes || !Number.isInteger(a.offset) || a.offset < 0 || a.offset > bytes.byteLength) throw new Error('Transferência de pacote inválida/expirada.');
      session.created = Date.now(); return { data: toBase64(bytes.subarray(a.offset, a.offset + 1024 * 1024)), total: bytes.byteLength }; },
    __studio_export_release(a) { sessions.delete(a.session); return { released: true }; },
  };
  return { handlers, internal, ensureUnreferenced, async call(name, a, isInternal = false) { const result = await (isInternal ? internal : handlers)[name](a);
    if (!['studio_read', 'studio_validate', '__studio_export_chunk', '__studio_export_release'].includes(name)) changed(); return result; }, dispose() { g.clearInterval(expiration); sessions.clear(); } };
}
