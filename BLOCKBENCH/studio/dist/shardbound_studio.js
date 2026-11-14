// ShardBound Studio 1.3.0 — Blockbench 5.2.2. Source: BLOCKBENCH/studio/src/
(() => {
  // src/core.mjs
  var VERSION = "1.3.0";
  var FORMAT = "shardbound_studio";
  var CHANNEL = "shardbound_events";
  var SCHEMA_VERSION = 1;
  var MAX_PARTICLES = 4096;
  var TYPES = Object.freeze({
    player: "Player / skin",
    item: "Item",
    melee: "Arma corpo a corpo",
    ranged: "Arma de longa dist\xE2ncia",
    block: "Bloco de cen\xE1rio",
    armor: "Armadura",
    monster: "Monstro",
    effect: "Part\xEDculas / VFX"
  });
  var EFFECT_TYPES = Object.freeze({ particle: "Part\xEDculas", trail: "Rastro", flash: "Flash", glow: "Brilho / luz" });
  var BODY_PARTS = ["Head", "Body", "LeftArm", "RightArm", "LeftLeg", "RightLeg"];
  var clone = (value) => structuredClone(value);
  function resourceId(value) {
    return typeof value === "string" && /^[a-z0-9_.-]+:[a-z0-9_./-]+$/.test(value) && value.replace(":", "/").split("/").every((part) => part && part !== "." && part !== "..");
  }
  function assertId(value) {
    if (!resourceId(value)) throw new Error("Use um ID namespace:path, em min\xFAsculas, sem . ou .. como segmentos.");
    return value;
  }
  function assetPath(id, extension, directory) {
    assertId(id);
    const [namespace, name] = id.split(":");
    return `${directory}/${namespace}/${name}.${extension}`;
  }
  function projectState(type = "monster", id = "custom:model") {
    if (!TYPES[type]) throw new Error("Tipo de projeto desconhecido.");
    assertId(id);
    return {
      schema: "shardbound.studio.project",
      version: SCHEMA_VERSION,
      contentType: type,
      id,
      unitsPerMeter: 16,
      sampleRate: 60,
      skinVariant: "wide",
      models: [],
      effects: [],
      sounds: [],
      materials: {},
      context: null,
      projectileVisual: type === "ranged" ? "core:arrow" : null,
      nextEventOrder: 0,
      renderContract: {
        version: 1,
        filtering: "nearest",
        colorSpace: "srgb",
        particleLighting: "unlit",
        transparency: "alpha-or-additive",
        gravitySpace: "emitter-local",
        particleMotion: "analytic-linear-drag",
        spriteOrientation: "camera-facing",
        trail: "world-space-ribbon",
        glow: "additive-sprite-and-point-light"
      }
    };
  }
  function checkState(state) {
    if (!state || state.schema !== "shardbound.studio.project" || state.version !== SCHEMA_VERSION)
      throw new Error("Projeto Studio ausente ou vers\xE3o n\xE3o suportada. O arquivo original n\xE3o foi alterado.");
    if (!TYPES[state.contentType]) throw new Error("Tipo de conte\xFAdo n\xE3o suportado.");
    assertId(state.id);
    if (!(state.unitsPerMeter > 0 && Number.isFinite(state.unitsPerMeter))) throw new Error("Escala de exporta\xE7\xE3o inv\xE1lida.");
    if (!Number.isFinite(state.sampleRate) || state.sampleRate < 1 || state.sampleRate > 240) throw new Error("Amostragem deve ficar entre 1 e 240 FPS.");
    for (const field of ["models", "effects", "sounds"]) if (!Array.isArray(state[field])) throw new Error(`Campo ${field} inv\xE1lido.`);
    if (!state.materials || typeof state.materials !== "object" || Array.isArray(state.materials)) throw new Error("Materiais inv\xE1lidos.");
    if (!["wide", "slim"].includes(state.skinVariant)) throw new Error("Variante de skin inv\xE1lida.");
    if (state.projectileVisual !== null && !resourceId(state.projectileVisual)) throw new Error("ID de proj\xE9til visual inv\xE1lido.");
    if (!Number.isInteger(state.nextEventOrder) || state.nextEventOrder < 0) throw new Error("Ordem de eventos inv\xE1lida.");
    for (const model of state.models) {
      if (!model || !TYPES[model.type] || !resourceId(model.id) || !Array.isArray(model.roots) || !model.roots.every((id) => typeof id === "string") || !Array.isArray(model.bindings)) throw new Error("Declara\xE7\xE3o de modelo inv\xE1lida.");
      if (model.bindings.some((binding) => !binding || !["group", "referenceGroup", "target"].every((field) => typeof binding[field] === "string") || !BODY_PARTS.includes(binding.target))) throw new Error("V\xEDnculo de modelo inv\xE1lido.");
    }
    if ([...state.effects, ...state.sounds].some((resource) => !resource || typeof resource !== "object" || !resourceId(resource.id))) throw new Error("Recurso de efeito/som inv\xE1lido.");
    if (state.context && (typeof state.context.root !== "string" || !state.context.groups || !BODY_PARTS.every((part) => typeof state.context.groups[part] === "string") || !Array.isArray(state.context.geometry))) throw new Error("Contexto de player inv\xE1lido.");
    return state;
  }
  function defaultEffect(kind = "particle", id = "custom:effect") {
    if (!EFFECT_TYPES[kind]) throw new Error("Tipo de efeito desconhecido.");
    assertId(id);
    return {
      id,
      kind,
      duration: 1,
      lifetime: 0.7,
      burst: 16,
      rate: 16,
      seed: 1,
      shape: "point",
      shapeSize: [1, 1, 1],
      speed: 5,
      speedSpread: 1,
      direction: [0, 1, 0],
      spread: 45,
      gravity: [0, -9.81, 0],
      drag: 0,
      size: 0.5,
      sizeEnd: 0,
      color: "#ffffff",
      colorEnd: "#ff9b33",
      opacity: 1,
      opacityEnd: 0,
      rotation: 0,
      angularVelocity: 0,
      texture: "",
      blend: "alpha",
      columns: 1,
      rows: 1,
      frames: 1,
      fps: 12,
      curves: {},
      trailLength: 0.35,
      trailSamples: 24,
      width: 1,
      startOffset: [0, 0, 0],
      endOffset: [0, 5, 0],
      intensity: 2,
      distance: 12,
      follow: false
    };
  }
  function validateEffect(effect) {
    const errors = [];
    const bad = (message) => errors.push(`${effect?.id || "Efeito"}: ${message}`);
    if (!resourceId(effect?.id)) bad("ID inv\xE1lido.");
    if (!EFFECT_TYPES[effect?.kind]) bad("tipo desconhecido.");
    const ranges = {
      duration: [1e-3, 3600],
      lifetime: [1e-3, 120],
      burst: [0, MAX_PARTICLES],
      rate: [0, MAX_PARTICLES],
      seed: [0, 4294967295],
      speed: [0, 1e4],
      speedSpread: [0, 1e4],
      spread: [0, 180],
      drag: [0, 100],
      size: [0, 1e4],
      sizeEnd: [0, 1e4],
      opacity: [0, 1],
      opacityEnd: [0, 1],
      rotation: [-36e3, 36e3],
      angularVelocity: [-36e3, 36e3],
      columns: [1, 256],
      rows: [1, 256],
      frames: [1, 65536],
      fps: [0, 240],
      trailLength: [1e-3, 30],
      trailSamples: [2, 240],
      width: [1e-3, 1e4],
      intensity: [0, 1e4],
      distance: [0, 1e4]
    };
    for (const [key, [min, max]] of Object.entries(ranges)) if (!Number.isFinite(effect?.[key]) || effect[key] < min || effect[key] > max) bad(`${key} deve ficar entre ${min} e ${max}.`);
    for (const key of ["burst", "seed", "columns", "rows", "frames", "trailSamples"]) if (!Number.isInteger(effect?.[key])) bad(`${key} deve ser inteiro.`);
    for (const key of ["shapeSize", "direction", "gravity", "startOffset", "endOffset"])
      if (!Array.isArray(effect?.[key]) || effect[key].length !== 3 || !effect[key].every((v) => Number.isFinite(v) && Math.abs(v) <= 1e6)) bad(`${key} deve ter tr\xEAs n\xFAmeros finitos.`);
    if (!["point", "box", "sphere", "cone"].includes(effect?.shape)) bad("forma de emiss\xE3o inv\xE1lida.");
    if (Array.isArray(effect?.shapeSize) && effect.shapeSize.some((value) => value < 0)) bad("dimens\xF5es da emiss\xE3o n\xE3o podem ser negativas.");
    if (!["alpha", "additive"].includes(effect?.blend)) bad("mistura inv\xE1lida.");
    if (!/^#[a-f0-9]{6}$/i.test(effect?.color) || !/^#[a-f0-9]{6}$/i.test(effect?.colorEnd)) bad("cor inv\xE1lida.");
    if (effect?.frames > effect?.columns * effect?.rows) bad("flipbook excede a grade do sprite.");
    if (effect?.kind === "particle" && effect.burst + Math.ceil(effect.rate * Math.min(effect.duration, effect.lifetime)) > MAX_PARTICLES)
      bad(`emiss\xE3o excede o limite expl\xEDcito de ${MAX_PARTICLES} part\xEDculas simult\xE2neas por efeito.`);
    if (!effect?.curves || typeof effect.curves !== "object" || Array.isArray(effect.curves)) bad("curvas inv\xE1lidas.");
    else for (const [key, points] of Object.entries(effect.curves)) {
      if (!["size", "opacity", "rotation", "color"].includes(key)) {
        bad(`curva ${key} desconhecida.`);
        continue;
      }
      if (!Array.isArray(points) || points.length < 2 || points.length > 64) {
        bad(`curva ${key} exige de 2 a 64 pontos.`);
        continue;
      }
      const times = /* @__PURE__ */ new Set();
      for (const p of points) {
        if (!Number.isFinite(p?.age) || p.age < 0 || p.age > 1 || times.has(p.age)) bad(`tempo inv\xE1lido ou repetido na curva ${key}.`);
        times.add(p?.age);
        if (key === "color" ? !/^#[a-f0-9]{6}$/i.test(p?.value) : !Number.isFinite(p?.value)) bad(`valor inv\xE1lido na curva ${key}.`);
        if (key === "opacity" && (p?.value < 0 || p?.value > 1)) bad("opacidade da curva fora de 0\u20131.");
        if (key === "size" && p?.value < 0) bad("tamanho da curva negativo.");
      }
    }
    return errors;
  }
  function stableJson(value) {
    const sort = (v) => Array.isArray(v) ? v.map(sort) : v && typeof v === "object" ? Object.fromEntries(Object.keys(v).sort().filter((k) => v[k] !== void 0).map((k) => [k, sort(v[k])])) : v;
    return JSON.stringify(sort(value), null, 2) + "\n";
  }
  function orderedEvents(events) {
    return events.slice().sort((a, b) => a.time - b.time || (a.order ?? 0) - (b.order ?? 0) || String(a.id).localeCompare(String(b.id)));
  }
  function eventIntervals(events, definitions, end) {
    const result = [], active = /* @__PURE__ */ new Map();
    for (const event of orderedEvents(events)) {
      const definition = definitions.find((e) => e.id === event.resource);
      const key = event.instance || `${event.resource}|${event.target || ""}`;
      if (event.action === "stop") {
        const entry2 = active.get(key);
        if (entry2) {
          entry2.end = Math.min(entry2.end, event.time);
          active.delete(key);
        }
        continue;
      }
      if (!definition || event.type === "sound") continue;
      if (event.action === "start" && active.has(key)) {
        active.get(key).end = Math.min(active.get(key).end, event.time);
        active.delete(key);
      }
      const entry = {
        event,
        definition,
        start: event.time,
        end: Math.min(end, event.time + definition.duration),
        continuous: event.action === "start"
      };
      result.push(entry);
      if (entry.continuous) active.set(key, entry);
    }
    return result;
  }
  function crossedEvents(events, previous, now, { length = 0, looped = false, initial = false } = {}) {
    const sorted = orderedEvents(events);
    if (initial) return sorted.filter((e) => e.time <= now && e.time >= 0);
    if (looped && length > 0) return sorted.filter((e) => e.time > previous && e.time <= length).concat(sorted.filter((e) => e.time <= now));
    if (now < previous) return [];
    return sorted.filter((e) => e.time > previous && e.time <= now);
  }

  // src/templates.mjs
  function playerParts(variant = "wide") {
    if (!["wide", "slim"].includes(variant)) throw new Error("Variante de bra\xE7o inv\xE1lida.");
    const w = variant === "slim" ? 3 : 4;
    return [
      { name: "Head", origin: [0, 24, 0], from: [-4, 24, -4], to: [4, 32, 4], uv: [0, 0], outerUv: [32, 0], inflate: 0.5 },
      { name: "Body", origin: [0, 24, 0], from: [-4, 12, -2], to: [4, 24, 2], uv: [16, 16], outerUv: [16, 32], inflate: 0.25 },
      { name: "LeftArm", origin: [5, 22, 0], from: [4, 12, -2], to: [4 + w, 24, 2], uv: [32, 48], outerUv: [48, 48], inflate: 0.25 },
      { name: "RightArm", origin: [-5, 22, 0], from: [-4 - w, 12, -2], to: [-4, 24, 2], uv: [40, 16], outerUv: [40, 32], inflate: 0.25 },
      { name: "LeftLeg", origin: [2, 12, 0], from: [0, 0, -2], to: [4, 12, 2], uv: [16, 48], outerUv: [0, 48], inflate: 0.25 },
      { name: "RightLeg", origin: [-2, 12, 0], from: [-4, 0, -2], to: [0, 12, 2], uv: [0, 16], outerUv: [0, 32], inflate: 0.25 }
    ];
  }
  function skinFaces([u, v], [w, h, d]) {
    return {
      east: [u, v + d, u + d, v + d + h],
      north: [u + d, v + d, u + d + w, v + d + h],
      west: [u + d + w, v + d, u + 2 * d + w, v + d + h],
      south: [u + 2 * d + w, v + d, u + 2 * d + 2 * w, v + d + h],
      up: [u + d + w, v + d, u + d, v],
      down: [u + d + 2 * w, v, u + d + w, v + d]
    };
  }
  function neutralSkinCanvas(g) {
    const canvas = g.document.createElement("canvas");
    canvas.width = canvas.height = 64;
    const ctx = canvas.getContext("2d");
    ctx.clearRect(0, 0, 64, 64);
    for (const part of playerParts()) for (const rect of Object.values(skinFaces(part.uv, part.to.map((n, i) => n - part.from[i])))) {
      ctx.fillStyle = part.name === "Head" ? "#bbbbbb" : "#999999";
      ctx.fillRect(rect[0], rect[1], rect[2] - rect[0], rect[3] - rect[1]);
    }
    return canvas;
  }
  function makePlayer(g, { variant = "wide", reference = false, rootName = "Player" } = {}) {
    const root = new g.Group({ name: rootName, origin: [0, 0, 0] }).init();
    const groups = {}, geometry = [], layers = [];
    const texture = new g.Texture({ name: reference ? "Refer\xEAncia neutra" : "Skin 64x64", width: 64, height: 64, uv_width: 64, uv_height: 64 });
    const skin = neutralSkinCanvas(g);
    texture.canvas.width = texture.canvas.height = 64;
    texture.ctx.drawImage(skin, 0, 0);
    texture.fromDataURL(skin.toDataURL("image/png")).add();
    for (const part of playerParts(variant)) {
      const group = new g.Group({ name: part.name, origin: part.origin }).addTo(root).init();
      groups[part.name] = group.uuid;
      for (const layer of reference ? [false] : [false, true]) {
        const faces = Object.fromEntries(Object.entries(skinFaces(layer ? part.outerUv : part.uv, part.to.map((n, i) => n - part.from[i]))).map(([direction2, uv]) => [direction2, { uv, texture: texture.uuid }]));
        const cube = new g.Cube({
          name: part.name + (layer ? "_layer" : ""),
          from: part.from,
          to: part.to,
          origin: part.origin,
          faces,
          box_uv: false,
          inflate: layer ? part.inflate : 0
        }).addTo(group).init();
        geometry.push(cube.uuid);
        if (layer) layers.push(cube.uuid);
      }
    }
    if (!BODY_PARTS.every((name) => groups[name])) throw new Error("Template de player incompleto.");
    return { root: root.uuid, groups, geometry, layers, texture: texture.uuid, variant };
  }

  // src/controller.mjs
  function createStudio(g) {
    const disposables = [], listeners = [], dataProperties = /* @__PURE__ */ new Set();
    const listen = (event, fn) => {
      g.Blockbench.on(event, fn);
      listeners.push([event, fn]);
    };
    const property = (target, type, name, options) => {
      const p = target.properties[name] || new g.Property(target, type, name, options);
      disposables.push(p);
      dataProperties.add(p);
      return p;
    };
    const isStudio = () => g.Format?.id === FORMAT;
    const state = () => {
      if (!isStudio()) throw new Error("Abra um projeto ShardBound Studio.");
      if (!g.Project.shardbound_studio || !Object.keys(g.Project.shardbound_studio).length) g.Project.shardbound_studio = projectState();
      return checkState(g.Project.shardbound_studio);
    };
    property(g.ModelProject, "object", "shardbound_studio", { default: () => ({}), condition: () => isStudio(), exposed: false });
    property(g.KeyframeDataPoint, "object", "studio_event", {
      default: () => ({}),
      exposed: false,
      condition: (point) => point.keyframe.channel === CHANNEL
    });
    const channelDescriptor = { name: "ShardBound: efeitos e sons", mutable: true, max_data_points: 1, condition: isStudio };
    const syncChannel = () => {
      if (isStudio()) g.EffectAnimator.prototype.channels[CHANNEL] = channelDescriptor;
      else delete g.EffectAnimator.prototype.channels[CHANNEL];
    };
    syncChannel();
    for (const project of g.ModelProject.all.filter((project2) => project2.format?.id === FORMAT)) for (const animation of project.animations || []) {
      const animator = animation.animators.effects;
      if (animator && !Array.isArray(animator[CHANNEL])) {
        g.Vue.set(animator, CHANNEL, []);
        g.Vue.set(animator.muted, CHANNEL, false);
      }
    }
    const format = new g.ModelFormat(FORMAT, {
      name: "ShardBound Studio",
      description: "Modelos, skins, anima\xE7\xF5es e efeitos ShardBound",
      icon: "auto_awesome",
      category: "general",
      target: ["ShardBound"],
      meshes: true,
      rotate_cubes: true,
      bone_rig: true,
      armature_rig: false,
      centered_grid: true,
      optional_box_uv: true,
      box_uv_float_size: true,
      per_texture_uv_size: true,
      uv_rotation: true,
      animation_mode: true,
      locators: true,
      quaternion_interpolation: false,
      euler_order: "ZYX",
      animation_files: false,
      can_convert_to: false,
      new() {
        studio2.ui?.handlers.newProject();
        return true;
      },
      onSetup(project) {
        if (!project.shardbound_studio?.schema) project.shardbound_studio = projectState();
      }
    });
    disposables.push(format);
    const codec = new g.Codec(FORMAT, {
      name: "Projeto ShardBound Studio",
      extension: "sbstudio",
      remember: true,
      format,
      load_filter: { type: "json", extensions: ["sbstudio"], condition: (data) => !!data?.shardbound_studio },
      compile(options = {}) {
        state();
        return g.Codecs.project.compile({ ...options, bitmaps: true, absolute_paths: false });
      },
      load(data, file = { path: "", no_file: true }) {
        checkState(data?.shardbound_studio);
        const effectErrors = data.shardbound_studio.effects.flatMap(validateEffect);
        if (effectErrors.length) throw new Error(effectErrors.join("\n"));
        if (data.meta?.model_format !== FORMAT) throw new Error("O arquivo n\xE3o cont\xE9m um projeto Studio.");
        g.Codecs.project.load(clone(data), { ...file, no_file: file.no_file ?? !file.path });
        for (const animation of g.Animation.all) effectsFor(animation);
        studio2.refresh();
      },
      export() {
        const project = g.Project;
        g.Blockbench.export({
          resource_id: FORMAT,
          type: "Projeto ShardBound Studio",
          extensions: ["sbstudio"],
          name: g.Project.name || "studio",
          startpath: g.Project.save_path,
          content: this.compile(),
          custom_writer: (content, path) => {
            g.Blockbench.writeFile(path, { content }, () => {
              project.save_path = path;
              project.saved = true;
            });
          }
        });
      }
    });
    disposables.push(codec);
    format.codec = codec;
    function effectsFor(animation) {
      const animator = animation.animators.effects ||= new g.EffectAnimator(animation);
      if (animator.studioController === studio2) return animator;
      animator.studioController = studio2;
      animator.channels = { [CHANNEL]: channelDescriptor };
      for (const channel of ["particle", "sound", "timeline", CHANNEL]) if (!Array.isArray(animator[channel])) animator[channel] = [];
      animator.createKeyframe = function(value, time, channel, ...args) {
        if (channel === CHANNEL) return studio2.onRequestEvent?.(animation, time ?? g.Timeline.time);
        return g.EffectAnimator.prototype.createKeyframe.call(this, value, time, channel, ...args);
      };
      return animator;
    }
    const find = (uuid) => g.Group.all.find((e) => e.uuid === uuid) || g.Outliner.elements.find((e) => e.uuid === uuid);
    function repairTemplateUV() {
      if (!isStudio()) return;
      const s = state(), repaired = [];
      for (const player of [s.context, ...s.models.map((m) => m.player)].filter(Boolean)) {
        const texture = g.Texture.all.find((t) => t.uuid === player.texture);
        if (!texture || texture.uv_width !== 32 || texture.uv_height !== 32) continue;
        const parts = playerParts(player.variant || s.skinVariant);
        const knownLayout = player.geometry.length && player.geometry.every((id) => {
          const cube = find(id), part = parts.find((p) => p.name === cube?.name?.replace(/_layer$/, ""));
          if (!part || cube.box_uv) return false;
          const expected = skinFaces(cube.name.endsWith("_layer") ? part.outerUv : part.uv, part.to.map((v, i) => v - part.from[i]));
          return Object.entries(expected).every(([face, uv]) => cube.faces[face]?.texture === texture.uuid && uv.every((v, i) => v === cube.faces[face].uv[i]));
        });
        if (knownLayout) repaired.push(texture);
      }
      if (repaired.length) {
        edit("Studio: corrigir escala UV do template 64\xD764", () => repaired.forEach((t) => {
          t.uv_width = t.uv_height = 64;
        }), { textures: repaired, uv_only: true });
        g.Blockbench.showQuickMessage("Escala UV 64\xD764 corrigida nos templates do player. A altera\xE7\xE3o pode ser desfeita.", 6e3);
      }
    }
    function edit(label, fn, aspects = {}) {
      state();
      if (g.Undo.current_save) throw new Error("Conclua a edi\xE7\xE3o ativa primeiro.");
      g.Undo.initEdit({ studio: true, ...aspects });
      try {
        const result = fn();
        g.Undo.finishEdit(label);
        studio2.refresh();
        return result;
      } catch (error) {
        g.Undo.cancelEdit(true);
        studio2.refresh();
        throw error;
      }
    }
    listen("create_undo_save", ({ save, aspects }) => {
      if (isStudio() && aspects.studio) save.shardbound_studio = clone(g.Project.shardbound_studio);
    });
    listen("load_undo_save", ({ save }) => {
      if (isStudio() && save.shardbound_studio) {
        g.Project.shardbound_studio = clone(save.shardbound_studio);
        studio2.refresh();
      }
    });
    function context() {
      const s = state();
      if (!s.context) s.context = makePlayer(g, { variant: s.skinVariant, reference: true, rootName: "Player de refer\xEAncia" });
      return s.context;
    }
    function addModel(type = "item", options = {}) {
      const s = state();
      if (!TYPES[type]) throw new Error("Tipo de modelo inv\xE1lido.");
      const id = assertId(options.id || `${s.id}_${s.models.length + 1}`);
      if (s.models.some((m) => m.id === id)) throw new Error("J\xE1 existe um modelo com esse ID.");
      const model = { id, type, name: options.name || TYPES[type], roots: [], bindings: [] };
      if (type === "player") {
        const player = makePlayer(g, { variant: s.skinVariant });
        model.roots = [player.root];
        model.player = player;
      } else if (type === "armor") {
        const ref = context();
        for (const part of playerParts(s.skinVariant)) {
          const root = new g.Group({ name: `Armadura_${part.name}`, origin: part.origin }).addTo(find(ref.groups[part.name])).init();
          model.roots.push(root.uuid);
          model.bindings.push({ group: root.uuid, target: part.name, referenceGroup: ref.groups[part.name] });
        }
      } else {
        let parent = "root", origin = [0, 0, 0];
        if (["melee", "ranged"].includes(type)) {
          const ref = context();
          parent = find(ref.groups.RightArm);
          origin = [-5, 16, 0];
        }
        const root = new g.Group({ name: options.name || type, origin }).addTo(parent).init();
        model.roots.push(root.uuid);
        if (parent !== "root") model.bindings.push({ group: root.uuid, target: "RightArm", referenceGroup: parent.uuid });
        if (type === "block") new g.Cube({ name: "Bloco", from: [0, 0, 0], to: [16, 16, 16] }).addTo(root).init();
      }
      s.models.push(model);
      return model;
    }
    const studio2 = {
      version: VERSION,
      apiVersion: 1,
      format,
      codec,
      isStudio,
      state,
      find,
      edit,
      onRefresh: null,
      onRequestEvent: null,
      preview: null,
      refresh() {
        if (isStudio()) {
          g.Canvas.updateAll();
          g.updateSelection();
        }
        studio2.preview?.invalidate();
        studio2.onRefresh?.();
      },
      createProject(type = "monster", options = {}) {
        if (!TYPES[type]) throw new Error("Tipo de projeto inv\xE1lido.");
        assertId(options.id || "custom:model");
        if (!g.newProject(format)) throw new Error("N\xE3o foi poss\xEDvel criar o projeto.");
        g.Project.name = options.name || TYPES[type];
        g.Project.texture_width = g.Project.texture_height = type === "player" ? 64 : 32;
        g.Project.shardbound_studio = projectState(type, options.id || "custom:model");
        state().skinVariant = options.variant || "wide";
        const model = addModel(type, { ...options, id: state().id });
        studio2.refresh();
        return model;
      },
      addModel(type, options) {
        const elements = [], groups = [], textures = [];
        return edit("Studio: adicionar modelo", () => {
          const before = new Set(g.Outliner.elements), beforeG = new Set(g.Group.all), beforeT = new Set(g.Texture.all);
          const result = addModel(type, options);
          elements.push(...g.Outliner.elements.filter((e) => !before.has(e)));
          groups.push(...g.Group.all.filter((e) => !beforeG.has(e)));
          textures.push(...g.Texture.all.filter((e) => !beforeT.has(e)));
          return result;
        }, { elements, groups, textures, outliner: true });
      },
      selectModel(model) {
        model.roots.map(find).filter(Boolean).forEach((root, i) => i ? root.multiSelect?.() : root.select());
      },
      renameModel(model, id, name) {
        assertId(id);
        if (state().models.some((m) => m !== model && m.id === id)) throw new Error("ID de modelo repetido.");
        return edit("Studio: propriedades do modelo", () => {
          model.id = id;
          model.name = name;
        });
      },
      removeModel(model) {
        return edit("Studio: remover modelo", () => {
          model.roots.map(find).filter(Boolean).forEach((root) => root.remove());
          state().models = state().models.filter((m) => m.id !== model.id);
        }, { elements: g.Outliner.elements.slice(), groups: g.Group.all.slice(), outliner: true });
      },
      createClip(name = "idle", length = 1, loop = "loop", fps = 24) {
        if (!name.trim() || g.Animation.all.some((a) => a.name === name)) throw new Error("Nome de clip vazio ou repetido.");
        if (!(length > 0 && length <= 1e3) || !Number.isFinite(fps) || fps < 1 || fps > 120 || !["once", "loop", "hold"].includes(loop)) throw new Error("Use dura\xE7\xE3o at\xE9 1000 s e timeline de 1\u2013120 FPS, limites nativos do Blockbench 5.2.2.");
        const animations = [];
        return edit("Studio: criar clip", () => {
          const animation = new g.Animation({ name, length, loop, snapping: fps }).add();
          animations.push(animation);
          g.Modes.options.animate.select();
          animation.select();
          effectsFor(animation).addToTimeline(true);
          return animation;
        }, { animations });
      },
      addEffect(kind, id, values = {}) {
        const effect = Object.assign(defaultEffect(kind, id), clone(values));
        const errors = validateEffect(effect);
        if (errors.length) throw new Error(errors.join("\n"));
        if ([...state().effects, ...state().sounds].some((e) => e.id === effect.id)) throw new Error("ID de efeito/som repetido.");
        return edit("Studio: criar efeito", () => {
          state().effects.push(effect);
          return effect;
        });
      },
      updateEffect(id, values) {
        const index = state().effects.findIndex((e) => e.id === id);
        if (index < 0) throw new Error("Efeito n\xE3o encontrado.");
        const effect = Object.assign(clone(state().effects[index]), clone(values));
        const errors = validateEffect(effect);
        if (errors.length) throw new Error(errors.join("\n"));
        if (effect.id !== id && [...state().effects, ...state().sounds].some((e) => e.id === effect.id)) throw new Error("ID de efeito/som repetido.");
        for (const event of studio2.allEvents().filter((e) => e.resource === id)) studio2.validateEvent({ ...event, resource: effect.id, type: effect.kind }, effect);
        return edit("Studio: editar efeito", () => {
          state().effects[index] = effect;
          for (const animation of g.Animation.all) for (const kf of animation.animators.effects?.[CHANNEL] || []) for (const point of kf.data_points)
            if (point.studio_event.resource === id) {
              point.studio_event.resource = effect.id;
              point.studio_event.type = effect.kind;
            }
          return effect;
        }, { animations: g.Animation.all.slice() });
      },
      removeEffect(id) {
        if (studio2.allEvents().some((e) => e.resource === id)) throw new Error("Remova os keyframes que referenciam esse efeito primeiro.");
        return edit("Studio: remover efeito", () => {
          state().effects = state().effects.filter((e) => e.id !== id);
        });
      },
      async addSound(id, data, filename = "", beforeCommit = () => {
      }, placement = null) {
        assertId(id);
        if ([...state().effects, ...state().sounds].some((e) => e.id === id)) throw new Error("ID de efeito/som repetido.");
        if (!/^data:audio\/(wav|x-wav|ogg|mpeg);base64,[A-Za-z0-9+/=]+$/.test(data) || data.length > 32 * 1024 * 1024)
          throw new Error("Use \xE1udio WAV, OGG ou MP3, at\xE9 24 MiB.");
        const project = g.Project, audio = new g.AudioContext();
        let decoded;
        try {
          decoded = await audio.decodeAudioData(Uint8Array.from(g.atob(data.split(",")[1]), (c) => c.charCodeAt(0)).buffer);
        } catch {
          throw new Error("O \xE1udio n\xE3o pode ser decodificado pelo Blockbench. Verifique o arquivo WAV, OGG ou MP3.");
        } finally {
          await audio.close();
        }
        if (g.Project !== project) throw new Error("O projeto mudou durante a importa\xE7\xE3o do \xE1udio.");
        if ([...state().effects, ...state().sounds].some((e) => e.id === id)) throw new Error("ID de efeito/som repetido.");
        beforeCommit();
        const sound = { id, data, filename, duration: decoded.duration };
        if (placement?.eventId) {
          const point = (placement.animation?.animators.effects?.[CHANNEL] || []).flatMap((k) => k.data_points.map((p) => ({ point: p, keyframe: k }))).find(({ point: point2, keyframe }) => point2.studio_event.id === placement.eventId || `${keyframe.uuid}:${point2.studio_event.id}` === placement.eventId)?.point;
          if (!point) throw new Error("O evento de \xE1udio n\xE3o existe mais.");
          const event = { ...clone(point.studio_event), resource: id, type: "sound" };
          studio2.validateEvent(event, sound);
          return edit("Studio: substituir \xE1udio no evento", () => {
            state().sounds.push(sound);
            point.studio_event = event;
            return sound;
          }, { animations: [placement.animation] });
        }
        if (placement) {
          studio2.addEvent(placement.animation, placement.time, { type: "sound", resource: id, target: "", action: "burst" }, sound);
          return sound;
        }
        return edit("Studio: importar som", () => {
          state().sounds.push(sound);
          return sound;
        });
      },
      removeSound(id) {
        if (studio2.allEvents().some((e) => e.resource === id)) throw new Error("Remova os keyframes desse som primeiro.");
        return edit("Studio: remover som", () => {
          state().sounds = state().sounds.filter((s) => s.id !== id);
        });
      },
      addEvent(animation, time, values, newDefinition = null) {
        state();
        if (!animation || !g.Animation.all.includes(animation)) throw new Error("Selecione um clip.");
        if (!Number.isFinite(time) || time < 0 || time > animation.length) throw new Error("O frame deve estar dentro da dura\xE7\xE3o do clip.");
        const data = {
          id: g.guid(),
          type: "particle",
          action: "burst",
          target: "",
          offset: [0, 0, 0],
          follow: false,
          instance: "",
          volume: 1,
          pitch: 1,
          loop: false,
          ...clone(values),
          order: state().nextEventOrder
        };
        if (newDefinition) {
          if (newDefinition.kind) {
            const errors = validateEffect(newDefinition);
            if (errors.length) throw new Error(errors.join("\n"));
          } else if (data.type !== "sound" || !(newDefinition.duration > 0) || !/^data:audio\/(wav|ogg|mpeg);base64,/.test(newDefinition.data)) throw new Error("\xC1udio novo inv\xE1lido.");
          if (data.resource !== newDefinition.id || [...state().effects, ...state().sounds].some((e) => e.id === newDefinition.id)) throw new Error("Recurso novo inv\xE1lido ou repetido.");
        }
        studio2.validateEvent(data, newDefinition);
        const keyframes = [], animations = [animation];
        return edit("Studio: evento na timeline", () => {
          if (newDefinition) state()[newDefinition.kind ? "effects" : "sounds"].push(clone(newDefinition));
          state().nextEventOrder++;
          const animator = effectsFor(animation);
          animator.addToTimeline(true);
          let keyframe = animator[CHANNEL].find((k) => Math.abs(k.time - time) < 1e-8);
          if (keyframe) {
            const point = new g.KeyframeDataPoint(keyframe);
            point.extend({ studio_event: data });
            keyframe.data_points.push(point);
          } else {
            keyframe = animator.addKeyframe({ channel: CHANNEL, time, data_points: [{ studio_event: data }] });
            keyframes.push(keyframe);
          }
          keyframe.select();
          return { keyframe, event: data };
        }, { keyframes, animations });
      },
      validateEvent(event, resourceOverride = null) {
        if (typeof event.id !== "string" || !event.id || !Number.isInteger(event.order) || event.order < 0 || typeof event.target !== "string" || typeof event.instance !== "string" || typeof event.follow !== "boolean" || typeof event.loop !== "boolean") throw new Error("Identifica\xE7\xE3o ou configura\xE7\xE3o de evento inv\xE1lida.");
        if (!["particle", "trail", "flash", "glow", "sound"].includes(event.type)) throw new Error("Tipo de evento desconhecido.");
        if (!["burst", "start", "stop"].includes(event.action)) throw new Error("A\xE7\xE3o de evento inv\xE1lida.");
        const resource = resourceOverride || (event.type === "sound" ? state().sounds : state().effects).find((r) => r.id === event.resource);
        if (!resource || event.type !== "sound" && resource.kind !== event.type) throw new Error("Recurso n\xE3o encontrado ou incompat\xEDvel com o tipo do evento.");
        if (event.target && !find(event.target)) throw new Error("Pasta/marcador do evento n\xE3o encontrado.");
        if (["trail", "flash"].includes(event.type) && !event.target) throw new Error("Escolha uma pasta ou marcador para o rastro/flash.");
        if (event.type === "flash") {
          const target = find(event.target), node = target instanceof g.Locator ? target.parent : target;
          const geometry = (candidate) => candidate instanceof g.Cube || candidate instanceof g.Mesh || candidate?.children?.some(geometry);
          if (!geometry(node)) throw new Error("O alvo do flash deve conter geometria.");
        }
        if (!Array.isArray(event.offset) || event.offset.length !== 3 || !event.offset.every(Number.isFinite)) throw new Error("Offset do evento inv\xE1lido.");
        if (!Number.isFinite(event.volume) || event.volume < 0 || event.volume > 1 || !Number.isFinite(event.pitch) || event.pitch < 0.1 || event.pitch > 4) throw new Error("Volume/pitch inv\xE1lido.");
        return event;
      },
      events(animation = g.Animation.selected) {
        return orderedEvents((animation?.animators.effects?.[CHANNEL] || []).flatMap((keyframe) => keyframe.data_points.map((point) => ({ ...clone(point.studio_event), id: `${keyframe.uuid}:${point.studio_event.id}`, time: keyframe.time, frame: Math.round(keyframe.time * animation.snapping), fps: animation.snapping, clip: animation.name, keyframe: keyframe.uuid }))));
      },
      allEvents() {
        return g.Animation.all.flatMap((animation) => studio2.events(animation));
      },
      updateEvent(eventId, values, time) {
        for (const animation of g.Animation.all) for (const keyframe of animation.animators.effects?.[CHANNEL] || []) {
          const point = keyframe.data_points.find((p) => p.studio_event.id === eventId || `${keyframe.uuid}:${p.studio_event.id}` === eventId);
          if (!point) continue;
          const event = { ...clone(point.studio_event), ...clone(values) };
          studio2.validateEvent(event);
          if (time !== void 0 && (!Number.isFinite(time) || time < 0 || time > animation.length)) throw new Error("Frame fora do clip.");
          return edit("Studio: editar evento", () => {
            point.studio_event = event;
            if (time === void 0 || Math.abs(time - keyframe.time) < 1e-8) return;
            const animator = effectsFor(animation), destination = animator[CHANNEL].find((k) => k !== keyframe && Math.abs(k.time - time) < 1e-8);
            if (destination) {
              const added = new g.KeyframeDataPoint(destination);
              added.extend({ studio_event: event });
              destination.data_points.push(added);
            } else animator.addKeyframe({ channel: CHANNEL, time, data_points: [{ studio_event: event }] });
            keyframe.data_points = keyframe.data_points.filter((p) => p !== point);
            if (!keyframe.data_points.length) keyframe.remove();
          }, { animations: [animation] });
        }
        throw new Error("Evento n\xE3o encontrado.");
      },
      removeEvent(id) {
        return edit("Studio: remover evento", () => {
          for (const animation of g.Animation.all) for (const keyframe of (animation.animators.effects?.[CHANNEL] || []).slice()) {
            keyframe.data_points = keyframe.data_points.filter((p) => p.studio_event.id !== id && `${keyframe.uuid}:${p.studio_event.id}` !== id);
            if (!keyframe.data_points.length) keyframe.remove();
          }
        }, { animations: g.Animation.all.slice() });
      },
      addLocator(name, parent = g.Group.first_selected) {
        if (!parent) throw new Error("Selecione uma pasta para o marcador.");
        const elements = [];
        return edit("Studio: marcador", () => {
          const locator = new g.Locator({ name, position: parent.origin.slice() }).addTo(parent).init();
          elements.push(locator);
          return locator;
        }, { elements, outliner: true });
      },
      bind(model, part = "RightArm", rootUuid = model.roots[0]) {
        if (!BODY_PARTS.includes(part)) throw new Error("Parte corporal inv\xE1lida.");
        const elements = [], groups = g.Group.all.slice(), textures = [];
        return edit("Studio: encaixar equipamento", () => {
          const before = new Set(g.Outliner.elements), beforeGroups = new Set(g.Group.all), beforeTextures = new Set(g.Texture.all);
          const ref = context(), root = find(rootUuid);
          if (!root || !model.roots.includes(rootUuid)) throw new Error("Pasta do modelo inv\xE1lida.");
          root.addTo(find(ref.groups[part]));
          model.bindings = model.bindings.filter((b) => b.group !== rootUuid);
          model.bindings.push({ group: rootUuid, target: part, referenceGroup: ref.groups[part] });
          elements.push(...g.Outliner.elements.filter((e) => !before.has(e)));
          groups.push(...g.Group.all.filter((e) => !beforeGroups.has(e)));
          textures.push(...g.Texture.all.filter((e) => !beforeTextures.has(e)));
        }, { elements, groups, textures, outliner: true });
      },
      importSkin(data, beforeCommit = () => {
      }) {
        const project = g.Project, model = state().models.find((m) => m.type === "player");
        if (!model) throw new Error("Crie um projeto/modelo de player para importar a skin.");
        return new Promise((resolve, reject) => {
          const image = new g.Image();
          image.onerror = () => reject(new Error("PNG inv\xE1lido."));
          image.onload = () => {
            if (g.Project !== project) return reject(new Error("O projeto mudou durante a importa\xE7\xE3o da skin."));
            if (image.width !== 64 || image.height !== 64) return reject(new Error("A skin deve ser PNG 64\xD764."));
            const texture = g.Texture.all.find((t) => t.uuid === model.player.texture);
            if (!texture) return reject(new Error("Textura da skin n\xE3o encontrada."));
            try {
              beforeCommit();
              edit("Studio: importar skin", () => {
                texture.fromDataURL(data);
                texture.canvas.width = 64;
                texture.canvas.height = 64;
                texture.ctx.drawImage(image, 0, 0);
                texture.updateChangesAfterEdit();
              }, { textures: [texture], bitmap: true });
              resolve(texture);
            } catch (e) {
              reject(e);
            }
          };
          image.src = data;
        });
      },
      setSkinLayers(visible) {
        const ids = state().models.flatMap((m) => m.player?.layers || []), elements = ids.map(find).filter(Boolean);
        return edit("Studio: camadas da skin", () => elements.forEach((e) => {
          e.visibility = !!visible;
        }), { elements });
      },
      dispose() {
        studio2.preview?.dispose();
        for (const [event, fn] of listeners) g.Blockbench.removeListener(event, fn);
        const hasOpenProjects = g.ModelProject.all.some((project) => project.format?.id === FORMAT);
        for (const disposable of disposables.reverse()) if (!hasOpenProjects || !dataProperties.has(disposable)) disposable.delete();
        delete g.EffectAnimator.prototype.channels[CHANNEL];
      }
    };
    listen("select_project", () => {
      syncChannel();
      if (isStudio()) for (const animation of g.Animation.all) effectsFor(animation);
      studio2.preview?.reset();
      studio2.onRefresh?.();
    });
    disposables.push(g.Codecs.project.on("parsed", () => {
      if (isStudio()) {
        for (const animation of g.Animation.all) effectsFor(animation);
        repairTemplateUV();
        studio2.refresh();
      }
    }));
    listen("finished_edit", () => {
      studio2.preview?.invalidate();
      studio2.onRefresh?.();
    });
    for (const event of ["undo", "redo"]) listen(event, () => {
      if (isStudio()) {
        for (const animation of g.Animation.all) if (animation.animators.effects) effectsFor(animation);
        studio2.refresh();
      }
    });
    listen("update_keyframe_selection", () => studio2.onRefresh?.());
    listen("select_animation", () => {
      if (isStudio() && g.Animation.selected) effectsFor(g.Animation.selected);
      studio2.preview?.reset();
      studio2.onRefresh?.();
    });
    for (const project of g.ModelProject.all) if (project.format?.id === FORMAT) project.format = format;
    if (isStudio()) {
      format.select();
      for (const animation of g.Animation.all) effectsFor(animation);
      repairTemplateUV();
    }
    return studio2;
  }

  // src/effects.mjs
  var clamp = (x, a = 0, b = 1) => Math.max(a, Math.min(b, x));
  var mix = (a, b, t) => a + (b - a) * t;
  function random(seed) {
    let n = seed >>> 0;
    return () => {
      n += 1831565813;
      let t = n;
      t = Math.imul(t ^ t >>> 15, t | 1);
      t ^= t + Math.imul(t ^ t >>> 7, t | 61);
      return ((t ^ t >>> 14) >>> 0) / 4294967296;
    };
  }
  function color(value) {
    return [1, 3, 5].map((i) => parseInt(value.slice(i, i + 2), 16) / 255);
  }
  function curve(points, age, start, end, isColor = false) {
    const list = points?.length ? points.slice().sort((a2, b2) => a2.age - b2.age) : [{ age: 0, value: start }, { age: 1, value: end }];
    let a = list[0], b = list.at(-1);
    if (age <= a.age) b = a;
    else if (age >= b.age) a = b;
    else for (let i = 1; i < list.length; i++) if (age <= list[i].age) {
      a = list[i - 1];
      b = list[i];
      break;
    }
    const t = a.age === b.age ? 0 : clamp((age - a.age) / (b.age - a.age));
    return isColor ? color(a.value).map((v, i) => mix(v, color(b.value)[i], t)) : mix(a.value, b.value, t);
  }
  function unit(v) {
    const length = Math.hypot(...v);
    return length ? v.map((n) => n / length) : [0, 1, 0];
  }
  function cross(a, b) {
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  }
  function direction(axis, spread, r) {
    const n = unit(axis), u = unit(cross(Math.abs(n[1]) < 0.99 ? [0, 1, 0] : [1, 0, 0], n)), v = cross(n, u);
    const cos = mix(1, Math.cos(spread * Math.PI / 180), r()), sin = Math.sqrt(Math.max(0, 1 - cos * cos)), angle = r() * Math.PI * 2;
    return n.map((x, i) => x * cos + sin * (Math.cos(angle) * u[i] + Math.sin(angle) * v[i]));
  }
  function spawnPosition(effect, r) {
    if (effect.shape === "point") return [0, 0, 0];
    if (effect.shape === "box") return effect.shapeSize.map((n) => (r() - 0.5) * n);
    const v = direction([0, 1, 0], 180, r), radius = Math.cbrt(r());
    if (effect.shape === "sphere") return v.map((n, i) => n * radius * effect.shapeSize[i] / 2);
    const angle = r() * Math.PI * 2, height = Math.cbrt(r()), radial = Math.sqrt(r()) * height;
    return [Math.cos(angle) * radial * effect.shapeSize[0] / 2, height * effect.shapeSize[1], Math.sin(angle) * radial * effect.shapeSize[2] / 2];
  }
  function particles(interval, time) {
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
      const r = random(effect.seed + Math.imul(index + 1, 2654435761) >>> 0);
      const origin = spawnPosition(effect, r), heading = direction(effect.direction, effect.spread, r);
      const speed = Math.max(0, effect.speed + (r() * 2 - 1) * effect.speedSpread), velocity = heading.map((n) => n * speed);
      const d = effect.drag, velocityTime = d > 0 ? (1 - Math.exp(-d * age)) / d : age;
      const gravityTime = d > 0 ? (age - velocityTime) / d : age * age / 2;
      const position = origin.map((n, i) => n + velocity[i] * velocityTime + effect.gravity[i] * gravityTime);
      const progress = age / effect.lifetime;
      records.push({
        index,
        birth: start + birth,
        age,
        position,
        size: curve(effect.curves.size, progress, effect.size, effect.sizeEnd),
        opacity: curve(effect.curves.opacity, progress, effect.opacity, effect.opacityEnd),
        color: curve(effect.curves.color, progress, effect.color, effect.colorEnd, true),
        rotation: curve(effect.curves.rotation, progress, effect.rotation, effect.rotation + effect.angularVelocity * effect.lifetime) * Math.PI / 180,
        frame: Math.min(effect.frames - 1, Math.floor(age * effect.fps) % effect.frames)
      });
    }
    if (records.length > MAX_PARTICLES) throw new Error("Efeito excedeu o limite validado de part\xEDculas.");
    return records;
  }
  function trailTimes(interval, time) {
    if (time < interval.start || time > interval.end + interval.definition.trailLength) return [];
    const end = Math.min(time, interval.end), start = Math.max(interval.start, time - interval.definition.trailLength);
    if (start >= end) return [];
    const count = interval.definition.trailSamples;
    return Array.from({ length: count }, (_, i) => mix(start, end, i / (count - 1)));
  }
  function impulse(interval, time) {
    const age = (time - interval.start) / interval.definition.duration;
    if (age < 0 || time >= interval.end) return null;
    const e = interval.definition;
    return {
      size: curve(e.curves.size, age, e.size, e.sizeEnd),
      opacity: curve(e.curves.opacity, age, e.opacity, e.opacityEnd),
      color: curve(e.curves.color, age, e.color, e.colorEnd, true),
      intensity: e.intensity * (1 - age)
    };
  }

  // src/materials.mjs
  function faceTextures(g, node) {
    const faces = node instanceof g.Cube ? g.Canvas.face_order.map((key) => node.faces[key]).filter((face) => face.texture !== null) : Object.values(node.faces).filter((face) => face.vertices.length >= 3);
    const textures = faces.map((face) => face.getTexture());
    return node instanceof g.Mesh ? textures.filter((texture, index, all) => !index || texture !== all[index - 1]) : textures;
  }
  function createMaterialPreview(g, studio2) {
    const originals = /* @__PURE__ */ new Map();
    function restore() {
      for (const [material, old] of originals) {
        Object.assign(material, old);
        delete material.uniforms.STUDIO_OPAQUE;
        delete material.uniforms.STUDIO_CUTOFF;
        material.needsUpdate = true;
      }
      originals.clear();
    }
    function update() {
      if (!studio2.isStudio()) {
        restore();
        return;
      }
      for (const texture of g.Texture.all) {
        const material = texture.material, settings = studio2.state().materials[texture.uuid] || { alphaMode: "MASK", alphaCutoff: 0.5, doubleSided: true };
        if (!material?.isShaderMaterial) continue;
        if (!originals.has(material)) {
          originals.set(material, { fragmentShader: material.fragmentShader, side: material.side, transparent: material.transparent, blending: material.blending, depthWrite: material.depthWrite });
          const shader = material.fragmentShader.replace(/\bdiscard\s*;/g, ";"), end = shader.lastIndexOf("}");
          material.fragmentShader = "uniform bool STUDIO_OPAQUE;\nuniform float STUDIO_CUTOFF;\n" + shader.slice(0, end) + "\nif (!STUDIO_OPAQUE && gl_FragColor.a < STUDIO_CUTOFF) discard;\ngl_FragColor.a = 1.0;\n" + shader.slice(end);
          material.uniforms.STUDIO_OPAQUE = { value: false };
          material.uniforms.STUDIO_CUTOFF = { value: 0.5 };
          material.needsUpdate = true;
        }
        const side = settings.doubleSided ? g.THREE.DoubleSide : g.THREE.FrontSide;
        if (material.side !== side) {
          material.side = side;
          material.needsUpdate = true;
        }
        material.uniforms.STUDIO_OPAQUE.value = settings.alphaMode === "OPAQUE";
        material.uniforms.STUDIO_CUTOFF.value = settings.alphaCutoff;
        material.transparent = false;
        material.depthWrite = true;
        material.blending = g.THREE.NoBlending;
      }
    }
    g.Blockbench.on("render_frame", update);
    update();
    return { update, dispose() {
      g.Blockbench.removeListener("render_frame", update);
      restore();
    } };
  }

  // src/preview.mjs
  function createPreview(g, studio2) {
    const T = g.THREE, root = new T.Group();
    root.name = "ShardBound Studio preview";
    root.userData.no_export = true;
    const pool = /* @__PURE__ */ new Map(), textureCache = /* @__PURE__ */ new Map(), anchorCache = /* @__PURE__ */ new Map(), audio = /* @__PURE__ */ new Map();
    let rendering = false, lastTime = null, lastAnimation = null, epoch = 0, dirty = true, lastFrame = "";
    let solo = null, soloStart = 0;
    let draft = null;
    const definitions = () => studio2.state().effects.map((e) => draft?.id === e.id ? draft : e);
    const white = g.document.createElement("canvas");
    white.width = white.height = 32;
    const whiteCtx = white.getContext("2d"), radial = whiteCtx.createRadialGradient(16, 16, 0, 16, 16, 16);
    radial.addColorStop(0, "#ffffff");
    radial.addColorStop(0.35, "#ffffffff");
    radial.addColorStop(1, "#ffffff00");
    whiteCtx.fillStyle = radial;
    whiteCtx.fillRect(0, 0, 32, 32);
    const whiteTexture = new T.Texture(white);
    whiteTexture.needsUpdate = true;
    whiteTexture.magFilter = whiteTexture.minFilter = T.NearestFilter;
    whiteTexture.generateMipmaps = false;
    function cancelAudio(media) {
      media.studioCanceled = true;
      media.pause();
      media.src = "";
    }
    function stopAudio() {
      for (const media of audio.values()) cancelAudio(media);
      audio.clear();
    }
    function texture(effect, frame = 0) {
      const key = `${effect.texture || "radial"}:${effect.columns}:${effect.rows}:${frame}`;
      if (textureCache.has(key)) return textureCache.get(key);
      const source = g.Texture.all.find((t) => t.uuid === effect.texture)?.img;
      if (!source?.complete || !source.naturalWidth) return whiteTexture;
      const width = source.naturalWidth / effect.columns, height = source.naturalHeight / effect.rows;
      const canvas = g.document.createElement("canvas");
      canvas.width = Math.max(1, Math.round(width));
      canvas.height = Math.max(1, Math.round(height));
      const col = frame % effect.columns, row = Math.floor(frame / effect.columns);
      canvas.getContext("2d").drawImage(source, col * width, row * height, width, height, 0, 0, canvas.width, canvas.height);
      const result = new T.Texture(canvas);
      result.needsUpdate = true;
      result.magFilter = result.minFilter = T.NearestFilter;
      result.generateMipmaps = false;
      textureCache.set(key, result);
      return result;
    }
    function matrixAt(target, time, animation) {
      const key = `${animation?.uuid || "solo"}:${target || "world"}:${time.toFixed(6)}`;
      if (anchorCache.has(key)) return anchorCache.get(key).clone();
      const current = g.Timeline.time;
      try {
        if (animation && Math.abs(time - current) > 1e-8) {
          g.Timeline.time = time;
          g.Animator.preview(false);
        }
        g.Canvas.scene.updateMatrixWorld(true);
        const node = studio2.find(target), matrix = node?.mesh ? node.mesh.matrixWorld.clone() : new T.Matrix4();
        matrix.premultiply(g.Canvas.scene.matrixWorld.clone().invert());
        anchorCache.set(key, matrix);
        return matrix.clone();
      } finally {
        if (g.Timeline.time !== current) {
          g.Timeline.time = current;
          g.Animator.preview(false);
        }
      }
    }
    function clearObject(object) {
      root.remove(object);
      if (object.userData.studioOwnGeometry) object.geometry?.dispose();
      const materials = Array.isArray(object.material) ? object.material : [object.material];
      new Set(materials).forEach((m) => m?.dispose());
    }
    function sprite(key, effect) {
      let object = pool.get(key);
      if (!object) {
        const material = new T.SpriteMaterial({
          map: whiteTexture,
          transparent: true,
          depthWrite: false,
          blending: effect.blend === "additive" ? T.AdditiveBlending : T.NormalBlending
        });
        object = new T.Sprite(material);
        root.add(object);
        pool.set(key, object);
      }
      return object;
    }
    function sound(event, time, initial = false) {
      const key = event.instance || `${event.resource}|${event.target || ""}`;
      if (event.action === "stop") {
        for (const [id, media2] of audio) if (media2.studioInstance === key) {
          cancelAudio(media2);
          audio.delete(id);
        }
        return;
      }
      const resource = studio2.state().sounds.find((s) => s.id === event.resource);
      if (!resource) return;
      let age = time - event.time;
      if (age < 0) return;
      if (!event.loop && resource.duration && age * event.pitch >= resource.duration) {
        if (initial) return;
        age = 0;
      }
      const audioKey = event.action === "start" ? key : `${event.id}:${epoch}`;
      if (audio.has(audioKey)) {
        if (event.action !== "start") return;
        cancelAudio(audio.get(audioKey));
        audio.delete(audioKey);
      }
      const media = new g.Audio(resource.data);
      media.volume = Math.max(0, Math.min(1, event.volume * (g.settings.volume.value / 100)));
      try {
        media.playbackRate = event.pitch * (g.Timeline.playback_speed || 100) / 100;
      } catch {
        preview.lastError = "A combina\xE7\xE3o de pitch e velocidade da timeline excedeu a faixa de reprodu\xE7\xE3o do \xE1udio no Blockbench.";
        return;
      }
      media.studioPitch = event.pitch;
      media.studioInstance = key;
      media.preservesPitch = false;
      media.loop = !!event.loop;
      audio.set(audioKey, media);
      media.onloadedmetadata = () => {
        if (age > 0 && Number.isFinite(media.duration)) media.currentTime = event.loop ? age * event.pitch % media.duration : Math.min(age * event.pitch, media.duration);
      };
      media.onended = () => {
        if (audio.get(audioKey) === media) audio.delete(audioKey);
      };
      media.play().then(() => {
        if (!media.studioCanceled) preview.stats.audioStarted++;
      }).catch((error) => {
        if (!media.studioCanceled) preview.lastError = `\xC1udio: ${error.message}`;
        if (audio.get(audioKey) === media) audio.delete(audioKey);
      });
    }
    function ribbon(key, interval, time, animation) {
      const times = trailTimes(interval, time);
      if (times.length < 2) return null;
      const e = interval.definition, position = [], uv = [], opacity = [], indices = [];
      const age = clamp((time - interval.start) / e.duration), width = e.width * curve(e.curves.size, age, e.size, e.sizeEnd);
      const alpha = curve(e.curves.opacity, age, e.opacity, e.opacityEnd), tint = curve(e.curves.color, age, e.color, e.colorEnd, true);
      for (let i = 0; i < times.length; i++) {
        const matrix = matrixAt(interval.event.target, times[i], animation), offset = new T.Vector3(...interval.event.offset);
        const a = new T.Vector3(...e.startOffset).add(offset).applyMatrix4(matrix), b = new T.Vector3(...e.endOffset).add(offset);
        b.sub(new T.Vector3(...e.startOffset).add(offset)).multiplyScalar(width).add(new T.Vector3(...e.startOffset).add(offset)).applyMatrix4(matrix);
        position.push(...a.toArray(), ...b.toArray());
        uv.push(i / (times.length - 1), 0, i / (times.length - 1), 1);
        const fade = Math.max(0, 1 - (time - times[i]) / e.trailLength) * alpha;
        opacity.push(fade, fade);
        if (i) {
          const j = i * 2;
          indices.push(j - 2, j - 1, j, j - 1, j + 1, j);
        }
      }
      let object = pool.get(key);
      if (!object) {
        const material = new T.ShaderMaterial({
          transparent: true,
          depthWrite: false,
          side: T.DoubleSide,
          blending: e.blend === "additive" ? T.AdditiveBlending : T.NormalBlending,
          uniforms: { tint: { value: new T.Color(e.color) }, map: { value: texture(e) } },
          vertexShader: "attribute float opacity; varying float vAlpha; varying vec2 vUv; void main(){vAlpha=opacity;vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}",
          fragmentShader: "uniform vec3 tint; uniform sampler2D map; varying float vAlpha; varying vec2 vUv; void main(){vec4 c=texture2D(map,vUv);gl_FragColor=vec4(c.rgb*tint,c.a*vAlpha);}"
        });
        object = new T.Mesh(new T.BufferGeometry(), material);
        object.userData.studioOwnGeometry = true;
        root.add(object);
        pool.set(key, object);
      }
      object.geometry.setAttribute("position", new T.Float32BufferAttribute(position, 3));
      object.geometry.setAttribute("uv", new T.Float32BufferAttribute(uv, 2));
      object.geometry.setAttribute("opacity", new T.Float32BufferAttribute(opacity, 1));
      object.geometry.setIndex(indices);
      object.geometry.computeBoundingSphere();
      object.material.uniforms.tint.value.setRGB(...tint);
      object.material.uniforms.map.value = texture(e);
      return object;
    }
    function flashes(key, interval, time, seen, animation) {
      const visual = impulse(interval, time);
      if (!visual) return;
      const target = studio2.find(interval.event.target), candidates = [];
      function visit(node) {
        if (node instanceof g.Cube || node instanceof g.Mesh) candidates.push(node);
        for (const child of node?.children || []) visit(child);
      }
      visit(target instanceof g.Locator ? target.parent : target);
      for (const node of candidates) {
        let hidden = false;
        for (let p = node; p && p !== "root"; p = p.parent) if (p.visibility === false) hidden = true;
        if (hidden) continue;
        const id = `${key}:${node.uuid}`;
        let object = pool.get(id);
        if (!object) {
          const textures = faceTextures(g, node), cache = /* @__PURE__ */ new Map();
          const materials = textures.map((texture2) => {
            const uuid = texture2?.uuid || "";
            if (cache.has(uuid)) return cache.get(uuid);
            const settings = studio2.state().materials[uuid] || { alphaMode: "MASK", alphaCutoff: 0.5, doubleSided: true };
            const material = new T.ShaderMaterial({
              transparent: true,
              blending: T.AdditiveBlending,
              depthWrite: false,
              side: settings.doubleSided ? T.DoubleSide : T.FrontSide,
              polygonOffset: true,
              polygonOffsetFactor: -1,
              polygonOffsetUnits: -1,
              uniforms: {
                map: { value: texture2?.material?.map || whiteTexture },
                tint: { value: new T.Color() },
                opacity: { value: 0 },
                opaque: { value: settings.alphaMode === "OPAQUE" },
                cutoff: { value: settings.alphaCutoff }
              },
              vertexShader: "varying vec2 vUv;void main(){vUv=uv;gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);}",
              fragmentShader: "uniform sampler2D map;uniform vec3 tint;uniform float opacity;uniform float cutoff;uniform bool opaque;varying vec2 vUv;void main(){float a=texture2D(map,vUv).a;if(!opaque&&a<cutoff)discard;gl_FragColor=vec4(tint,opacity);}"
            });
            cache.set(uuid, material);
            return material;
          });
          if (!materials.length) continue;
          object = new T.Mesh(node.mesh.geometry, materials.every((m) => m === materials[0]) ? materials[0] : materials);
          object.matrixAutoUpdate = false;
          root.add(object);
          pool.set(id, object);
        }
        for (const material of new Set(Array.isArray(object.material) ? object.material : [object.material])) {
          material.uniforms.tint.value.setRGB(...visual.color);
          material.uniforms.opacity.value = visual.opacity;
        }
        const at = interval.event.follow || interval.definition.follow ? time : interval.start;
        object.matrix.copy(matrixAt(node.uuid, at, animation));
        const anchor = matrixAt(interval.event.target, at, animation), delta = new T.Vector3(...interval.event.offset).applyMatrix4(anchor).sub(new T.Vector3().setFromMatrixPosition(anchor));
        object.matrix.setPosition(new T.Vector3().setFromMatrixPosition(object.matrix).add(delta));
        seen.add(id);
      }
    }
    const preview = {
      root,
      enabled: true,
      suspended: false,
      stats: { particles: 0, trails: 0, flashes: 0, glows: 0, audioStarted: 0 },
      lastError: "",
      get activeSounds() {
        return audio.size;
      },
      get soloId() {
        return solo?.id || "";
      },
      setDraft(effect) {
        draft = effect;
        preview.invalidate();
      },
      clearDraft() {
        if (draft) {
          draft = null;
          preview.invalidate();
        }
      },
      invalidate() {
        dirty = true;
        preview.lastError = "";
        anchorCache.clear();
        for (const object of pool.values()) clearObject(object);
        pool.clear();
        for (const item of textureCache.values()) item.dispose();
        textureCache.clear();
      },
      reset() {
        lastTime = null;
        lastAnimation = null;
        lastFrame = "";
        solo = null;
        draft = null;
        stopAudio();
        for (const object of pool.values()) clearObject(object);
        pool.clear();
        anchorCache.clear();
        for (const map of textureCache.values()) map.dispose();
        textureCache.clear();
        dirty = true;
      },
      solo(id, target = "", event = {}) {
        const effect = studio2.state().effects.find((e) => e.id === id);
        if (!effect) throw new Error("Efeito n\xE3o encontrado.");
        if (["trail", "flash"].includes(effect.kind) && !target) throw new Error("Selecione uma pasta ou marcador para pr\xE9-visualizar o rastro/flash.");
        if (solo?.id !== id || solo?.target !== target) soloStart = g.performance.now() / 1e3;
        solo = { id, target, offset: event.offset?.slice() || [0, 0, 0], follow: event.follow ?? true };
        preview.invalidate();
      },
      stopSolo() {
        solo = null;
        preview.invalidate();
      },
      render(animation = g.Animation.selected, time = g.Timeline.time, { playing = false, seek = false } = {}) {
        if (rendering || preview.suspended) return;
        if (!studio2.isStudio() || !preview.enabled) {
          root.visible = false;
          return;
        }
        const events = solo ? [{
          id: "solo",
          type: definitions().find((e) => e.id === solo.id)?.kind,
          resource: solo.id,
          time: 0,
          action: "start",
          target: solo.target,
          offset: solo.offset,
          follow: solo.follow
        }] : studio2.events(animation);
        const muted = animation?.animators.effects?.muted[CHANNEL];
        if (muted && !solo) {
          root.visible = false;
          stopAudio();
          return;
        }
        root.visible = true;
        if (root.parent !== g.Canvas.scene) g.Canvas.scene.add(root);
        const signature = `${animation?.uuid}:${time}:${solo?.id || ""}`;
        if (lastFrame === signature && !dirty && !playing) return;
        rendering = true;
        try {
          if (playing && !solo) {
            const looped = lastAnimation === animation?.uuid && lastTime !== null && time < lastTime && !seek;
            if (looped || lastAnimation !== animation?.uuid) {
              stopAudio();
              epoch++;
            }
            const initial = lastTime === null || lastAnimation !== animation?.uuid;
            for (const event of crossedEvents(events, lastTime ?? 0, time, { length: animation?.length, looped, initial }))
              if (event.type === "sound") sound(event, time, initial);
            for (const media of audio.values()) {
              const rate = media.studioPitch * (g.Timeline.playback_speed || 100) / 100;
              if (media.playbackRate !== rate) try {
                media.playbackRate = rate;
              } catch {
                preview.lastError = "Velocidade de \xE1udio n\xE3o suportada pelo Blockbench.";
              }
            }
          }
          if (seek || !playing) {
            if (!g.Timeline.playing) stopAudio();
          }
          lastTime = time;
          lastAnimation = animation?.uuid;
          lastFrame = signature;
          dirty = false;
          const intervals = eventIntervals(events, definitions(), solo ? 3600 : animation?.length || 3600), seen = /* @__PURE__ */ new Set();
          preview.stats.particles = preview.stats.trails = preview.stats.flashes = preview.stats.glows = 0;
          for (const interval of intervals) {
            const e = interval.definition, key = `${interval.event.id}:${interval.start}`;
            if (e.kind === "particle") for (const p of particles(interval, time)) {
              const id = `${key}:${p.index}`, object = sprite(id, e);
              const anchor = matrixAt(interval.event.target, interval.event.follow || e.follow ? time : p.birth, solo ? null : animation);
              object.position.set(...p.position).add(new T.Vector3(...interval.event.offset)).applyMatrix4(anchor);
              object.scale.setScalar(p.size);
              object.material.opacity = p.opacity;
              object.material.color.setRGB(...p.color);
              object.material.rotation = p.rotation;
              object.material.map = texture(e, p.frame);
              seen.add(id);
              preview.stats.particles++;
            }
            else if (e.kind === "trail") {
              if (ribbon(key, interval, time, solo ? null : animation)) {
                seen.add(key);
                preview.stats.trails++;
              }
            } else if (e.kind === "flash") {
              const count = seen.size;
              flashes(key, interval, time, seen, solo ? null : animation);
              if (seen.size > count) preview.stats.flashes++;
            } else if (e.kind === "glow") {
              const visual = impulse(interval, time);
              if (!visual) continue;
              const object = sprite(key, { ...e, blend: "additive" }), anchor = matrixAt(interval.event.target, interval.event.follow || e.follow ? time : interval.start, solo ? null : animation);
              object.position.set(...interval.event.offset).applyMatrix4(anchor);
              object.scale.setScalar(visual.size);
              object.material.color.setRGB(...visual.color);
              object.material.opacity = visual.opacity;
              object.material.map = texture(e);
              seen.add(key);
              const lightKey = key + ":light";
              let light = pool.get(lightKey);
              if (!light) {
                light = new T.PointLight(16777215, 1, e.distance);
                root.add(light);
                pool.set(lightKey, light);
              }
              light.position.copy(object.position);
              light.color.setRGB(...visual.color);
              light.intensity = visual.intensity;
              light.distance = e.distance;
              seen.add(lightKey);
              preview.stats.glows++;
            }
          }
          for (const [key, object] of pool) if (!seen.has(key)) {
            clearObject(object);
            pool.delete(key);
          }
          while (anchorCache.size > 4096) anchorCache.delete(anchorCache.keys().next().value);
        } catch (error) {
          preview.lastError = error.message;
          throw error;
        } finally {
          rendering = false;
        }
      },
      dispose() {
        preview.reset();
        root.parent?.remove(root);
        for (const map of textureCache.values()) map.dispose();
        textureCache.clear();
        whiteTexture.dispose();
        for (const [event, fn] of handlers) g.Blockbench.removeListener(event, fn);
      }
    };
    const handlers = [
      ["display_animation_frame", ({ in_loop }) => {
        if (studio2.isStudio() && !solo) preview.render(g.Animation.selected, g.Animation.selected?.time ?? g.Timeline.time, { playing: !!in_loop && g.Timeline.playing, seek: !in_loop });
      }],
      ["timeline_play", () => {
        solo = null;
        lastTime = null;
        lastAnimation = null;
      }],
      ["timeline_pause", () => stopAudio()],
      ["render_frame", () => {
        if (!studio2.isStudio() || preview.suspended || rendering) return;
        if (solo) {
          const e = definitions().find((e2) => e2.id === solo.id);
          if (e) preview.render(null, (g.performance.now() / 1e3 - soloStart) % (e.duration + (e.kind === "particle" ? e.lifetime : 0.25)));
        } else if (dirty && g.Modes.animate) preview.render();
        else if (!g.Modes.animate) root.visible = false;
      }]
    ];
    for (const [event, fn] of handlers) g.Blockbench.on(event, fn);
    return preview;
  }

  // src/validation.mjs
  function validateProject(g, studio2) {
    const errors = [], warnings = [], error = (message) => errors.push(message);
    let state;
    try {
      state = checkState(studio2.state());
    } catch (e) {
      return { errors: [e.message], warnings, valid: false };
    }
    const groups = g.Group.all, elements = g.Outliner.elements;
    if (!state.models.length) error("O projeto ainda n\xE3o cont\xE9m modelos declarados. Adicione um modelo independente.");
    const byId = new Map([...groups, ...elements].map((n) => [n.uuid, n]));
    const uniqueIds = (items, label) => {
      const used = /* @__PURE__ */ new Set();
      for (const item of items) {
        if (!resourceId(item.id)) error(`${label}: ID inv\xE1lido ${item.id}.`);
        if (used.has(item.id)) error(`${label}: ID repetido ${item.id}.`);
        used.add(item.id);
      }
    };
    uniqueIds(state.models, "Modelo");
    uniqueIds(state.effects, "Efeito");
    uniqueIds(state.sounds, "Som");
    for (const sound of state.sounds) {
      if (!/^data:audio\/(wav|x-wav|ogg|mpeg);base64,[A-Za-z0-9+/=]+$/.test(sound.data) || !(sound.duration > 0 && Number.isFinite(sound.duration))) error(`${sound.id}: \xE1udio importado inv\xE1lido. Importe o arquivo novamente.`);
      if (state.effects.some((effect) => effect.id === sound.id)) error(`${sound.id}: efeito e som devem ter IDs distintos para sele\xE7\xE3o na timeline.`);
    }
    const roots = /* @__PURE__ */ new Set();
    if (state.context) {
      if (!(byId.get(state.context.root) instanceof g.Group)) error("Raiz do player de refer\xEAncia removida.");
      for (const part of BODY_PARTS) if (!(byId.get(state.context.groups[part]) instanceof g.Group)) error(`Player de refer\xEAncia: pasta ${part} removida.`);
    }
    for (const model of state.models) {
      if (!model.roots?.length) error(`${model.id}: modelo sem pasta raiz.`);
      for (const uuid of model.roots || []) {
        const root = byId.get(uuid);
        if (!(root instanceof g.Group)) {
          error(`${model.id}: pasta raiz removida.`);
          continue;
        }
        if (root.export === false) error(`${model.id}: pasta raiz desativada para exporta\xE7\xE3o.`);
        if (roots.has(uuid)) error(`${model.id}: pasta atribu\xEDda a mais de um modelo.`);
        roots.add(uuid);
      }
      for (const binding of model.bindings || []) {
        if (!model.roots.includes(binding.group) || !byId.has(binding.referenceGroup)) error(`${model.id}: v\xEDnculo de equipamento inv\xE1lido.`);
        if (byId.get(binding.group)?.parent?.uuid !== binding.referenceGroup) error(`${model.id}: a pasta do equipamento foi movida para fora do v\xEDnculo. Use Encaixar para atualizar.`);
      }
      if (model.type === "player") {
        const texture = g.Texture.all.find((t) => t.uuid === model.player?.texture);
        if (!texture || texture.width !== 64 || texture.height !== 64) error(`${model.id}: skin exige uma textura 64\xD764.`);
      }
    }
    for (const group of groups) {
      if (!group.name?.trim()) error("Existe uma pasta sem nome.");
      if (!group.origin.every(Number.isFinite) || !group.rotation.every(Number.isFinite)) error(`${group.name}: piv\xF4/rota\xE7\xE3o inv\xE1lidos.`);
      const visited = /* @__PURE__ */ new Set([group]);
      let parent = group.parent;
      while (parent instanceof g.Group) {
        if (visited.has(parent)) {
          error(`${group.name}: ciclo na hierarquia.`);
          break;
        }
        visited.add(parent);
        parent = parent.parent;
      }
      if (roots.has(group.uuid)) {
        for (const p of visited) if (p !== group && roots.has(p.uuid)) error(`${group.name}: modelos independentes n\xE3o podem ser aninhados um dentro do outro.`);
      }
    }
    const names = groups.map((g2) => g2.name);
    if (new Set(names).size !== names.length) warnings.push("H\xE1 nomes de pastas repetidos. O Studio usa UUIDs est\xE1veis para distinguir os n\xF3s.");
    const isReference = (node) => state.context?.geometry?.includes(node.uuid);
    const owned = (node) => {
      let p = node;
      const seen = /* @__PURE__ */ new Set();
      while (p && !seen.has(p)) {
        if (roots.has(p.uuid)) return true;
        seen.add(p);
        p = p.parent;
      }
      return false;
    };
    for (const element of elements) {
      if (isReference(element) || element instanceof g.Locator || element.export === false) continue;
      if (!owned(element)) {
        error(`${element.name}: geometria fora dos modelos declarados. Mova-a para uma pasta de modelo.`);
        continue;
      }
      if (!(element instanceof g.Cube || element instanceof g.Mesh)) {
        error(`${element.name}: tipo de geometria n\xE3o suportado pelo Studio.`);
        continue;
      }
      if (element instanceof g.Cube && element.from.some((v, i) => !Number.isFinite(v) || !Number.isFinite(element.to[i]) || v > element.to[i])) error(`${element.name}: dimens\xF5es de cubo inv\xE1lidas.`);
      if (element instanceof g.Mesh) {
        if (Object.values(element.vertices).some((v) => !v.every(Number.isFinite))) error(`${element.name}: v\xE9rtice inv\xE1lido.`);
        for (const face of Object.values(element.faces)) if (face.vertices.some((id) => !Object.hasOwn(element.vertices, id))) error(`${element.name}: face referencia v\xE9rtice removido.`);
      }
      for (const [direction2, face] of Object.entries(element.faces || {})) {
        if (face.texture === null || face.enabled === false) continue;
        const texture = face.getTexture();
        if (!texture) {
          error(`${element.name}/${direction2}: textura ausente. Crie/aplique uma textura.`);
          continue;
        }
        if (texture.render_mode !== "default") error(`${texture.name}: converta o material do modelo para o modo Default; efeitos aditivos/emissivos pertencem aos VFX Studio.`);
        if (!texture.img?.complete || !texture.img.naturalWidth) error(`${texture.name}: aguarde o carregamento da imagem ou recarregue a textura.`);
        const uvs = Array.isArray(face.uv) ? [face.uv.slice(0, 2), face.uv.slice(2, 4)] : Object.values(face.uv || {});
        if (uvs.some((v) => v.some((n) => !Number.isFinite(n)))) error(`${element.name}/${direction2}: UV inv\xE1lido.`);
        if (uvs.some((v) => v[0] < 0 || v[1] < 0 || v[0] > texture.getUVWidth() || v[1] > texture.getUVHeight())) warnings.push(`${element.name}/${direction2}: UV fora da imagem (wrap ${texture.wrap_mode}).`);
      }
    }
    const renderable = (element) => {
      for (let p = element; p && p !== "root"; p = p.parent) if (p.export === false) return false;
      return element instanceof g.Cube ? Object.values(element.faces).some((face) => face.texture !== null) : element instanceof g.Mesh && Object.values(element.faces).some((face) => face.vertices.length >= 3);
    };
    for (const model of state.models) if (model.type !== "effect" && !elements.some((e) => owned(e) && !isReference(e) && renderable(e) && model.roots.some((root) => {
      let p = e;
      while (p && p !== "root") {
        if (p.uuid === root) return true;
        p = p.parent;
      }
      return false;
    }))) error(`${model.id}: modelo sem geometria export\xE1vel. Modele e habilite suas faces antes de exportar.`);
    for (const effect of state.effects) {
      errors.push(...validateEffect(effect));
      if (effect.texture && !g.Texture.all.some((t) => t.uuid === effect.texture)) error(`${effect.id}: textura de sprite removida.`);
      const texture = g.Texture.all.find((t) => t.uuid === effect.texture);
      if (texture && (!texture.img.complete || !texture.img.naturalWidth || texture.width % effect.columns || texture.height % effect.rows)) error(`${effect.id}: sprite n\xE3o carregado ou dimens\xF5es incompat\xEDveis com a grade de frames.`);
    }
    for (const texture of g.Texture.all) {
      const material = state.materials[texture.uuid];
      if (material && !["OPAQUE", "MASK"].includes(material.alphaMode)) error(`${texture.name}: o perfil de modelo aceita OPAQUE ou MASK.`);
      if (material && (!Number.isFinite(material.alphaCutoff) || material.alphaCutoff < 0 || material.alphaCutoff > 1)) error(`${texture.name}: corte alpha inv\xE1lido.`);
    }
    const clips = /* @__PURE__ */ new Set();
    for (const animation of g.Animation.all) {
      if (!animation.name.trim() || clips.has(animation.name)) error(`Clip sem nome ou repetido: ${animation.name}.`);
      clips.add(animation.name);
      if (!(animation.length > 0 && animation.length <= 1e3 && Number.isFinite(animation.length))) error(`${animation.name}: dura\xE7\xE3o deve ficar entre 0 e 1000 s, limite da timeline nativa 5.2.2.`);
      if (!Number.isFinite(animation.snapping) || animation.snapping < 1 || animation.snapping > 120) error(`${animation.name}: a timeline nativa 5.2.2 suporta 1\u2013120 FPS.`);
      for (const property of ["anim_time_update", "start_delay", "loop_delay"]) if (animation[property] && animation[property] !== "0") error(`${animation.name}: ${property} altera o rel\xF3gio do clip e n\xE3o pertence ao contrato Studio. Use tempos e eventos expl\xEDcitos.`);
      if (animation.blend_weight && !Number.isFinite(Number(animation.blend_weight))) error(`${animation.name}: peso de blend din\xE2mico n\xE3o export\xE1vel. Use um valor num\xE9rico.`);
      if (animation.length * state.sampleRate * Math.max(groups.length, 1) > 2e6) error(`${animation.name}: exporta\xE7\xE3o excede 2 milh\xF5es de amostras de grupos. Reduza dura\xE7\xE3o/FPS.`);
      for (const [uuid, animator] of Object.entries(animation.animators)) {
        if (uuid === "effects") {
          for (const channel of ["particle", "sound", "timeline"]) {
            if (channel !== CHANNEL && animator[channel]?.length) error(`${animation.name}: use os eventos Studio em vez da trilha legada ${channel}.`);
          }
          continue;
        }
        if (!groups.some((group) => group.uuid === uuid)) error(`${animation.name}: canal de anima\xE7\xE3o referencia pasta removida.`);
        for (const kf of animator.keyframes) {
          if (!Number.isFinite(kf.time) || kf.time < 0 || kf.time > animation.length) error(`${animation.name}: keyframe de transforma\xE7\xE3o fora da dura\xE7\xE3o do clip.`);
          if (!["position", "rotation", "scale"].includes(kf.channel)) error(`${animation.name}: canal ${kf.channel} n\xE3o export\xE1vel.`);
          if (!["linear", "step", "bezier", "catmullrom"].includes(kf.interpolation)) error(`${animation.name}: interpola\xE7\xE3o ${kf.interpolation} desconhecida.`);
          for (const point of kf.data_points) if (["x", "y", "z"].some((axis) => point[axis] === "" || !Number.isFinite(Number(point[axis]))))
            error(`${animation.name}: express\xF5es din\xE2micas n\xE3o s\xE3o um contrato de exporta\xE7\xE3o. Use valores num\xE9ricos nos keyframes.`);
        }
      }
      for (const event of studio2.events(animation)) {
        try {
          studio2.validateEvent(event);
        } catch (e) {
          error(`${animation.name} / frame ${event.frame}: ${e.message}`);
        }
        if (!Number.isFinite(event.time) || event.time < 0 || event.time > animation.length) error(`${animation.name}: evento fora da dura\xE7\xE3o do clip.`);
      }
    }
    return { valid: !errors.length, errors: [...new Set(errors)], warnings: [...new Set(warnings)] };
  }

  // src/exporter.mjs
  function createExporter(g, studio2) {
    const T = g.THREE;
    function materialFor(texture, cache, state) {
      if (!texture) throw new Error("Textura ausente ao compilar geometria.");
      if (cache.has(texture.uuid)) return cache.get(texture.uuid);
      const settings = state.materials[texture.uuid] || { alphaMode: "MASK", alphaCutoff: 0.5, doubleSided: true };
      const canvas = g.document.createElement("canvas");
      canvas.width = texture.img.naturalWidth;
      canvas.height = texture.img.naturalHeight;
      canvas.getContext("2d").drawImage(texture.img, 0, 0);
      const map = new T.Texture(canvas);
      map.needsUpdate = true;
      map.magFilter = map.minFilter = T.NearestFilter;
      map.generateMipmaps = false;
      map.wrapS = map.wrapT = texture.wrap_mode === "repeat" ? T.RepeatWrapping : T.ClampToEdgeWrapping;
      const material = new T.MeshStandardMaterial({
        name: texture.name,
        map,
        color: 16777215,
        roughness: 1,
        metalness: 0,
        alphaTest: settings.alphaMode === "MASK" ? settings.alphaCutoff : 0,
        transparent: false,
        side: settings.doubleSided ? T.DoubleSide : T.FrontSide
      });
      cache.set(texture.uuid, material);
      return material;
    }
    function sceneFor(model, state) {
      const scene = new T.Scene(), unitRoot = new T.Group();
      unitRoot.name = "ShardBound_metres";
      unitRoot.scale.setScalar(1 / state.unitsPerMeter);
      scene.add(unitRoot);
      const clones = /* @__PURE__ */ new Map(), materials = /* @__PURE__ */ new Map(), roots = new Set(model.roots), referenceGeometry = new Set(state.context?.geometry || []);
      const belongs = (node) => {
        let p = node;
        while (p && p !== "root") {
          if (roots.has(p.uuid)) return true;
          p = p.parent;
        }
        return false;
      };
      function visit(node) {
        if (node instanceof g.Group) {
          if (node.export === false) return null;
          const children = node.children.map(visit).filter(Boolean);
          if (!belongs(node) && !children.length) return null;
          const object2 = new T.Group();
          object2.name = node.name;
          object2.uuid = node.mesh.uuid;
          object2.position.copy(node.mesh.position);
          object2.quaternion.copy(node.mesh.quaternion);
          object2.scale.copy(node.mesh.scale);
          object2.userData = { studio_uuid: node.uuid, studio_role: belongs(node) ? "model" : "reference_transform" };
          children.forEach((child) => object2.add(child));
          clones.set(node.uuid, object2);
          return object2;
        }
        if (!belongs(node) || node.export === false || referenceGeometry.has(node.uuid)) return null;
        if (node instanceof g.Locator) {
          const object2 = new T.Group();
          object2.name = node.name;
          object2.position.copy(node.mesh.position);
          object2.quaternion.copy(node.mesh.quaternion);
          object2.userData = { studio_uuid: node.uuid, studio_role: "locator" };
          return object2;
        }
        if (!(node instanceof g.Cube || node instanceof g.Mesh)) return null;
        const geometry = node.mesh.geometry.clone();
        const textures = faceTextures(g, node);
        if (!textures.length) {
          geometry.dispose();
          return null;
        }
        const materialsOut = textures.map((texture) => materialFor(texture, materials, state));
        const object = new T.Mesh(geometry, materialsOut.every((m) => m === materialsOut[0]) ? materialsOut[0] : materialsOut);
        object.name = node.name;
        object.uuid = node.mesh.uuid;
        object.position.copy(node.mesh.position);
        object.quaternion.copy(node.mesh.quaternion);
        object.scale.copy(node.mesh.scale);
        object.userData = { studio_uuid: node.uuid };
        return object;
      }
      for (const node of g.Outliner.root) {
        const object = visit(node);
        if (object) unitRoot.add(object);
      }
      scene.updateMatrixWorld(true);
      return { scene, clones, materials };
    }
    function samples(animation, groups, rate) {
      const times = /* @__PURE__ */ new Set([0, animation.length]);
      for (let i = 1; i < Math.ceil(animation.length * rate); i++) times.add(i / rate);
      for (const animator of Object.values(animation.animators)) for (const keyframe of animator.keyframes || []) {
        if (keyframe.time >= 0 && keyframe.time <= animation.length) times.add(keyframe.time);
      }
      for (const animator of Object.values(animation.animators)) for (const channel of ["position", "rotation", "scale"]) {
        const keys = (animator[channel] || []).slice().sort((a, b) => a.time - b.time);
        const boundary = (time) => {
          const precision = Math.max(1e-6, Math.abs(time) * 2 ** -21);
          for (const t of [time - precision, time + precision]) if (t > 0 && t < animation.length) times.add(t);
        };
        for (let i = 0; i < keys.length; i++) {
          if (i && keys[i - 1].interpolation === "step") boundary(keys[i].time - 1 / 1200);
          if (keys[i].data_points.length > 1) boundary(keys[i].time + 1 / 1200);
        }
      }
      const sorted = [...new Set([...times].map(Math.fround))].sort((a, b) => a - b);
      const tracks = new Map(groups.map((group) => [group.uuid, { position: [], quaternion: [], scale: [] }]));
      animation.select();
      g.Animation.all.forEach((a) => {
        a.playing = a === animation;
      });
      for (const time of sorted) {
        g.Timeline.time = time;
        g.Animator.preview(false);
        g.Project.model_3d?.updateMatrixWorld(true);
        for (const group of groups) {
          const values = tracks.get(group.uuid), mesh = group.mesh;
          mesh.position.toArray(values.position, values.position.length);
          mesh.quaternion.toArray(values.quaternion, values.quaternion.length);
          mesh.scale.toArray(values.scale, values.scale.length);
        }
      }
      return { times: sorted, tracks };
    }
    const exporter = {
      validate: () => validateProject(g, studio2),
      async compile() {
        if (studio2.exporting) throw new Error("J\xE1 existe uma exporta\xE7\xE3o em andamento.");
        const report = exporter.validate();
        if (!report.valid) throw new Error(report.errors.join("\n"));
        const state = clone(studio2.state()), files = /* @__PURE__ */ new Map(), packageModels = [];
        const previous = {
          project: g.Project,
          animation: g.Animation.selected,
          time: g.Timeline.time,
          playing: g.Timeline.playing,
          viewMode: g.Project.view_mode,
          flags: g.Animation.all.map((a) => [a, a.playing])
        };
        const scenes = [], baked = [], playerChannels = [];
        const clips = g.Animation.all.map((animation) => ({
          name: animation.name,
          length: animation.length,
          loop: animation.loop,
          fps: animation.snapping,
          events: studio2.events(animation).map(({ keyframe, clip, ...event }) => event)
        }));
        const locators = g.Locator.all.map((locator) => ({ uuid: locator.uuid, name: locator.name, parent: locator.parent?.uuid || null, position: locator.position.slice(), rotation: locator.rotation.slice() }));
        const definitions = clone(state.effects);
        for (const effect of definitions) if (effect.texture) {
          const texture = g.Texture.all.find((t) => t.uuid === effect.texture);
          if (!texture) throw new Error("Sprite n\xE3o encontrado.");
          const filename = assetPath(effect.id, "png", "effects");
          files.set(filename, Uint8Array.from(g.atob(texture.getBase64()), (c) => c.charCodeAt(0)));
          effect.texture = filename;
        }
        const originalAnimators = new Map(g.Animation.all.map((a) => [a, new Set(Object.keys(a.animators))]));
        studio2.exporting = true;
        if (studio2.preview) studio2.preview.suspended = true;
        try {
          g.Timeline.pause();
          try {
            g.Project.view_mode = "textured";
            g.Animator.showDefaultPose();
            g.Canvas.updateAll();
            for (const model of state.models) {
              const built = sceneFor(model, state);
              scenes.push(built);
              packageModels.push({
                id: model.id,
                type: model.type,
                file: assetPath(model.id, "glb", "models"),
                roots: model.roots,
                bindings: model.bindings,
                skin: model.player ? { variant: state.skinVariant, width: 64, height: 64, layers: true } : void 0
              });
            }
            const allGroups = g.Group.all.slice();
            for (const animation of g.Animation.all) {
              const hasTransforms = Object.values(animation.animators).some((a) => a.type !== "effect" && a.keyframes.length);
              const sampled = samples(animation, allGroups, state.sampleRate);
              baked.push({ name: animation.name, length: animation.length, hasTransforms, ...sampled });
              if (state.context) playerChannels.push({
                name: animation.name,
                length: animation.length,
                loop: animation.loop,
                fps: state.sampleRate,
                root: { times: sampled.times, ...sampled.tracks.get(state.context.root) },
                channels: Object.fromEntries(Object.entries(state.context.groups).map(([name, uuid]) => [name, {
                  times: sampled.times,
                  ...sampled.tracks.get(uuid)
                }]))
              });
            }
          } finally {
            if (g.Project === previous.project) {
              g.Project.view_mode = previous.viewMode;
              g.Canvas.updateAll();
              previous.animation?.select();
              previous.flags.forEach(([animation, playing]) => {
                animation.playing = playing;
              });
              g.Timeline.setTime(previous.time);
              if (g.Modes.animate) g.Animator.preview(false);
              else g.Animator.showDefaultPose();
              for (const [animation, keys] of originalAnimators) for (const key of Object.keys(animation.animators))
                if (!keys.has(key) && !animation.animators[key].keyframes.length) delete animation.animators[key];
            }
            if (studio2.preview) studio2.preview.suspended = false;
          }
          for (let i = 0; i < state.models.length; i++) {
            const built = scenes[i], animations = [];
            for (const clip of baked) {
              const tracks = [];
              for (const [uuid, object] of built.clones) {
                const values = clip.tracks.get(uuid);
                if (!values) continue;
                for (const channel of ["position", "quaternion", "scale"]) {
                  if (values[channel].some((v) => !Number.isFinite(v))) throw new Error(`${clip.name}: a anima\xE7\xE3o produziu valores inv\xE1lidos.`);
                  const Track = channel === "quaternion" ? T.QuaternionKeyframeTrack : T.VectorKeyframeTrack;
                  tracks.push(new Track(`${object.uuid}.${channel}`, clip.times, values[channel], T.InterpolateLinear));
                }
              }
              if (tracks.length && clip.hasTransforms) animations.push(new T.AnimationClip(clip.name, clip.length, tracks));
            }
            const bytes = await new Promise((resolve, reject) => {
              const timer = setTimeout(() => reject(new Error("O exportador GLB n\xE3o respondeu em 30 segundos.")), 3e4);
              try {
                new T.GLTFExporter().parse(
                  built.scene,
                  (result) => {
                    clearTimeout(timer);
                    resolve(result);
                  },
                  { binary: true, animations, trs: true, onlyVisible: false, embedImages: true, forcePowerOfTwoTextures: false, exportFaceColors: false }
                );
              } catch (error) {
                clearTimeout(timer);
                reject(error);
              }
            });
            if (!(bytes instanceof ArrayBuffer)) throw new Error("O exportador n\xE3o retornou GLB bin\xE1rio.");
            files.set(packageModels[i].file, new Uint8Array(bytes));
          }
          const sounds = state.sounds.map((sound) => {
            const [, mime, encoded] = /^data:(audio\/[^;]+);base64,(.*)$/.exec(sound.data);
            const extension = mime.includes("ogg") ? "ogg" : mime.includes("mpeg") ? "mp3" : "wav";
            const filename = assetPath(sound.id, extension, "audio");
            files.set(filename, Uint8Array.from(g.atob(encoded), (c) => c.charCodeAt(0)));
            return { id: sound.id, file: filename, duration: sound.duration };
          });
          const envelope = (schema) => ({ schema, version: SCHEMA_VERSION, generator: `ShardBound Studio ${VERSION}`, project: state.id });
          files.set("studio.effects.json", stableJson({ ...envelope("shardbound.studio.effects"), renderContract: state.renderContract, definitions }));
          files.set("studio.events.json", stableJson({ ...envelope("shardbound.studio.events"), clips, sounds, locators }));
          files.set("studio.player-animations.json", stableJson({ ...envelope("shardbound.studio.player_animations"), context: state.context ? {
            variant: state.skinVariant,
            geometryIncluded: false,
            root: state.context.root,
            groups: state.context.groups
          } : null, clips: playerChannels }));
          files.set("studio.manifest.json", stableJson({
            ...envelope("shardbound.studio.package"),
            editorVersion: g.Blockbench.version,
            unitsPerMeter: state.unitsPerMeter,
            models: packageModels,
            effects: "studio.effects.json",
            events: "studio.events.json",
            playerAnimations: "studio.player-animations.json",
            projectileVisual: state.projectileVisual,
            runtimeIntegration: "not-yet-implemented"
          }));
          return { files, report, state };
        } finally {
          if (studio2.preview) studio2.preview.suspended = false;
          for (const built of scenes) {
            built.scene.traverse((node) => {
              node.geometry?.dispose();
            });
            for (const material of built.materials.values()) {
              material.map?.dispose();
              material.dispose();
            }
          }
          studio2.exporting = false;
          if (previous.playing && g.Project === previous.project) g.Timeline.start();
        }
      },
      async write(directory) {
        const compiled = await exporter.compile();
        if (!g.isApp) throw new Error("Exporta\xE7\xE3o de pasta dispon\xEDvel no Blockbench Desktop.");
        const path = studio2.nativeRequire("path"), parent = path.resolve(directory);
        const nativeFS = studio2.nativeRequire("fs", { scope: parent, message: "Salvar o pacote ShardBound Studio somente na pasta de destino escolhida." });
        if (!nativeFS) throw new Error("O Blockbench n\xE3o concedeu acesso \xE0 pasta de exporta\xE7\xE3o. Permita o acesso ao exportar novamente.");
        const fs = nativeFS.promises, slug = compiled.state.id.replace(/[^a-z0-9_.-]/g, "_");
        let target;
        for (let suffix = 0; suffix < 1e4; suffix++) {
          target = path.join(parent, slug + (suffix ? `-${suffix}` : ""));
          try {
            await fs.mkdir(target);
            break;
          } catch (e) {
            if (e.code !== "EEXIST") throw e;
            if (suffix === 9999) throw new Error("N\xE3o foi poss\xEDvel reservar uma pasta nova.");
          }
        }
        try {
          for (const [relative, content] of compiled.files) {
            const destination = path.resolve(target, ...relative.split("/"));
            if (!destination.startsWith(target + path.sep)) throw new Error("Caminho de pacote inv\xE1lido.");
            await fs.mkdir(path.dirname(destination), { recursive: true });
            await fs.writeFile(destination, typeof content === "string" ? content : g.Buffer.from(content), { flag: "wx" });
          }
        } catch (error) {
          throw new Error(`A exporta\xE7\xE3o falhou. Arquivos parciais est\xE3o em ${target}. ${error.message}`);
        }
        return { directory: target, files: [...compiled.files.keys()], warnings: compiled.report.warnings };
      }
    };
    return exporter;
  }

  // src/effect-inspector.mjs
  function createEffectInspector(g, studio2, { paintSprite, addAudio, chooseResource }) {
    const host = g.document.createElement("div");
    g.Panels.keyframe.node.append(host);
    const field = (key, label, min = 0, max = 1e4, step = 0.1) => ({ key, label, type: "number", min, max, step });
    const choice = (key, label, options) => ({ key, label, type: "select", options });
    const vector = (key, label) => ({ key, label, type: "vector" });
    const color2 = (key, label) => ({ key, label, type: "color" });
    const groups = (e) => [
      { name: "Apar\xEAncia", open: true, fields: [
        color2("color", "Cor inicial"),
        color2("colorEnd", "Cor final"),
        field("opacity", "Opacidade inicial", 0, 1, 0.01),
        field("opacityEnd", "Opacidade final", 0, 1, 0.01),
        ...e.kind === "flash" ? [] : [
          field("size", "Tamanho inicial"),
          field("sizeEnd", "Tamanho final"),
          choice("blend", "Mistura", { alpha: "Transpar\xEAncia", additive: "Aditiva" })
        ]
      ] },
      { name: "Emiss\xE3o e dura\xE7\xE3o", open: true, fields: [
        field("duration", "Dura\xE7\xE3o (s)", 1e-3, 3600),
        ...e.kind === "particle" ? [
          field("burst", "Quantidade no disparo", 0, 4096, 1),
          field("rate", "Quantidade por segundo", 0, 4096, 1),
          field("lifetime", "Vida da part\xEDcula (s)", 1e-3, 120),
          choice("shape", "Forma do emissor", { point: "Ponto", box: "Caixa", sphere: "Esfera", cone: "Cone" }),
          vector("shapeSize", "Dimens\xF5es")
        ] : []
      ] },
      { name: "Movimento", visible: e.kind === "particle", fields: [
        vector("direction", "Dire\xE7\xE3o"),
        field("spread", "Abertura (\xB0)", 0, 180),
        field("speed", "Velocidade"),
        field("speedSpread", "Varia\xE7\xE3o"),
        vector("gravity", "Gravidade"),
        field("drag", "Arrasto", 0, 100),
        field("rotation", "Rota\xE7\xE3o (\xB0)", -36e3, 36e3),
        field("angularVelocity", "Rota\xE7\xE3o por segundo", -36e3, 36e3)
      ] },
      { name: "Rastro", visible: e.kind === "trail", fields: [
        field("trailLength", "Comprimento temporal (s)", 1e-3, 30),
        field("trailSamples", "Amostras", 2, 240, 1),
        field("width", "Largura", 1e-3),
        vector("startOffset", "In\xEDcio da faixa"),
        vector("endOffset", "Fim da faixa")
      ] },
      { name: "Ilumina\xE7\xE3o", visible: e.kind === "glow", fields: [field("intensity", "Intensidade"), field("distance", "Alcance")] },
      { name: "Sprite e atlas", visible: e.kind !== "flash", fields: e.kind === "particle" ? [
        field("columns", "Colunas", 1, 256, 1),
        field("rows", "Linhas", 1, 256, 1),
        field("frames", "Frames", 1, 65536, 1),
        field("fps", "FPS", 0, 240)
      ] : [] },
      { name: "Avan\xE7ado", fields: [
        { key: "id", label: "ID do recurso", type: "text" },
        choice("kind", "Aplica\xE7\xE3o visual", { particle: "Sprites emitidos", trail: "Faixa ao longo do movimento", flash: "Cor sobre a superf\xEDcie", glow: "Sprite com luz" }),
        ...e.kind === "particle" ? [field("seed", "Semente", 0, 4294967295, 1), { key: "follow", label: "Part\xEDculas acompanham a origem", type: "checkbox" }] : []
      ] }
    ].filter((section) => section.visible !== false);
    let syncing = false, signature = "", activeSelection = "", project = null, disposed = false;
    const vm = new g.Vue({
      data: () => ({
        visible: false,
        events: [],
        eventId: "",
        event: null,
        definition: null,
        textures: [],
        targets: [],
        error: "",
        loopPreview: false,
        curveChannel: "size",
        curvePoints: [],
        resources: []
      }),
      computed: {
        sections() {
          return this.definition ? groups(this.definition) : [];
        },
        curveOptions() {
          return this.definition?.kind === "flash" ? { opacity: "Opacidade", color: "Cor" } : { size: "Tamanho", opacity: "Opacidade", color: "Cor", ...this.definition?.kind === "particle" ? { rotation: "Rota\xE7\xE3o" } : {} };
        }
      },
      methods: {
        selectEvent() {
          signature = "";
          sync(true);
        },
        moveEvent() {
          try {
            studio2.updateEvent(this.eventId, {}, Number(this.event.time));
            this.error = "";
            signature = "";
            sync(true);
          } catch (error) {
            this.error = error.message;
            signature = "";
            sync(true);
          }
        },
        fieldInput() {
          const errors = validateEffect(this.definition);
          this.error = errors.join("\n");
          if (errors.length) return;
          studio2.preview.setDraft(clone(this.definition));
          if (this.loopPreview) this.startPreview();
          else studio2.preview.render();
        },
        fieldChange() {
          if (syncing || !this.definition) return;
          try {
            const id = studio2.events().find((e) => e.id === this.eventId)?.resource;
            if (!id) return;
            syncing = true;
            studio2.updateEffect(id, clone(this.definition));
            this.error = "";
            studio2.preview.clearDraft();
          } catch (error) {
            this.error = error.message;
            studio2.preview.clearDraft();
          } finally {
            syncing = false;
            signature = "";
            sync(true);
            if (this.loopPreview) this.startPreview();
          }
        },
        eventChange() {
          try {
            const e = this.event;
            studio2.updateEvent(this.eventId, {
              resource: e.resource,
              type: studio2.state().effects.find((d) => d.id === e.resource)?.kind || "sound",
              action: e.action,
              target: e.target,
              offset: e.offset.slice(),
              follow: !!e.follow,
              instance: e.instance,
              volume: e.volume,
              pitch: e.pitch,
              loop: !!e.loop
            });
            this.error = "";
            if (this.loopPreview && this.definition) this.startPreview();
          } catch (error) {
            this.error = error.message;
            signature = "";
            sync(true);
          }
        },
        startPreview() {
          try {
            studio2.preview.solo(this.definition.id, this.event.target, this.event);
            this.loopPreview = true;
          } catch (error) {
            this.error = error.message;
            this.loopPreview = false;
          }
        },
        togglePreview() {
          if (this.loopPreview) {
            this.loopPreview = false;
            studio2.preview.stopSolo();
            studio2.preview.clearDraft();
            g.Animator.preview(false);
          } else this.startPreview();
        },
        sprite() {
          paintSprite(this.definition.id);
        },
        audio() {
          addAudio(g.Animation.selected, g.Timeline.time, this.eventId);
        },
        add() {
          chooseResource(g.Animation.selected, g.Timeline.time);
        },
        loadCurve() {
          const e = this.definition, key = this.curveChannel;
          const start = key === "rotation" ? e.rotation : e[key], end = key === "rotation" ? e.rotation + e.angularVelocity * e.lifetime : e[key + "End"];
          this.curvePoints = clone(e.curves[key] || [{ age: 0, value: start }, { age: 1, value: end }]);
        },
        curveInput(commit = false) {
          g.Vue.set(this.definition.curves, this.curveChannel, clone(this.curvePoints));
          this.fieldInput();
          if (commit && !this.error) this.fieldChange();
        },
        addPoint() {
          if (this.curvePoints.length >= 64) return;
          const sorted = this.curvePoints.slice().sort((a, b) => a.age - b.age);
          let gap = 0, age = 0.5;
          for (let i = 1; i < sorted.length; i++) if (sorted[i].age - sorted[i - 1].age > gap) {
            gap = sorted[i].age - sorted[i - 1].age;
            age = sorted[i - 1].age + gap / 2;
          }
          this.curvePoints.push({ age, value: this.curvePoints[0].value });
          this.curveInput(true);
        },
        removePoint(index) {
          if (this.curvePoints.length > 2) {
            this.curvePoints.splice(index, 1);
            this.curveInput(true);
          }
        },
        curveLine() {
          const max = Math.max(1, ...this.curvePoints.map((p) => Number(p.value) || 0));
          return this.curvePoints.slice().sort((a, b) => a.age - b.age).map((p) => `${p.age * 240},${90 - (Number(p.value) || 0) / max * 80}`).join(" ");
        },
        dragCurve(index, event) {
          if (this.curveChannel === "color") return;
          const svg = event.currentTarget.ownerSVGElement, bounds = svg.getBoundingClientRect();
          const max = Math.max(1, ...this.curvePoints.map((p) => Number(p.value) || 0));
          const move = (ev) => {
            const point = this.curvePoints[index];
            point.age = Math.max(0, Math.min(1, (ev.clientX - bounds.left) / bounds.width));
            point.value = Math.max(0, (90 - (ev.clientY - bounds.top) / bounds.height * 100) / 80 * max);
            this.curveInput();
          };
          const up = () => {
            g.document.removeEventListener("pointermove", move);
            g.document.removeEventListener("pointerup", up);
            if (!disposed) this.curveInput(true);
            cleanupDrag = null;
          };
          cleanupDrag?.();
          cleanupDrag = () => {
            g.document.removeEventListener("pointermove", move);
            g.document.removeEventListener("pointerup", up);
          };
          g.document.addEventListener("pointermove", move);
          g.document.addEventListener("pointerup", up, { once: true });
          event.preventDefault();
        },
        curveY(point) {
          const max = Math.max(1, ...this.curvePoints.map((p) => Number(p.value) || 0));
          return 90 - (Number(point.value) || 0) / max * 80;
        }
      },
      template: `<div v-show="visible" class="sbstudio-keyframe-properties">
      <select v-if="events.length > 1" aria-label="Evento selecionado" v-model="eventId" @change="selectEvent"><option v-for="e in events" :key="e.id" :value="e.id">{{e.resource}} \xB7 {{e.action}}</option></select>
      <div v-if="event">
        <details><summary>Evento e origem</summary>
        <label>Tempo do evento (s) <input aria-label="Tempo do evento" type="number" min="0" step="0.01" v-model.number="event.time" @change="moveEvent"></label>
        <label>Recurso <select v-model="event.resource" @change="eventChange"><option v-for="r in resources" :value="r.id">{{r.id}}</option></select></label>
        <label>A\xE7\xE3o <select v-model="event.action" @change="eventChange"><option value="burst">Disparar uma vez</option><option value="start">Iniciar</option><option value="stop">Parar inst\xE2ncia</option></select></label>
        <label v-if="definition">Origem <select v-model="event.target" @change="eventChange"><option value="">Mundo / origem</option><option v-for="t in targets" :value="t.uuid">{{t.name}}</option></select></label>
        <div v-if="definition" class="sbstudio-field"><span>Deslocamento</span><div class="sbstudio-vector"><input v-for="(n,i) in event.offset" :aria-label="'Deslocamento '+i" type="number" step="0.1" v-model.number="event.offset[i]" @change="eventChange"></div></div>
        <label v-if="definition && definition.kind !== 'trail' && definition.kind !== 'flash'"><input type="checkbox" v-model="event.follow" @change="eventChange"> Acompanhar origem ap\xF3s emitir</label>
        <details><summary>Inst\xE2ncia para iniciar/parar</summary><input aria-label="Inst\xE2ncia" v-model="event.instance" @change="eventChange"></details>
        </details>
        <div v-if="event.type === 'sound'">
          <label>Volume <input aria-label="Volume" type="range" min="0" max="1" step="0.01" v-model.number="event.volume" @change="eventChange"></label>
          <label>Velocidade <input aria-label="Velocidade do som" type="number" min="0.1" max="4" step="0.1" v-model.number="event.pitch" @change="eventChange"></label>
          <label><input type="checkbox" v-model="event.loop" @change="eventChange"> Repetir at\xE9 parar</label><button @click="audio">Escolher arquivo de \xE1udio\u2026</button>
        </div>
        <div v-if="definition">
          <div class="sbstudio-preview-controls"><button @click="togglePreview" :title="loopPreview ? 'Voltar ao tempo da timeline' : 'Pr\xE9via em loop na viewport'"><i class="material-icons">{{loopPreview ? 'stop' : 'play_arrow'}}</i>{{loopPreview ? 'Tempo da timeline' : 'Pr\xE9via em loop'}}</button></div>
          <details v-for="section in sections" :key="section.name" :open="section.open"><summary>{{section.name}}</summary>
            <div v-for="f in section.fields" :key="f.key" class="sbstudio-field">
              <label :for="'sbstudio-field-'+f.key">{{f.label}}</label>
              <div v-if="f.type === 'vector'" class="sbstudio-vector"><input v-for="(n,i) in definition[f.key]" :aria-label="f.label+' '+i" type="number" step="0.1" v-model.number="definition[f.key][i]" @input="fieldInput" @change="fieldChange"></div>
              <select v-else-if="f.type === 'select'" :id="'sbstudio-field-'+f.key" v-model="definition[f.key]" @change="fieldChange"><option v-for="(name,id) in f.options" :value="id">{{name}}</option></select>
              <input v-else-if="f.type === 'checkbox'" :id="'sbstudio-field-'+f.key" type="checkbox" v-model="definition[f.key]" @change="fieldChange">
              <input v-else-if="f.type === 'color'" :id="'sbstudio-field-'+f.key" type="color" v-model="definition[f.key]" @input="fieldInput" @change="fieldChange">
              <input v-else-if="f.type === 'text'" :id="'sbstudio-field-'+f.key" v-model="definition[f.key]" @change="fieldChange">
              <input v-else :id="'sbstudio-field-'+f.key" type="number" :min="f.min" :max="f.max" :step="f.step" v-model.number="definition[f.key]" @input="fieldInput" @change="fieldChange">
            </div>
            <template v-if="section.name === 'Sprite e atlas'"><label>Textura <select aria-label="Textura do efeito" v-model="definition.texture" @change="fieldChange"><option value="">Disco suave</option><option v-for="t in textures" :value="t.uuid">{{t.name}}</option></select></label><button @click="sprite">Pintar sprite</button></template>
          </details>
          <details class="sbstudio-curves"><summary>Curvas ao longo da vida</summary><select aria-label="Propriedade da curva" v-model="curveChannel" @change="loadCurve"><option v-for="(name,id) in curveOptions" :value="id">{{name}}</option></select>
            <svg v-if="curveChannel !== 'color'" viewBox="0 0 240 100"><polyline :points="curveLine()"/><circle v-for="(p,i) in curvePoints" :cx="p.age*240" :cy="curveY(p)" r="5" @pointerdown="dragCurve(i,$event)"/></svg>
            <div v-for="(p,i) in curvePoints" class="sbstudio-curve-point"><input aria-label="Idade normalizada" type="number" min="0" max="1" step="0.01" v-model.number="p.age" @input="curveInput(false)" @change="curveInput(true)"><input v-if="curveChannel==='color'" type="color" v-model="p.value" @input="curveInput(false)" @change="curveInput(true)"><input v-else aria-label="Valor da curva" type="number" step="0.01" v-model.number="p.value" @input="curveInput(false)" @change="curveInput(true)"><button :disabled="curvePoints.length<=2" @click="removePoint(i)">\xD7</button></div>
            <button :disabled="curvePoints.length>=64" @click="addPoint">Adicionar ponto</button><small>0 = nascimento; 1 = fim da vida. Arraste os pontos para ajustar.</small>
          </details>
        </div>
        <p v-if="error" class="sbstudio-error">{{error}}</p>
      </div>
    </div>`
    }).$mount(host);
    let cleanupDrag = null;
    function sync(force = false) {
      if (syncing || disposed) return;
      const selected = studio2.isStudio() && g.Modes.animate ? g.Timeline.selected.filter((k) => k.channel === CHANNEL) : [];
      const events = selected.length ? studio2.events().filter((e) => selected.some((k) => k.uuid === e.keyframe)) : [];
      const next = `${g.Project?.uuid}:${events.map((e) => e.id).join(",")}`;
      if (activeSelection !== next || project !== g.Project) {
        studio2.preview.clearDraft();
        if (studio2.preview.soloId) studio2.preview.stopSolo();
        vm.loopPreview = false;
        vm.error = "";
      }
      activeSelection = next;
      vm.visible = !!events.length;
      if (!events.length) {
        signature = next;
        return;
      }
      vm.events = events;
      if (!events.some((e) => e.id === vm.eventId)) vm.eventId = events[0].id;
      const current = events.find((e) => e.id === vm.eventId), definition = studio2.state().effects.find((e) => e.id === current.resource);
      const dataSignature = next + ":" + vm.eventId + ":" + JSON.stringify([current, definition]);
      if (!force && signature === dataSignature) return;
      project = g.Project;
      signature = dataSignature;
      vm.event = clone(current);
      vm.definition = definition ? clone(definition) : null;
      vm.textures = g.Texture.all.map((t) => ({ uuid: t.uuid, name: t.name }));
      vm.targets = [...g.Group.all, ...g.Locator.all].map((t) => ({ uuid: t.uuid, name: t.name }));
      vm.resources = [...studio2.state().effects, ...studio2.state().sounds].map((r) => ({ id: r.id }));
      if (vm.definition) {
        if (!vm.curveOptions[vm.curveChannel]) vm.curveChannel = Object.keys(vm.curveOptions)[0];
        vm.loadCurve();
      }
    }
    const onPlay = () => {
      vm.loopPreview = false;
      studio2.preview.clearDraft();
    };
    const onMode = () => sync(true);
    g.Blockbench.on("timeline_play", onPlay);
    g.Blockbench.on("select_mode", onMode);
    const css = g.Blockbench.addCSS(`#panel_keyframe:has(>.sbstudio-keyframe-properties:not([style*="display: none"])){max-height:65vh;min-height:0;overflow-y:auto}.sbstudio-keyframe-properties{padding:8px 6px;max-height:48vh;min-height:0;overflow-y:auto;flex-shrink:1}.sbstudio-keyframe-properties input{max-width:100%;min-width:0}.sbstudio-keyframe-properties input[type=number]{width:100%}.sbstudio-keyframe-properties select{max-width:100%;width:100%}.sbstudio-keyframe-properties label{display:block;margin:5px 0}.sbstudio-keyframe-properties details{border-top:1px solid var(--color-border);padding:6px 0}.sbstudio-keyframe-properties summary{cursor:pointer;font-weight:600;margin:3px 0 8px}.sbstudio-field{display:grid;grid-template-columns:minmax(80px,1fr) minmax(80px,1fr);align-items:center;gap:6px;margin:5px 0}.sbstudio-field input{min-width:0;width:100%}.sbstudio-field input[type=checkbox]{width:auto}.sbstudio-vector,.sbstudio-curve-point{display:flex;gap:3px}.sbstudio-vector input,.sbstudio-curve-point input{min-width:0;width:100%}.sbstudio-curves svg{width:100%;height:100px;background:var(--color-back);overflow:visible}.sbstudio-curves polyline{fill:none;stroke:var(--color-accent);stroke-width:2}.sbstudio-curves circle{fill:var(--color-accent);cursor:move}.sbstudio-curves small{display:block;margin-top:6px;color:var(--color-subtle_text)}.sbstudio-error{color:var(--color-error);white-space:pre-line}.sbstudio-preview-controls button{display:flex;align-items:center;gap:5px;margin:8px 0}`);
    return { vm, refresh: sync, dispose() {
      disposed = true;
      cleanupDrag?.();
      studio2.preview.clearDraft();
      studio2.preview.stopSolo();
      g.Blockbench.removeListener("timeline_play", onPlay);
      g.Blockbench.removeListener("select_mode", onMode);
      vm.$destroy();
      vm.$el.remove();
      css.delete();
    } };
  }

  // src/ui.mjs
  function createUI(g, studio2, exporter) {
    const actions = [], dialogs = /* @__PURE__ */ new Set(), attachments = [], restores = [], toolbarItems = [];
    let busy = false, disposed = false, inspector;
    const error = (e) => {
      g.Blockbench.showMessageBox({ title: "ShardBound Studio", icon: "error", message: String(e.message || e) });
      return false;
    };
    const run = (fn) => {
      try {
        const r = fn();
        return r?.catch ? r.catch(error) : r;
      } catch (e) {
        return error(e);
      }
    };
    const input = (label, value) => ({ type: "input", label, value });
    const number = (label, value, min, max, step = 0.1) => ({ type: "number", label, value, min, max, step });
    const select = (label, value, options) => ({ type: "select", label, value, options });
    function dialog(title, form, confirm) {
      const d = new g.Dialog({
        id: "sbstudio_" + g.guid(),
        title,
        width: 520,
        form,
        onConfirm: (r) => run(() => confirm(r)),
        onButton() {
          g.setTimeout(() => {
            dialogs.delete(d);
            d.delete();
          }, 0);
        }
      });
      dialogs.add(d);
      d.show();
      return d;
    }
    function model() {
      if (!studio2.isStudio()) return null;
      let node = g.Group.first_selected || g.Outliner.selected[0];
      while (node && node !== "root") {
        const m = studio2.state().models.find((m2) => m2.roots.includes(node.uuid));
        if (m) return m;
        node = node.parent;
      }
      return studio2.state().models[0];
    }
    const hasPlayer = () => studio2.isStudio() && studio2.state().models.some((m) => m.player);
    const canBind = () => studio2.isStudio() && ["item", "melee", "ranged", "armor"].includes(model()?.type);
    const inAnimation = () => studio2.isStudio() && !!g.Modes.animate && !!g.Animation.selected;
    function uniqueId(prefix) {
      const s = studio2.state();
      let n = 1, id = prefix;
      while ([...s.models, ...s.effects, ...s.sounds].some((r) => r.id === id)) id = prefix + "_" + n++;
      return id;
    }
    function newProject() {
      dialog("Novo projeto ShardBound Studio", {
        type: select("Conte\xFAdo", "monster", TYPES),
        name: input("Nome", "Meu modelo"),
        variant: { ...select("Bra\xE7os do player", "wide", { wide: "Padr\xE3o (4 px)", slim: "Fino (3 px)" }), condition: (r) => ["player", "melee", "ranged", "armor"].includes(r.type) },
        id: input("ID do recurso", "custom:model")
      }, (r) => {
        studio2.createProject(r.type, r);
        studio2.selectModel(studio2.state().models[0]);
      });
    }
    function addModel() {
      dialog("Modelo independente", {
        type: select("Conte\xFAdo", "item", TYPES),
        name: input("Nome", "Modelo relacionado"),
        id: input("ID do recurso", uniqueId(studio2.state().id + "_modelo"))
      }, (r) => studio2.selectModel(studio2.addModel(r.type, r)));
    }
    function modelProperties() {
      const m = model();
      if (m) dialog("Propriedades do modelo", { name: input("Nome", m.name), id: input("ID do recurso", m.id) }, (r) => studio2.renameModel(m, r.id, r.name));
    }
    function bind() {
      const m = model();
      if (m) dialog("Encaixe do equipamento", {
        root: select("Pasta", m.roots[0], Object.fromEntries(m.roots.map((id) => [id, studio2.find(id)?.name || id]))),
        part: select("Parte do player", "RightArm", Object.fromEntries(BODY_PARTS.map((p) => [p, p])))
      }, (r) => studio2.bind(m, r.part, r.root));
    }
    function material(texture = g.Texture.selected) {
      if (!texture) return;
      const old = studio2.state().materials[texture.uuid] || { alphaMode: "MASK", alphaCutoff: 0.5, doubleSided: true };
      dialog(
        "Material \u2014 " + texture.name,
        {
          alphaMode: select("Transpar\xEAncia", old.alphaMode, { OPAQUE: "Opaco", MASK: "Recorte alpha" }),
          alphaCutoff: number("Corte alpha", old.alphaCutoff, 0, 1, 0.01),
          doubleSided: { type: "checkbox", label: "Renderizar os dois lados", value: old.doubleSided }
        },
        (r) => studio2.edit("Studio: material", () => {
          studio2.state().materials[texture.uuid] = r;
        })
      );
    }
    function importSkin() {
      const project = g.Project;
      g.Blockbench.import({ resource_id: "sbstudio_skin", type: "Skin PNG 64\xD764", extensions: ["png"], readtype: "binary" }, (files) => run(async () => {
        if (!files[0] || disposed) return;
        if (g.Project !== project) throw new Error("O projeto mudou.");
        await studio2.importSkin("data:image/png;base64," + g.Buffer.from(files[0].content).toString("base64"), () => {
          if (disposed) throw new Error("O plugin foi desativado durante a importa\xE7\xE3o.");
        });
      }));
    }
    const origin = () => g.Locator.selected[0]?.uuid || g.Group.first_selected?.uuid || model()?.roots[0] || "";
    function addResource(animation, time, resource, newDefinition = null) {
      const definition = newDefinition || studio2.state().effects.find((e) => e.id === resource);
      const added = studio2.addEvent(animation, time, { type: definition?.kind || "sound", resource, target: definition ? origin() : "", action: "burst" }, newDefinition);
      g.Timeline.setTime(time);
      added.keyframe.select();
      inspector.vm.eventId = `${added.keyframe.uuid}:${added.event.id}`;
      inspector.refresh(true);
      return added;
    }
    function newEffect(animation = g.Animation.selected, time = g.Timeline.time) {
      if (!animation || time < 0 || time > animation.length) throw new Error("Posicione o cursor dentro de uma anima\xE7\xE3o.");
      const id = uniqueId(studio2.state().id.split(":")[0] + ":particula");
      const r = addResource(animation, time, id, defaultEffect("particle", id));
      inspector.vm.startPreview();
      return r;
    }
    async function audioFiles(files, animation, time, replaceId = null) {
      const project = g.Project;
      for (const file of files) {
        if (disposed || g.Project !== project) throw new Error("O projeto mudou durante a escolha do \xE1udio.");
        const ext = file.name.split(".").at(-1).toLowerCase(), mime = { wav: "audio/wav", ogg: "audio/ogg", mp3: "audio/mpeg" }[ext];
        if (!mime) throw new Error("Escolha WAV, OGG ou MP3.");
        const base = file.name.toLowerCase().replace(/\.[^.]+$/, "").replace(/[^a-z0-9_/-]+/g, "_") || "som";
        const id = uniqueId(studio2.state().id.split(":")[0] + ":" + base), data = "data:" + mime + ";base64," + g.Buffer.from(file.content).toString("base64");
        await studio2.addSound(
          id,
          data,
          file.name,
          () => {
            if (disposed || g.Project !== project || !g.Animation.all.includes(animation)) throw new Error("A anima\xE7\xE3o foi fechada durante a importa\xE7\xE3o.");
          },
          { animation, time: Math.min(time, animation.length), eventId: replaceId }
        );
        const e = studio2.events(animation).findLast((e2) => e2.resource === id);
        if (e) {
          inspector.vm.eventId = e.id;
          inspector.refresh(true);
        }
        replaceId = null;
      }
    }
    function addAudio(animation = g.Animation.selected, time = g.Timeline.time, replaceId = null) {
      if (!animation) throw new Error("Selecione uma anima\xE7\xE3o.");
      const project = g.Project;
      g.Blockbench.import(
        { resource_id: "sbstudio_sound", type: "Som neste frame", extensions: ["wav", "ogg", "mp3"], readtype: "binary", multiple: true },
        (files) => run(() => {
          if (disposed || g.Project !== project) throw new Error("O projeto mudou.");
          return audioFiles(files, animation, time, replaceId);
        })
      );
    }
    function chooseResource(animation = g.Animation.selected, time = g.Timeline.time, event) {
      new g.Menu("sbstudio_frame_resources", [
        { name: "Nova part\xEDcula", icon: "auto_awesome", click: () => run(() => newEffect(animation, time)) },
        { name: "Escolher \xE1udio\u2026", icon: "audio_file", click: () => run(() => addAudio(animation, time)) },
        "_",
        ...[...studio2.state().effects, ...studio2.state().sounds].map((r) => ({ name: r.id, icon: r.kind ? "auto_awesome" : "audio_file", click: () => run(() => addResource(animation, time, r.id)) }))
      ]).open(event || g.Toolbars.timeline.node);
    }
    function paintSprite(id) {
      const e = studio2.state().effects.find((e2) => e2.id === id);
      if (!e) return;
      let texture = g.Texture.all.find((t) => t.uuid === e.texture);
      if (!texture) {
        const textures = [];
        studio2.edit("Studio: sprite para pintar", () => {
          const canvas = g.document.createElement("canvas");
          canvas.width = canvas.height = 32;
          texture = new g.Texture({ name: id.split(":")[1], width: 32, height: 32, uv_width: 32, uv_height: 32 });
          texture.canvas.width = texture.canvas.height = 32;
          texture.fromDataURL(canvas.toDataURL()).add();
          textures.push(texture);
          e.texture = texture.uuid;
        }, { textures, bitmap: true });
      }
      texture.select();
      studio2.preview.stopSolo();
      g.Modes.options.paint.select();
    }
    function validate() {
      const report = exporter.validate();
      validation.update();
      g.Validator.validate();
      g.Validator.openDialog();
      return report;
    }
    async function exportFolder() {
      if (busy) return;
      if (!exporter.validate().valid) return validate();
      const directory = g.Blockbench.pickDirectory({ resource_id: "sbstudio_export", title: "Exportar pacote ShardBound" });
      if (!directory) return;
      busy = true;
      try {
        const r = await exporter.write(directory);
        g.Blockbench.showQuickMessage(`Pacote exportado: ${r.directory}`, 6e3);
      } finally {
        busy = false;
      }
    }
    const handlers = {
      newProject,
      addModel,
      modelProperties,
      bind,
      material,
      skin: importSkin,
      newEffect,
      audio: addAudio,
      event: chooseResource,
      sprite: () => paintSprite(inspector.vm.definition?.id),
      validate,
      exportFolder,
      save: () => studio2.codec.export()
    };
    function action(id, name, icon, click, condition = studio2.isStudio) {
      const a = new g.Action("sbstudio_" + id, { name, icon, condition, category: "animation", click: (event) => run(() => click(event)) });
      actions.push(a);
      return a;
    }
    function attach(menu, entry, path) {
      menu.addAction(entry, path);
      attachments.push([menu, (path ? path + "." : "") + (typeof entry === "string" ? entry : entry.id)]);
    }
    function wrap(id, fn) {
      const a = g.BarItems[id], original = a.click;
      const replacement = function(...args) {
        return studio2.isStudio() ? run(() => fn(original.bind(this), ...args)) : original.apply(this, args);
      };
      a.click = replacement;
      restores.push(() => {
        if (a.click === replacement) a.click = original;
      });
    }
    attach(g.MenuBar.menus.file, action("exportFolder", "Pacote ShardBound (.glb + recursos)", "folder_zip", exportFolder, () => studio2.isStudio() && !busy), "export");
    wrap("save_project", () => {
      if (!g.Project.save_path) return studio2.codec.export();
      const p = g.Project;
      g.Blockbench.writeFile(p.save_path, { content: studio2.codec.compile() }, () => {
        p.saved = true;
      });
    });
    wrap("save_project_as", () => studio2.codec.export());
    wrap("export_over", () => g.BarItems.save_project.click());
    wrap("add_animation", () => {
      let name = "idle", n = 1;
      while (g.Animation.all.some((a) => a.name === name)) name = "animation_" + n++;
      studio2.createClip(name, 1, "loop", 24).propertiesDialog();
    });
    const originalAnimationProperties = g.Animation.prototype.propertiesDialog;
    const animationProperties = function(...args) {
      const result = originalAnimationProperties.apply(this, args);
      if (!studio2.isStudio()) return result;
      const animation = this, d = g.Dialog.open, values = d.getFormResult();
      d.content_vue?.$el?.setAttribute("hidden", "");
      d.form_config.override.condition = false;
      d.form_config.snapping.min = 1;
      d.form_config.snapping.max = 120;
      d.form_config.studio_length = number("Dura\xE7\xE3o (segundos)", animation.length, 0.01, 1e3);
      d.form.form_config = d.form_config;
      d.form.buildForm();
      d.form.setValues(values, false);
      d.form.updateValues({ cause: "setup" });
      const confirm = d.onConfirm;
      d.onConfirm = function(r, event) {
        return run(() => {
          if (!(r.studio_length > 0 && r.studio_length <= 1e3) || !(r.snapping >= 1 && r.snapping <= 120)) throw new Error("Use dura\xE7\xE3o at\xE9 1000 s e 1\u2013120 FPS.");
          if (Object.values(animation.animators).some((a) => a.keyframes.some((k) => k.time > r.studio_length))) throw new Error("A dura\xE7\xE3o deve incluir todos os keyframes.");
          if (!r.name.trim() || g.Animation.all.some((a) => a !== animation && a.name === r.name)) throw new Error("Nome de anima\xE7\xE3o vazio ou repetido.");
          const result2 = confirm.call(this, r, event);
          if (result2 === false) return false;
          if (animation.length !== r.studio_length) studio2.edit("Studio: dura\xE7\xE3o da anima\xE7\xE3o", () => animation.setLength(r.studio_length), { animations: [animation] });
        });
      };
      return result;
    };
    g.Animation.prototype.propertiesDialog = animationProperties;
    restores.push(() => {
      if (g.Animation.prototype.propertiesDialog === animationProperties) g.Animation.prototype.propertiesDialog = originalAnimationProperties;
    });
    wrap("save_project_incremental", () => {
      const p = g.Project, path = p.save_path.replace(/\.(sbstudio|bbmodel)$/i, "_" + Date.now() + ".sbstudio");
      if (path === p.save_path) throw new Error("Salve o projeto Studio antes de criar uma vers\xE3o incremental.");
      g.Blockbench.writeFile(path, { content: studio2.codec.compile() }, () => {
        p.save_path = path;
        p.saved = true;
      });
    });
    wrap("project_window", (original) => {
      original();
      const d = g.Dialog.open, values = d.getFormResult(), s = studio2.state(), project = g.Project;
      Object.assign(d.form_config, {
        studio_id: input("ID ShardBound", s.id),
        studio_units: number("Unidades por metro", s.unitsPerMeter, 1e-3, 1e5),
        studio_samples: number("Amostragem da exporta\xE7\xE3o (FPS)", s.sampleRate, 1, 240, 1),
        studio_projectile: { ...input("Proj\xE9til visual compartilhado", s.projectileVisual || ""), condition: () => s.models.some((m) => m.type === "ranged") }
      });
      d.form.form_config = d.form_config;
      d.form.buildForm();
      d.form.setValues(values, false);
      d.form.updateValues({ cause: "setup" });
      const confirm = d.onConfirm;
      d.onConfirm = function(r, event) {
        return run(() => {
          if (g.Project !== project) throw new Error("O projeto mudou.");
          assertId(r.studio_id);
          if (r.studio_projectile) assertId(r.studio_projectile);
          if (!(r.studio_units > 0) || !(r.studio_samples >= 1 && r.studio_samples <= 240)) throw new Error("Escala ou FPS inv\xE1lido.");
          const result = confirm.call(this, r, event);
          if (result === false) return false;
          studio2.edit("Studio: propriedades", () => Object.assign(studio2.state(), { id: r.studio_id, unitsPerMeter: r.studio_units, sampleRate: r.studio_samples, projectileVisual: r.studio_projectile || null }));
        });
      };
    });
    const particle = action("newEffect", "Part\xEDcula neste frame", "auto_awesome", () => newEffect(), inAnimation);
    const audio = action("audio", "Som neste frame\u2026", "audio_file", () => addAudio(), inAnimation);
    const resources = action("resources", "Recurso neste frame", "add", (event) => chooseResource(g.Animation.selected, g.Timeline.time, event), inAnimation);
    for (const a of [particle, audio, resources]) {
      g.Toolbars.timeline.add(a, 6);
      toolbarItems.push([g.Toolbars.timeline, a]);
      attach(g.MenuBar.menus.animation, a);
      attach(g.Animation.prototype.menu, a);
      attach(g.Timeline.menu, a);
    }
    const modelAction = action("addModel", "Modelo independente", "library_add", addModel);
    attach(g.Panels.outliner.menu, modelAction);
    attach(g.BarItems.add_element.side_menu, modelAction);
    attach(g.Group.prototype.menu, { id: "sbstudio_model_properties", name: "Propriedades do modelo", icon: "settings", condition: studio2.isStudio, click: () => run(modelProperties) });
    attach(g.Group.prototype.menu, { id: "sbstudio_bind", name: "Encaixe do equipamento", icon: "pan_tool", condition: canBind, click: () => run(bind) });
    attach(g.Texture.prototype.menu, { id: "sbstudio_texture_material", name: "Material ShardBound", icon: "texture", condition: studio2.isStudio, click: (t) => run(() => material(t)) });
    const skin = action("skin", "Substituir skin 64\xD764\u2026", "image", importSkin, hasPlayer);
    attach(g.Texture.prototype.menu, skin);
    attach(g.MenuBar.menus.file, skin, "import");
    const layer = action("layer", "Mostrar / ocultar camada externa", "layers", () => {
      const elements = studio2.state().models.flatMap((m) => m.player?.layers || []).map(studio2.find).filter(Boolean);
      studio2.setSkinLayers(!elements.every((e) => e.visibility));
    }, hasPlayer);
    attach(g.Group.prototype.menu, layer);
    const validation = new g.ValidatorCheck("sbstudio_contract", {
      condition: studio2.isStudio,
      update_triggers: ["finished_edit", "select_project", "select_animation"],
      run() {
        const r = exporter.validate();
        r.errors.forEach((message) => this.fail({ message }));
        r.warnings.forEach((message) => this.warn({ message }));
      }
    });
    const drop = g.Blockbench.addDragHandler(
      "sbstudio_audio",
      { extensions: ["wav", "ogg", "mp3"], readtype: "binary", propagate: true, element: "#panel_timeline", condition: inAnimation },
      (files) => run(() => audioFiles(files, g.Animation.selected, g.Timeline.time))
    );
    inspector = createEffectInspector(g, studio2, { paintSprite, addAudio, chooseResource });
    const refresh = () => inspector.refresh();
    studio2.onRefresh = refresh;
    studio2.onRequestEvent = (animation, time) => run(() => chooseResource(animation, time));
    attach(g.MenuBar.menus.animation, { id: "sbstudio_library", name: "Recursos de efeitos e sons", icon: "library_books", condition: inAnimation, children: () => [...studio2.state().effects, ...studio2.state().sounds].map((r) => ({ id: "sbstudio_resource_" + r.id, name: r.id, icon: r.kind ? "auto_awesome" : "audio_file", children: [
      { name: "Inserir neste frame", icon: "add", click: () => run(() => addResource(g.Animation.selected, g.Timeline.time, r.id)) },
      { name: "Remover recurso", icon: "delete", click: () => run(() => r.kind ? studio2.removeEffect(r.id) : studio2.removeSound(r.id)) }
    ] })) });
    const boneLabel = g.Panels.bone?.handle?.querySelector("span"), originalLabel = boneLabel?.textContent;
    const vocabulary = () => {
      if (boneLabel) boneLabel.textContent = studio2.isStudio() ? "Pasta / piv\xF4" : originalLabel;
    };
    const mode = () => {
      if (!g.Modes.animate) {
        studio2.preview.clearDraft();
        studio2.preview.stopSolo();
      }
    };
    let shownPreviewError = "";
    const previewError = () => {
      const message = studio2.preview.lastError;
      if (message && message !== shownPreviewError) g.Blockbench.showQuickMessage(message, 6e3);
      shownPreviewError = message;
    };
    g.Blockbench.on("select_project", vocabulary);
    g.Blockbench.on("select_mode", mode);
    g.Blockbench.on("render_frame", previewError);
    vocabulary();
    refresh();
    return { handlers, inspector, audioFiles, refresh, dispose() {
      disposed = true;
      studio2.onRefresh = studio2.onRequestEvent = null;
      inspector.dispose();
      drop.delete();
      validation.delete();
      g.Blockbench.removeListener("select_project", vocabulary);
      g.Blockbench.removeListener("select_mode", mode);
      if (boneLabel) boneLabel.textContent = originalLabel;
      g.Blockbench.removeListener("render_frame", previewError);
      for (const d of dialogs) {
        if (g.Dialog.stack.includes(d)) d.hide();
        d.delete();
      }
      dialogs.clear();
      restores.reverse().forEach((fn) => fn());
      attachments.forEach(([menu, id]) => menu.removeAction(id));
      toolbarItems.forEach(([bar, a]) => bar.remove(a));
      actions.forEach((a) => a.delete());
    } };
  }

  // src/main.mjs
  var studio;
  var ui;
  BBPlugin.register("shardbound_studio", {
    title: "ShardBound Studio",
    author: "ShardBound",
    description: "Autoria de modelos, skins, anima\xE7\xF5es, part\xEDculas, VFX e sons sincronizados para ShardBound.",
    version: VERSION,
    min_version: "5.2.2",
    variant: "desktop",
    tags: ["Animation", "Model", "Texture"],
    onload() {
      studio = createStudio(window);
      studio.preview = createPreview(window, studio);
      studio.exporter = createExporter(window, studio);
      studio.nativeRequire = requireNativeModule;
      studio.materialPreview = createMaterialPreview(window, studio);
      ui = createUI(window, studio, studio.exporter);
      studio.ui = ui;
      window.ShardBoundStudio = studio;
    },
    onunload() {
      ui?.dispose();
      studio?.materialPreview?.dispose();
      studio?.dispose();
      delete window.ShardBoundStudio;
      ui = studio = null;
    }
  });
})();
