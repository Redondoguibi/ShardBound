import { VERSION, SCHEMA_VERSION, stableJson, clone, assetPath } from './core.mjs';
import { validateProject } from './validation.mjs';
import { faceTextures } from './materials.mjs';

export function createExporter(g, studio) {
  const T = g.THREE;
  function materialFor(texture, cache, state) {
    if (!texture) throw new Error('Textura ausente ao compilar geometria.');
    if (cache.has(texture.uuid)) return cache.get(texture.uuid);
    const settings = state.materials[texture.uuid] || { alphaMode: 'MASK', alphaCutoff: 0.5, doubleSided: true };
    const canvas = g.document.createElement('canvas'); canvas.width = texture.img.naturalWidth; canvas.height = texture.img.naturalHeight;
    canvas.getContext('2d').drawImage(texture.img, 0, 0);
    const map = new T.Texture(canvas); map.needsUpdate = true; map.magFilter = map.minFilter = T.NearestFilter; map.generateMipmaps = false;
    map.wrapS = map.wrapT = texture.wrap_mode === 'repeat' ? T.RepeatWrapping : T.ClampToEdgeWrapping;
    const material = new T.MeshStandardMaterial({ name: texture.name, map, color: 0xffffff, roughness: 1, metalness: 0,
      alphaTest: settings.alphaMode === 'MASK' ? settings.alphaCutoff : 0, transparent: false,
      side: settings.doubleSided ? T.DoubleSide : T.FrontSide });
    cache.set(texture.uuid, material); return material;
  }
  function sceneFor(model, state) {
    const scene = new T.Scene(), unitRoot = new T.Group(); unitRoot.name = 'ShardBound_metres'; unitRoot.scale.setScalar(1 / state.unitsPerMeter); scene.add(unitRoot);
    const clones = new Map(), materials = new Map(), roots = new Set(model.roots), referenceGeometry = new Set(state.context?.geometry || []);
    const belongs = node => { let p = node; while (p && p !== 'root') { if (roots.has(p.uuid)) return true; p = p.parent; } return false; };
    function visit(node) {
      if (node instanceof g.Group) {
        if (node.export === false) return null;
        const children = node.children.map(visit).filter(Boolean);
        if (!belongs(node) && !children.length) return null;
        const object = new T.Group(); object.name = node.name; object.uuid = node.mesh.uuid;
        object.position.copy(node.mesh.position); object.quaternion.copy(node.mesh.quaternion); object.scale.copy(node.mesh.scale);
        object.userData = { studio_uuid: node.uuid, studio_role: belongs(node) ? 'model' : 'reference_transform' };
        children.forEach(child => object.add(child)); clones.set(node.uuid, object); return object;
      }
      if (!belongs(node) || node.export === false || referenceGeometry.has(node.uuid)) return null;
      if (node instanceof g.Locator) {
        const object = new T.Group(); object.name = node.name; object.position.copy(node.mesh.position); object.quaternion.copy(node.mesh.quaternion);
        object.userData = { studio_uuid: node.uuid, studio_role: 'locator' }; return object;
      }
      if (!(node instanceof g.Cube || node instanceof g.Mesh)) return null;
      const geometry = node.mesh.geometry.clone();
      // Match 5.2.2 buffer material indices independently of the viewport's solid/UV/wireframe mode.
      const textures = faceTextures(g, node);
      if (!textures.length) { geometry.dispose(); return null; }
      const materialsOut = textures.map(texture => materialFor(texture, materials, state));
      const object = new T.Mesh(geometry, materialsOut.every(m => m === materialsOut[0]) ? materialsOut[0] : materialsOut); object.name = node.name; object.uuid = node.mesh.uuid;
      object.position.copy(node.mesh.position); object.quaternion.copy(node.mesh.quaternion); object.scale.copy(node.mesh.scale);
      object.userData = { studio_uuid: node.uuid }; return object;
    }
    for (const node of g.Outliner.root) { const object = visit(node); if (object) unitRoot.add(object); }
    scene.updateMatrixWorld(true); return { scene, clones, materials };
  }
  function samples(animation, groups, rate) {
    const times = new Set([0, animation.length]);
    for (let i = 1; i < Math.ceil(animation.length * rate); i++) times.add(i / rate);
    for (const animator of Object.values(animation.animators)) for (const keyframe of animator.keyframes || []) {
      if (keyframe.time >= 0 && keyframe.time <= animation.length) times.add(keyframe.time);
    }
    for (const animator of Object.values(animation.animators)) for (const channel of ['position', 'rotation', 'scale']) {
      const keys = (animator[channel] || []).slice().sort((a, b) => a.time - b.time);
      const boundary = time => {
        const precision = Math.max(1e-6, Math.abs(time) * 2 ** -21);
        for (const t of [time - precision, time + precision]) if (t > 0 && t < animation.length) times.add(t);
      };
      for (let i = 0; i < keys.length; i++) {
        // Native 5.2.2 snaps to a key within 1/1200s. Preserve both sides of its step boundary.
        if (i && keys[i - 1].interpolation === 'step') boundary(keys[i].time - 1 / 1200);
        if (keys[i].data_points.length > 1) boundary(keys[i].time + 1 / 1200);
      }
    }
    const sorted = [...new Set([...times].map(Math.fround))].sort((a, b) => a - b);
    const tracks = new Map(groups.map(group => [group.uuid, { position: [], quaternion: [], scale: [] }]));
    animation.select(); g.Animation.all.forEach(a => { a.playing = a === animation; });
    for (const time of sorted) {
      g.Timeline.time = time; g.Animator.preview(false); g.Project.model_3d?.updateMatrixWorld(true);
      for (const group of groups) {
        const values = tracks.get(group.uuid), mesh = group.mesh;
        mesh.position.toArray(values.position, values.position.length); mesh.quaternion.toArray(values.quaternion, values.quaternion.length); mesh.scale.toArray(values.scale, values.scale.length);
      }
    }
    return { times: sorted, tracks };
  }
  const exporter = {
    validate: () => validateProject(g, studio),
    async compile() {
      if (studio.exporting) throw new Error('Já existe uma exportação em andamento.');
      const report = exporter.validate(); if (!report.valid) throw new Error(report.errors.join('\n'));
      const state = clone(studio.state()), files = new Map(), packageModels = [];
      const previous = { project: g.Project, animation: g.Animation.selected, time: g.Timeline.time,
        playing: g.Timeline.playing, viewMode: g.Project.view_mode, flags: g.Animation.all.map(a => [a, a.playing]) };
      const scenes = [], baked = [], playerChannels = [];
      const clips = g.Animation.all.map(animation => ({ name: animation.name, length: animation.length,
        loop: animation.loop, fps: animation.snapping, events: studio.events(animation).map(({ keyframe, clip, ...event }) => event) }));
      const locators = g.Locator.all.map(locator => ({ uuid: locator.uuid, name: locator.name, parent: locator.parent?.uuid || null, position: locator.position.slice(), rotation: locator.rotation.slice() }));
      const definitions = clone(state.effects);
      for (const effect of definitions) if (effect.texture) {
        const texture = g.Texture.all.find(t => t.uuid === effect.texture); if (!texture) throw new Error('Sprite não encontrado.');
        const filename = assetPath(effect.id, 'png', 'effects');
        files.set(filename, Uint8Array.from(g.atob(texture.getBase64()), c => c.charCodeAt(0))); effect.texture = filename;
      }
      const originalAnimators = new Map(g.Animation.all.map(a => [a, new Set(Object.keys(a.animators))]));
      studio.exporting = true; if (studio.preview) studio.preview.suspended = true;
      try {
      g.Timeline.pause();
      try {
        g.Project.view_mode = 'textured'; g.Animator.showDefaultPose(); g.Canvas.updateAll();
        for (const model of state.models) {
          const built = sceneFor(model, state); scenes.push(built);
          packageModels.push({ id: model.id, type: model.type, file: assetPath(model.id, 'glb', 'models'), roots: model.roots,
            bindings: model.bindings, skin: model.player ? { variant: state.skinVariant, width: 64, height: 64, layers: true } : undefined });
        }
        const allGroups = g.Group.all.slice();
        for (const animation of g.Animation.all) {
          const hasTransforms = Object.values(animation.animators).some(a => a.type !== 'effect' && a.keyframes.length);
          const sampled = samples(animation, allGroups, state.sampleRate); baked.push({ name: animation.name, length: animation.length, hasTransforms, ...sampled });
          if (state.context) playerChannels.push({ name: animation.name, length: animation.length, loop: animation.loop, fps: state.sampleRate,
            root: { times: sampled.times, ...sampled.tracks.get(state.context.root) },
            channels: Object.fromEntries(Object.entries(state.context.groups).map(([name, uuid]) => [name, { times: sampled.times,
              ...sampled.tracks.get(uuid) }])) });
        }
      } finally {
        if (g.Project === previous.project) {
          g.Project.view_mode = previous.viewMode; g.Canvas.updateAll();
          previous.animation?.select(); previous.flags.forEach(([animation, playing]) => { animation.playing = playing; });
          g.Timeline.setTime(previous.time); if (g.Modes.animate) g.Animator.preview(false); else g.Animator.showDefaultPose();
          for (const [animation, keys] of originalAnimators) for (const key of Object.keys(animation.animators))
            if (!keys.has(key) && !animation.animators[key].keyframes.length) delete animation.animators[key];
        }
        if (studio.preview) studio.preview.suspended = false;
      }
        for (let i = 0; i < state.models.length; i++) {
          const built = scenes[i], animations = [];
          for (const clip of baked) {
            const tracks = [];
            for (const [uuid, object] of built.clones) {
              const values = clip.tracks.get(uuid); if (!values) continue;
              for (const channel of ['position', 'quaternion', 'scale']) {
                if (values[channel].some(v => !Number.isFinite(v))) throw new Error(`${clip.name}: a animação produziu valores inválidos.`);
                const Track = channel === 'quaternion' ? T.QuaternionKeyframeTrack : T.VectorKeyframeTrack;
                tracks.push(new Track(`${object.uuid}.${channel}`, clip.times, values[channel], T.InterpolateLinear));
              }
            }
            if (tracks.length && clip.hasTransforms) animations.push(new T.AnimationClip(clip.name, clip.length, tracks));
          }
          const bytes = await new Promise((resolve, reject) => {
            const timer = setTimeout(() => reject(new Error('O exportador GLB não respondeu em 30 segundos.')), 30000);
            try { new T.GLTFExporter().parse(built.scene, result => { clearTimeout(timer); resolve(result); },
              { binary: true, animations, trs: true, onlyVisible: false, embedImages: true, forcePowerOfTwoTextures: false, exportFaceColors: false }); }
            catch (error) { clearTimeout(timer); reject(error); }
          });
          if (!(bytes instanceof ArrayBuffer)) throw new Error('O exportador não retornou GLB binário.');
          files.set(packageModels[i].file, new Uint8Array(bytes));
        }
        const sounds = state.sounds.map(sound => {
          const [, mime, encoded] = /^data:(audio\/[^;]+);base64,(.*)$/.exec(sound.data);
          const extension = mime.includes('ogg') ? 'ogg' : mime.includes('mpeg') ? 'mp3' : 'wav';
          const filename = assetPath(sound.id, extension, 'audio'); files.set(filename, Uint8Array.from(g.atob(encoded), c => c.charCodeAt(0)));
          return { id: sound.id, file: filename, duration: sound.duration };
        });
        const envelope = schema => ({ schema, version: SCHEMA_VERSION, generator: `ShardBound Studio ${VERSION}`, project: state.id });
        files.set('studio.effects.json', stableJson({ ...envelope('shardbound.studio.effects'), renderContract: state.renderContract, definitions }));
        files.set('studio.events.json', stableJson({ ...envelope('shardbound.studio.events'), clips, sounds, locators }));
        files.set('studio.player-animations.json', stableJson({ ...envelope('shardbound.studio.player_animations'), context: state.context ? { variant: state.skinVariant, geometryIncluded: false,
          root: state.context.root, groups: state.context.groups } : null, clips: playerChannels }));
        files.set('studio.manifest.json', stableJson({ ...envelope('shardbound.studio.package'), editorVersion: g.Blockbench.version,
          unitsPerMeter: state.unitsPerMeter, models: packageModels, effects: 'studio.effects.json', events: 'studio.events.json',
          playerAnimations: 'studio.player-animations.json', projectileVisual: state.projectileVisual, runtimeIntegration: 'not-yet-implemented' }));
        return { files, report, state };
      } finally {
        if (studio.preview) studio.preview.suspended = false;
        for (const built of scenes) { built.scene.traverse(node => { node.geometry?.dispose(); }); for (const material of built.materials.values()) { material.map?.dispose(); material.dispose(); } }
        studio.exporting = false;
        if (previous.playing && g.Project === previous.project) g.Timeline.start();
      }
    },
    async write(directory) {
      const compiled = await exporter.compile();
      if (!g.isApp) throw new Error('Exportação de pasta disponível no Blockbench Desktop.');
      const path = studio.nativeRequire('path'), parent = path.resolve(directory);
      const nativeFS = studio.nativeRequire('fs', { scope: parent, message: 'Salvar o pacote ShardBound Studio somente na pasta de destino escolhida.' });
      if (!nativeFS) throw new Error('O Blockbench não concedeu acesso à pasta de exportação. Permita o acesso ao exportar novamente.');
      const fs = nativeFS.promises, slug = compiled.state.id.replace(/[^a-z0-9_.-]/g, '_');
      let target;
      for (let suffix = 0; suffix < 10000; suffix++) {
        target = path.join(parent, slug + (suffix ? `-${suffix}` : ''));
        try { await fs.mkdir(target); break; } catch (e) { if (e.code !== 'EEXIST') throw e; if (suffix === 9999) throw new Error('Não foi possível reservar uma pasta nova.'); }
      }
      try {
        for (const [relative, content] of compiled.files) {
          const destination = path.resolve(target, ...relative.split('/'));
          if (!destination.startsWith(target + path.sep)) throw new Error('Caminho de pacote inválido.');
          await fs.mkdir(path.dirname(destination), { recursive: true });
          await fs.writeFile(destination, typeof content === 'string' ? content : g.Buffer.from(content), { flag: 'wx' });
        }
      } catch (error) { throw new Error(`A exportação falhou. Arquivos parciais estão em ${target}. ${error.message}`); }
      return { directory: target, files: [...compiled.files.keys()], warnings: compiled.report.warnings };
    },
  };
  return exporter;
}
