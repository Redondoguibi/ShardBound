import { VERSION, FORMAT, CHANNEL, TYPES, BODY_PARTS, projectState, checkState, assertId, clone, defaultEffect, validateEffect, orderedEvents } from './core.mjs';
import { makePlayer, playerParts, skinFaces } from './templates.mjs';

export function createStudio(g) {
  const disposables = [], listeners = [], dataProperties = new Set();
  const listen = (event, fn) => { g.Blockbench.on(event, fn); listeners.push([event, fn]); };
  const property = (target, type, name, options) => { const p = target.properties[name] || new g.Property(target, type, name, options); disposables.push(p); dataProperties.add(p); return p; };
  const isStudio = () => g.Format?.id === FORMAT;
  const state = () => {
    if (!isStudio()) throw new Error('Abra um projeto ShardBound Studio.');
    // Blockbench selects a fresh tab before calling ModelFormat.onSetup.
    if (!g.Project.shardbound_studio || !Object.keys(g.Project.shardbound_studio).length) g.Project.shardbound_studio = projectState();
    return checkState(g.Project.shardbound_studio);
  };
  property(g.ModelProject, 'object', 'shardbound_studio', { default: () => ({}), condition: () => isStudio(), exposed: false });
  property(g.KeyframeDataPoint, 'object', 'studio_event', { default: () => ({}), exposed: false,
    condition: point => point.keyframe.channel === CHANNEL });
  // The 5.2.2 helper addChannel fails to initialize some existing projects. Register the public channel descriptor explicitly.
  const channelDescriptor = { name: 'ShardBound: efeitos e sons', mutable: true, max_data_points: 1, condition: isStudio };
  const syncChannel = () => { if (isStudio()) g.EffectAnimator.prototype.channels[CHANNEL] = channelDescriptor; else delete g.EffectAnimator.prototype.channels[CHANNEL]; };
  syncChannel();
  for (const project of g.ModelProject.all.filter(project => project.format?.id === FORMAT)) for (const animation of project.animations || []) {
    const animator = animation.animators.effects;
    if (animator && !Array.isArray(animator[CHANNEL])) { g.Vue.set(animator, CHANNEL, []); g.Vue.set(animator.muted, CHANNEL, false); }
  }
  const format = new g.ModelFormat(FORMAT, { name: 'ShardBound Studio', description: 'Modelos, skins, animações e efeitos ShardBound',
    icon: 'auto_awesome', category: 'general', target: ['ShardBound'], meshes: true, rotate_cubes: true, bone_rig: true,
    armature_rig: false, centered_grid: true, optional_box_uv: true, box_uv_float_size: true,
    per_texture_uv_size: true, uv_rotation: true, animation_mode: true, locators: true,
    quaternion_interpolation: false, euler_order: 'ZYX', animation_files: false, can_convert_to: false,
    new() { studio.ui?.handlers.newProject(); return true; },
    onSetup(project) { if (!project.shardbound_studio?.schema) project.shardbound_studio = projectState(); } });
  disposables.push(format);
  const codec = new g.Codec(FORMAT, { name: 'Projeto ShardBound Studio', extension: 'sbstudio', remember: true, format,
    load_filter: { type: 'json', extensions: ['sbstudio'], condition: data => !!data?.shardbound_studio },
    compile(options = {}) { state(); return g.Codecs.project.compile({ ...options, bitmaps: true, absolute_paths: false }); },
    load(data, file = { path: '', no_file: true }) {
      checkState(data?.shardbound_studio);
      const effectErrors = data.shardbound_studio.effects.flatMap(validateEffect); if (effectErrors.length) throw new Error(effectErrors.join('\n'));
      if (data.meta?.model_format !== FORMAT) throw new Error('O arquivo não contém um projeto Studio.');
      g.Codecs.project.load(clone(data), { ...file, no_file: file.no_file ?? !file.path });
      for (const animation of g.Animation.all) effectsFor(animation);
      studio.refresh();
    },
    export() {
      const project = g.Project;
      g.Blockbench.export({ resource_id: FORMAT, type: 'Projeto ShardBound Studio', extensions: ['sbstudio'],
        name: g.Project.name || 'studio', startpath: g.Project.save_path, content: this.compile(),
        custom_writer: (content, path) => { g.Blockbench.writeFile(path, { content }, () => { project.save_path = path; project.saved = true; }); } });
    } });
  disposables.push(codec);
  format.codec = codec;
  function effectsFor(animation) {
    const animator = animation.animators.effects ||= new g.EffectAnimator(animation);
    if (animator.studioController === studio) return animator;
    animator.studioController = studio;
    // Native effect channels target other runtimes. Studio owns one combined authoring channel.
    animator.channels = { [CHANNEL]: channelDescriptor };
    for (const channel of ['particle', 'sound', 'timeline', CHANNEL]) if (!Array.isArray(animator[channel])) animator[channel] = [];
    animator.createKeyframe = function(value, time, channel, ...args) {
      if (channel === CHANNEL) return studio.onRequestEvent?.(animation, time ?? g.Timeline.time);
      return g.EffectAnimator.prototype.createKeyframe.call(this, value, time, channel, ...args);
    };
    return animator;
  }
  const find = uuid => g.Group.all.find(e => e.uuid === uuid) || g.Outliner.elements.find(e => e.uuid === uuid);
  function repairTemplateUV() {
    if (!isStudio()) return;
    const s = state(), repaired = [];
    for (const player of [s.context, ...s.models.map(m => m.player)].filter(Boolean)) {
      const texture = g.Texture.all.find(t => t.uuid === player.texture);
      if (!texture || texture.uv_width !== 32 || texture.uv_height !== 32) continue;
      const parts = playerParts(player.variant || s.skinVariant);
      // Only repair the exact generated skin layout. Authored/custom UVs are never rewritten.
      const knownLayout = player.geometry.length && player.geometry.every(id => {
        const cube = find(id), part = parts.find(p => p.name === cube?.name?.replace(/_layer$/, ''));
        if (!part || cube.box_uv) return false;
        const expected = skinFaces(cube.name.endsWith('_layer') ? part.outerUv : part.uv, part.to.map((v, i) => v - part.from[i]));
        return Object.entries(expected).every(([face, uv]) => cube.faces[face]?.texture === texture.uuid && uv.every((v, i) => v === cube.faces[face].uv[i]));
      });
      if (knownLayout) repaired.push(texture);
    }
    if (repaired.length) {
      edit('Studio: corrigir escala UV do template 64×64', () => repaired.forEach(t => { t.uv_width = t.uv_height = 64; }), { textures: repaired, uv_only: true });
      g.Blockbench.showQuickMessage('Escala UV 64×64 corrigida nos templates do player. A alteração pode ser desfeita.', 6000);
    }
  }
  function edit(label, fn, aspects = {}) {
    state();
    if (g.Undo.current_save) throw new Error('Conclua a edição ativa primeiro.');
    g.Undo.initEdit({ studio: true, ...aspects });
    try { const result = fn(); g.Undo.finishEdit(label); studio.refresh(); return result; }
    catch (error) { g.Undo.cancelEdit(true); studio.refresh(); throw error; }
  }
  listen('create_undo_save', ({ save, aspects }) => { if (isStudio() && aspects.studio) save.shardbound_studio = clone(g.Project.shardbound_studio); });
  listen('load_undo_save', ({ save }) => { if (isStudio() && save.shardbound_studio) { g.Project.shardbound_studio = clone(save.shardbound_studio); studio.refresh(); } });
  function context() {
    const s = state();
    if (!s.context) s.context = makePlayer(g, { variant: s.skinVariant, reference: true, rootName: 'Player de referência' });
    return s.context;
  }
  function addModel(type = 'item', options = {}) {
    const s = state(); if (!TYPES[type]) throw new Error('Tipo de modelo inválido.');
    const id = assertId(options.id || `${s.id}_${s.models.length + 1}`);
    if (s.models.some(m => m.id === id)) throw new Error('Já existe um modelo com esse ID.');
    const model = { id, type, name: options.name || TYPES[type], roots: [], bindings: [] };
    if (type === 'player') {
      const player = makePlayer(g, { variant: s.skinVariant }); model.roots = [player.root]; model.player = player;
    } else if (type === 'armor') {
      const ref = context();
      for (const part of playerParts(s.skinVariant)) {
        const root = new g.Group({ name: `Armadura_${part.name}`, origin: part.origin }).addTo(find(ref.groups[part.name])).init();
        model.roots.push(root.uuid); model.bindings.push({ group: root.uuid, target: part.name, referenceGroup: ref.groups[part.name] });
      }
    } else {
      let parent = 'root', origin = [0, 0, 0];
      if (['melee', 'ranged'].includes(type)) { const ref = context(); parent = find(ref.groups.RightArm); origin = [-5, 16, 0]; }
      const root = new g.Group({ name: options.name || type, origin }).addTo(parent).init(); model.roots.push(root.uuid);
      if (parent !== 'root') model.bindings.push({ group: root.uuid, target: 'RightArm', referenceGroup: parent.uuid });
      if (type === 'block') new g.Cube({ name: 'Bloco', from: [0, 0, 0], to: [16, 16, 16] }).addTo(root).init();
    }
    s.models.push(model); return model;
  }
  const studio = {
    version: VERSION, apiVersion: 1, format, codec, isStudio, state, find, edit, onRefresh: null, onRequestEvent: null, preview: null,
    refresh() { if (isStudio()) { g.Canvas.updateAll(); g.updateSelection(); } studio.preview?.invalidate(); studio.onRefresh?.(); },
    createProject(type = 'monster', options = {}) {
      if (!TYPES[type]) throw new Error('Tipo de projeto inválido.');
      assertId(options.id || 'custom:model');
      if (!g.newProject(format)) throw new Error('Não foi possível criar o projeto.');
      g.Project.name = options.name || TYPES[type]; g.Project.texture_width = g.Project.texture_height = type === 'player' ? 64 : 32;
      g.Project.shardbound_studio = projectState(type, options.id || 'custom:model');
      state().skinVariant = options.variant || 'wide';
      const model = addModel(type, { ...options, id: state().id });
      studio.refresh(); return model;
    },
    addModel(type, options) {
      const elements = [], groups = [], textures = [];
      return edit('Studio: adicionar modelo', () => {
        const before = new Set(g.Outliner.elements), beforeG = new Set(g.Group.all), beforeT = new Set(g.Texture.all);
        const result = addModel(type, options);
        elements.push(...g.Outliner.elements.filter(e => !before.has(e))); groups.push(...g.Group.all.filter(e => !beforeG.has(e)));
        textures.push(...g.Texture.all.filter(e => !beforeT.has(e))); return result;
      }, { elements, groups, textures, outliner: true });
    },
    selectModel(model) { model.roots.map(find).filter(Boolean).forEach((root, i) => i ? root.multiSelect?.() : root.select()); },
    renameModel(model, id, name) { assertId(id); if (state().models.some(m => m !== model && m.id === id)) throw new Error('ID de modelo repetido.'); return edit('Studio: propriedades do modelo', () => { model.id = id; model.name = name; }); },
    removeModel(model) { return edit('Studio: remover modelo', () => { model.roots.map(find).filter(Boolean).forEach(root => root.remove()); state().models = state().models.filter(m => m.id !== model.id); }, { elements: g.Outliner.elements.slice(), groups: g.Group.all.slice(), outliner: true }); },
    createClip(name = 'idle', length = 1, loop = 'loop', fps = 24) {
      if (!name.trim() || g.Animation.all.some(a => a.name === name)) throw new Error('Nome de clip vazio ou repetido.');
      if (!(length > 0 && length <= 1000) || !Number.isFinite(fps) || fps < 1 || fps > 120 || !['once', 'loop', 'hold'].includes(loop)) throw new Error('Use duração até 1000 s e timeline de 1–120 FPS, limites nativos do Blockbench 5.2.2.');
      const animations = [];
      return edit('Studio: criar clip', () => {
        const animation = new g.Animation({ name, length, loop, snapping: fps }).add(); animations.push(animation);
        g.Modes.options.animate.select(); animation.select();
        effectsFor(animation).addToTimeline(true);
        return animation;
      }, { animations });
    },
    addEffect(kind, id, values = {}) {
      const effect = Object.assign(defaultEffect(kind, id), clone(values));
      const errors = validateEffect(effect); if (errors.length) throw new Error(errors.join('\n'));
      if ([...state().effects, ...state().sounds].some(e => e.id === effect.id)) throw new Error('ID de efeito/som repetido.');
      return edit('Studio: criar efeito', () => { state().effects.push(effect); return effect; });
    },
    updateEffect(id, values) {
      const index = state().effects.findIndex(e => e.id === id); if (index < 0) throw new Error('Efeito não encontrado.');
      const effect = Object.assign(clone(state().effects[index]), clone(values));
      const errors = validateEffect(effect); if (errors.length) throw new Error(errors.join('\n'));
      if (effect.id !== id && [...state().effects, ...state().sounds].some(e => e.id === effect.id)) throw new Error('ID de efeito/som repetido.');
      // Changing the visual application keeps every existing timeline event compatible.
      for (const event of studio.allEvents().filter(e => e.resource === id)) studio.validateEvent({ ...event, resource: effect.id, type: effect.kind }, effect);
      return edit('Studio: editar efeito', () => {
        state().effects[index] = effect;
        for (const animation of g.Animation.all) for (const kf of animation.animators.effects?.[CHANNEL] || []) for (const point of kf.data_points)
          if (point.studio_event.resource === id) { point.studio_event.resource = effect.id; point.studio_event.type = effect.kind; }
        return effect;
      }, { animations: g.Animation.all.slice() });
    },
    removeEffect(id) { if (studio.allEvents().some(e => e.resource === id)) throw new Error('Remova os keyframes que referenciam esse efeito primeiro.');
      return edit('Studio: remover efeito', () => { state().effects = state().effects.filter(e => e.id !== id); }); },
    async addSound(id, data, filename = '', beforeCommit = () => {}, placement = null) {
      assertId(id); if ([...state().effects, ...state().sounds].some(e => e.id === id)) throw new Error('ID de efeito/som repetido.');
      if (!/^data:audio\/(wav|x-wav|ogg|mpeg);base64,[A-Za-z0-9+/=]+$/.test(data) || data.length > 32 * 1024 * 1024)
        throw new Error('Use áudio WAV, OGG ou MP3, até 24 MiB.');
      const project = g.Project, audio = new g.AudioContext(); let decoded;
      try { decoded = await audio.decodeAudioData(Uint8Array.from(g.atob(data.split(',')[1]), c => c.charCodeAt(0)).buffer); }
      catch { throw new Error('O áudio não pode ser decodificado pelo Blockbench. Verifique o arquivo WAV, OGG ou MP3.'); }
      finally { await audio.close(); }
      if (g.Project !== project) throw new Error('O projeto mudou durante a importação do áudio.');
      if ([...state().effects, ...state().sounds].some(e => e.id === id)) throw new Error('ID de efeito/som repetido.');
      beforeCommit();
      const sound = { id, data, filename, duration: decoded.duration };
      if (placement?.eventId) {
        const point = (placement.animation?.animators.effects?.[CHANNEL] || []).flatMap(k => k.data_points.map(p => ({ point: p, keyframe: k })))
          .find(({ point, keyframe }) => point.studio_event.id === placement.eventId || `${keyframe.uuid}:${point.studio_event.id}` === placement.eventId)?.point;
        if (!point) throw new Error('O evento de áudio não existe mais.');
        const event = { ...clone(point.studio_event), resource: id, type: 'sound' };
        studio.validateEvent(event, sound);
        return edit('Studio: substituir áudio no evento', () => { state().sounds.push(sound); point.studio_event = event; return sound; }, { animations: [placement.animation] });
      }
      if (placement) {
        studio.addEvent(placement.animation, placement.time, { type: 'sound', resource: id, target: '', action: 'burst' }, sound);
        return sound;
      }
      return edit('Studio: importar som', () => { state().sounds.push(sound); return sound; });
    },
    removeSound(id) { if (studio.allEvents().some(e => e.resource === id)) throw new Error('Remova os keyframes desse som primeiro.');
      return edit('Studio: remover som', () => { state().sounds = state().sounds.filter(s => s.id !== id); }); },
    addEvent(animation, time, values, newDefinition = null) {
      state(); if (!animation || !g.Animation.all.includes(animation)) throw new Error('Selecione um clip.');
      if (!Number.isFinite(time) || time < 0 || time > animation.length) throw new Error('O frame deve estar dentro da duração do clip.');
      const data = { id: g.guid(), type: 'particle', action: 'burst', target: '', offset: [0, 0, 0], follow: false,
        instance: '', volume: 1, pitch: 1, loop: false, ...clone(values), order: state().nextEventOrder };
      if (newDefinition) {
        if (newDefinition.kind) { const errors = validateEffect(newDefinition); if (errors.length) throw new Error(errors.join('\n')); }
        else if (data.type !== 'sound' || !(newDefinition.duration > 0) || !/^data:audio\/(wav|ogg|mpeg);base64,/.test(newDefinition.data)) throw new Error('Áudio novo inválido.');
        if (data.resource !== newDefinition.id || [...state().effects, ...state().sounds].some(e => e.id === newDefinition.id)) throw new Error('Recurso novo inválido ou repetido.');
      }
      studio.validateEvent(data, newDefinition);
      const keyframes = [], animations = [animation];
      return edit('Studio: evento na timeline', () => {
        if (newDefinition) state()[newDefinition.kind ? 'effects' : 'sounds'].push(clone(newDefinition));
        state().nextEventOrder++;
        const animator = effectsFor(animation); animator.addToTimeline(true);
        let keyframe = animator[CHANNEL].find(k => Math.abs(k.time - time) < 1e-8);
        if (keyframe) {
          const point = new g.KeyframeDataPoint(keyframe); point.extend({ studio_event: data }); keyframe.data_points.push(point);
        } else { keyframe = animator.addKeyframe({ channel: CHANNEL, time, data_points: [{ studio_event: data }] }); keyframes.push(keyframe); }
        keyframe.select(); return { keyframe, event: data };
      }, { keyframes, animations });
    },
    validateEvent(event, resourceOverride = null) {
      if (typeof event.id !== 'string' || !event.id || !Number.isInteger(event.order) || event.order < 0 || typeof event.target !== 'string' || typeof event.instance !== 'string' || typeof event.follow !== 'boolean' || typeof event.loop !== 'boolean') throw new Error('Identificação ou configuração de evento inválida.');
      if (!['particle', 'trail', 'flash', 'glow', 'sound'].includes(event.type)) throw new Error('Tipo de evento desconhecido.');
      if (!['burst', 'start', 'stop'].includes(event.action)) throw new Error('Ação de evento inválida.');
      const resource = resourceOverride || (event.type === 'sound' ? state().sounds : state().effects).find(r => r.id === event.resource);
      if (!resource || (event.type !== 'sound' && resource.kind !== event.type)) throw new Error('Recurso não encontrado ou incompatível com o tipo do evento.');
      if (event.target && !find(event.target)) throw new Error('Pasta/marcador do evento não encontrado.');
      if (['trail', 'flash'].includes(event.type) && !event.target) throw new Error('Escolha uma pasta ou marcador para o rastro/flash.');
      if (event.type === 'flash') {
        const target = find(event.target), node = target instanceof g.Locator ? target.parent : target;
        const geometry = candidate => candidate instanceof g.Cube || candidate instanceof g.Mesh || candidate?.children?.some(geometry);
        if (!geometry(node)) throw new Error('O alvo do flash deve conter geometria.');
      }
      if (!Array.isArray(event.offset) || event.offset.length !== 3 || !event.offset.every(Number.isFinite)) throw new Error('Offset do evento inválido.');
      if (!Number.isFinite(event.volume) || event.volume < 0 || event.volume > 1 || !Number.isFinite(event.pitch) || event.pitch < 0.1 || event.pitch > 4) throw new Error('Volume/pitch inválido.');
      return event;
    },
    events(animation = g.Animation.selected) {
      return orderedEvents((animation?.animators.effects?.[CHANNEL] || []).flatMap(keyframe => keyframe.data_points.map(point => ({ ...clone(point.studio_event), id: `${keyframe.uuid}:${point.studio_event.id}`, time: keyframe.time, frame: Math.round(keyframe.time * animation.snapping), fps: animation.snapping, clip: animation.name, keyframe: keyframe.uuid }))));
    },
    allEvents() { return g.Animation.all.flatMap(animation => studio.events(animation)); },
    updateEvent(eventId, values, time) {
      for (const animation of g.Animation.all) for (const keyframe of animation.animators.effects?.[CHANNEL] || []) {
        const point = keyframe.data_points.find(p => p.studio_event.id === eventId || `${keyframe.uuid}:${p.studio_event.id}` === eventId); if (!point) continue;
        const event = { ...clone(point.studio_event), ...clone(values) }; studio.validateEvent(event);
        if (time !== undefined && (!Number.isFinite(time) || time < 0 || time > animation.length)) throw new Error('Frame fora do clip.');
        return edit('Studio: editar evento', () => {
          point.studio_event = event;
          if (time === undefined || Math.abs(time - keyframe.time) < 1e-8) return;
          const animator = effectsFor(animation), destination = animator[CHANNEL].find(k => k !== keyframe && Math.abs(k.time - time) < 1e-8);
          if (destination) { const added = new g.KeyframeDataPoint(destination); added.extend({ studio_event: event }); destination.data_points.push(added); }
          else animator.addKeyframe({ channel: CHANNEL, time, data_points: [{ studio_event: event }] });
          keyframe.data_points = keyframe.data_points.filter(p => p !== point); if (!keyframe.data_points.length) keyframe.remove();
        }, { animations: [animation] });
      }
      throw new Error('Evento não encontrado.');
    },
    removeEvent(id) {
      return edit('Studio: remover evento', () => {
        for (const animation of g.Animation.all) for (const keyframe of (animation.animators.effects?.[CHANNEL] || []).slice()) {
          keyframe.data_points = keyframe.data_points.filter(p => p.studio_event.id !== id && `${keyframe.uuid}:${p.studio_event.id}` !== id); if (!keyframe.data_points.length) keyframe.remove();
        }
      }, { animations: g.Animation.all.slice() });
    },
    addLocator(name, parent = g.Group.first_selected) {
      if (!parent) throw new Error('Selecione uma pasta para o marcador.');
      const elements = [];
      return edit('Studio: marcador', () => { const locator = new g.Locator({ name, position: parent.origin.slice() }).addTo(parent).init(); elements.push(locator); return locator; }, { elements, outliner: true });
    },
    bind(model, part = 'RightArm', rootUuid = model.roots[0]) {
      if (!BODY_PARTS.includes(part)) throw new Error('Parte corporal inválida.');
      const elements = [], groups = g.Group.all.slice(), textures = [];
      return edit('Studio: encaixar equipamento', () => {
        const before = new Set(g.Outliner.elements), beforeGroups = new Set(g.Group.all), beforeTextures = new Set(g.Texture.all);
        const ref = context(), root = find(rootUuid); if (!root || !model.roots.includes(rootUuid)) throw new Error('Pasta do modelo inválida.');
        root.addTo(find(ref.groups[part])); model.bindings = model.bindings.filter(b => b.group !== rootUuid);
        model.bindings.push({ group: rootUuid, target: part, referenceGroup: ref.groups[part] });
        elements.push(...g.Outliner.elements.filter(e => !before.has(e))); groups.push(...g.Group.all.filter(e => !beforeGroups.has(e))); textures.push(...g.Texture.all.filter(e => !beforeTextures.has(e)));
      }, { elements, groups, textures, outliner: true });
    },
    importSkin(data, beforeCommit = () => {}) {
      const project = g.Project, model = state().models.find(m => m.type === 'player'); if (!model) throw new Error('Crie um projeto/modelo de player para importar a skin.');
      return new Promise((resolve, reject) => { const image = new g.Image(); image.onerror = () => reject(new Error('PNG inválido.'));
        image.onload = () => {
          if (g.Project !== project) return reject(new Error('O projeto mudou durante a importação da skin.'));
          if (image.width !== 64 || image.height !== 64) return reject(new Error('A skin deve ser PNG 64×64.'));
          const texture = g.Texture.all.find(t => t.uuid === model.player.texture); if (!texture) return reject(new Error('Textura da skin não encontrada.'));
          try { beforeCommit(); edit('Studio: importar skin', () => {
            // The decoded pixels are available before Undo captures the resulting bitmap.
            texture.fromDataURL(data); texture.canvas.width = 64; texture.canvas.height = 64; texture.ctx.drawImage(image, 0, 0); texture.updateChangesAfterEdit();
          }, { textures: [texture], bitmap: true }); resolve(texture); } catch (e) { reject(e); }
        }; image.src = data; });
    },
    setSkinLayers(visible) { const ids = state().models.flatMap(m => m.player?.layers || []), elements = ids.map(find).filter(Boolean);
      return edit('Studio: camadas da skin', () => elements.forEach(e => { e.visibility = !!visible; }), { elements }); },
    dispose() {
      studio.preview?.dispose();
      for (const [event, fn] of listeners) g.Blockbench.removeListener(event, fn);
      const hasOpenProjects = g.ModelProject.all.some(project => project.format?.id === FORMAT);
      // Unloading must never strip serializable metadata/events from a still-open project.
      for (const disposable of disposables.reverse()) if (!hasOpenProjects || !dataProperties.has(disposable)) disposable.delete();
      delete g.EffectAnimator.prototype.channels[CHANNEL];
    },
  };
  listen('select_project', () => { syncChannel(); if (isStudio()) for (const animation of g.Animation.all) effectsFor(animation); studio.preview?.reset(); studio.onRefresh?.(); });
  disposables.push(g.Codecs.project.on('parsed', () => { if (isStudio()) { for (const animation of g.Animation.all) effectsFor(animation); repairTemplateUV(); studio.refresh(); } }));
  listen('finished_edit', () => { studio.preview?.invalidate(); studio.onRefresh?.(); });
  for (const event of ['undo', 'redo']) listen(event, () => { if (isStudio()) { for (const animation of g.Animation.all) if (animation.animators.effects) effectsFor(animation); studio.refresh(); } });
  listen('update_keyframe_selection', () => studio.onRefresh?.());
  listen('select_animation', () => { if (isStudio() && g.Animation.selected) effectsFor(g.Animation.selected); studio.preview?.reset(); studio.onRefresh?.(); });
  for (const project of g.ModelProject.all) if (project.format?.id === FORMAT) project.format = format;
  if (isStudio()) { format.select(); for (const animation of g.Animation.all) effectsFor(animation); repairTemplateUV(); }
  return studio;
}
