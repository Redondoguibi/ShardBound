import { CHANNEL, BODY_PARTS, checkState, resourceId, validateEffect } from './core.mjs';
export function validateProject(g, studio) {
  const errors = [], warnings = [], error = message => errors.push(message);
  let state; try { state = checkState(studio.state()); } catch (e) { return { errors: [e.message], warnings, valid: false }; }
  const groups = g.Group.all, elements = g.Outliner.elements;
  if (!state.models.length) error('O projeto ainda não contém modelos declarados. Adicione um modelo independente.');
  const byId = new Map([...groups, ...elements].map(n => [n.uuid, n]));
  const uniqueIds = (items, label) => {
    const used = new Set(); for (const item of items) {
      if (!resourceId(item.id)) error(`${label}: ID inválido ${item.id}.`);
      if (used.has(item.id)) error(`${label}: ID repetido ${item.id}.`); used.add(item.id);
    }
  };
  uniqueIds(state.models, 'Modelo'); uniqueIds(state.effects, 'Efeito'); uniqueIds(state.sounds, 'Som');
  for (const sound of state.sounds) {
    if (!/^data:audio\/(wav|x-wav|ogg|mpeg);base64,[A-Za-z0-9+/=]+$/.test(sound.data) || !(sound.duration > 0 && Number.isFinite(sound.duration))) error(`${sound.id}: áudio importado inválido. Importe o arquivo novamente.`);
    if (state.effects.some(effect => effect.id === sound.id)) error(`${sound.id}: efeito e som devem ter IDs distintos para seleção na timeline.`);
  }
  const roots = new Set();
  if (state.context) {
    if (!(byId.get(state.context.root) instanceof g.Group)) error('Raiz do player de referência removida.');
    for (const part of BODY_PARTS) if (!(byId.get(state.context.groups[part]) instanceof g.Group)) error(`Player de referência: pasta ${part} removida.`);
  }
  for (const model of state.models) {
    if (!model.roots?.length) error(`${model.id}: modelo sem pasta raiz.`);
    for (const uuid of model.roots || []) {
      const root = byId.get(uuid); if (!(root instanceof g.Group)) { error(`${model.id}: pasta raiz removida.`); continue; }
      if (root.export === false) error(`${model.id}: pasta raiz desativada para exportação.`);
      if (roots.has(uuid)) error(`${model.id}: pasta atribuída a mais de um modelo.`); roots.add(uuid);
    }
    for (const binding of model.bindings || []) {
      if (!model.roots.includes(binding.group) || !byId.has(binding.referenceGroup)) error(`${model.id}: vínculo de equipamento inválido.`);
      if (byId.get(binding.group)?.parent?.uuid !== binding.referenceGroup) error(`${model.id}: a pasta do equipamento foi movida para fora do vínculo. Use Encaixar para atualizar.`);
    }
    if (model.type === 'player') {
      const texture = g.Texture.all.find(t => t.uuid === model.player?.texture);
      if (!texture || texture.width !== 64 || texture.height !== 64) error(`${model.id}: skin exige uma textura 64×64.`);
    }
  }
  for (const group of groups) {
    if (!group.name?.trim()) error('Existe uma pasta sem nome.');
    if (!group.origin.every(Number.isFinite) || !group.rotation.every(Number.isFinite)) error(`${group.name}: pivô/rotação inválidos.`);
    const visited = new Set([group]); let parent = group.parent;
    while (parent instanceof g.Group) { if (visited.has(parent)) { error(`${group.name}: ciclo na hierarquia.`); break; } visited.add(parent); parent = parent.parent; }
    if (roots.has(group.uuid)) {
      for (const p of visited) if (p !== group && roots.has(p.uuid)) error(`${group.name}: modelos independentes não podem ser aninhados um dentro do outro.`);
    }
  }
  const names = groups.map(g => g.name);
  if (new Set(names).size !== names.length) warnings.push('Há nomes de pastas repetidos. O Studio usa UUIDs estáveis para distinguir os nós.');
  const isReference = node => state.context?.geometry?.includes(node.uuid);
  const owned = node => {
    let p = node; const seen = new Set();
    while (p && !seen.has(p)) { if (roots.has(p.uuid)) return true; seen.add(p); p = p.parent; }
    return false;
  };
  for (const element of elements) {
    if (isReference(element) || element instanceof g.Locator || element.export === false) continue;
    if (!owned(element)) { error(`${element.name}: geometria fora dos modelos declarados. Mova-a para uma pasta de modelo.`); continue; }
    if (!(element instanceof g.Cube || element instanceof g.Mesh)) { error(`${element.name}: tipo de geometria não suportado pelo Studio.`); continue; }
    if (element instanceof g.Cube && element.from.some((v, i) => !Number.isFinite(v) || !Number.isFinite(element.to[i]) || v > element.to[i])) error(`${element.name}: dimensões de cubo inválidas.`);
    if (element instanceof g.Mesh) {
      if (Object.values(element.vertices).some(v => !v.every(Number.isFinite))) error(`${element.name}: vértice inválido.`);
      for (const face of Object.values(element.faces)) if (face.vertices.some(id => !Object.hasOwn(element.vertices, id))) error(`${element.name}: face referencia vértice removido.`);
    }
    for (const [direction, face] of Object.entries(element.faces || {})) {
      if (face.texture === null || face.enabled === false) continue;
      const texture = face.getTexture();
      if (!texture) { error(`${element.name}/${direction}: textura ausente. Crie/aplique uma textura.`); continue; }
      if (texture.render_mode !== 'default') error(`${texture.name}: converta o material do modelo para o modo Default; efeitos aditivos/emissivos pertencem aos VFX Studio.`);
      if (!texture.img?.complete || !texture.img.naturalWidth) error(`${texture.name}: aguarde o carregamento da imagem ou recarregue a textura.`);
      const uvs = Array.isArray(face.uv) ? [face.uv.slice(0, 2), face.uv.slice(2, 4)] : Object.values(face.uv || {});
      if (uvs.some(v => v.some(n => !Number.isFinite(n)))) error(`${element.name}/${direction}: UV inválido.`);
      // UV wrapping is intentional in native Blockbench; diagnose it rather than silently clamp it.
      if (uvs.some(v => v[0] < 0 || v[1] < 0 || v[0] > texture.getUVWidth() || v[1] > texture.getUVHeight())) warnings.push(`${element.name}/${direction}: UV fora da imagem (wrap ${texture.wrap_mode}).`);
    }
  }
  const renderable = element => {
    for (let p = element; p && p !== 'root'; p = p.parent) if (p.export === false) return false;
    return element instanceof g.Cube ? Object.values(element.faces).some(face => face.texture !== null)
      : element instanceof g.Mesh && Object.values(element.faces).some(face => face.vertices.length >= 3);
  };
  for (const model of state.models) if (model.type !== 'effect' && !elements.some(e => owned(e) && !isReference(e) && renderable(e) && model.roots.some(root => {
    let p = e; while (p && p !== 'root') { if (p.uuid === root) return true; p = p.parent; } return false;
  }))) error(`${model.id}: modelo sem geometria exportável. Modele e habilite suas faces antes de exportar.`);
  for (const effect of state.effects) {
    errors.push(...validateEffect(effect));
    if (effect.texture && !g.Texture.all.some(t => t.uuid === effect.texture)) error(`${effect.id}: textura de sprite removida.`);
    const texture = g.Texture.all.find(t => t.uuid === effect.texture);
    if (texture && (!texture.img.complete || !texture.img.naturalWidth || texture.width % effect.columns || texture.height % effect.rows)) error(`${effect.id}: sprite não carregado ou dimensões incompatíveis com a grade de frames.`);
  }
  for (const texture of g.Texture.all) {
    const material = state.materials[texture.uuid];
    if (material && !['OPAQUE', 'MASK'].includes(material.alphaMode)) error(`${texture.name}: o perfil de modelo aceita OPAQUE ou MASK.`);
    if (material && (!Number.isFinite(material.alphaCutoff) || material.alphaCutoff < 0 || material.alphaCutoff > 1)) error(`${texture.name}: corte alpha inválido.`);
  }
  const clips = new Set();
  for (const animation of g.Animation.all) {
    if (!animation.name.trim() || clips.has(animation.name)) error(`Clip sem nome ou repetido: ${animation.name}.`); clips.add(animation.name);
    if (!(animation.length > 0 && animation.length <= 1000 && Number.isFinite(animation.length))) error(`${animation.name}: duração deve ficar entre 0 e 1000 s, limite da timeline nativa 5.2.2.`);
    if (!Number.isFinite(animation.snapping) || animation.snapping < 1 || animation.snapping > 120) error(`${animation.name}: a timeline nativa 5.2.2 suporta 1–120 FPS.`);
    for (const property of ['anim_time_update', 'start_delay', 'loop_delay']) if (animation[property] && animation[property] !== '0') error(`${animation.name}: ${property} altera o relógio do clip e não pertence ao contrato Studio. Use tempos e eventos explícitos.`);
    if (animation.blend_weight && !Number.isFinite(Number(animation.blend_weight))) error(`${animation.name}: peso de blend dinâmico não exportável. Use um valor numérico.`);
    if (animation.length * state.sampleRate * Math.max(groups.length, 1) > 2000000) error(`${animation.name}: exportação excede 2 milhões de amostras de grupos. Reduza duração/FPS.`);
    for (const [uuid, animator] of Object.entries(animation.animators)) {
      if (uuid === 'effects') {
        for (const channel of ['particle', 'sound', 'timeline']) {
          if (channel !== CHANNEL && animator[channel]?.length) error(`${animation.name}: use os eventos Studio em vez da trilha legada ${channel}.`);
        }
        continue;
      }
      if (!groups.some(group => group.uuid === uuid)) error(`${animation.name}: canal de animação referencia pasta removida.`);
      for (const kf of animator.keyframes) {
        if (!Number.isFinite(kf.time) || kf.time < 0 || kf.time > animation.length) error(`${animation.name}: keyframe de transformação fora da duração do clip.`);
        if (!['position', 'rotation', 'scale'].includes(kf.channel)) error(`${animation.name}: canal ${kf.channel} não exportável.`);
        if (!['linear', 'step', 'bezier', 'catmullrom'].includes(kf.interpolation)) error(`${animation.name}: interpolação ${kf.interpolation} desconhecida.`);
        for (const point of kf.data_points) if (['x', 'y', 'z'].some(axis => point[axis] === '' || !Number.isFinite(Number(point[axis]))))
          error(`${animation.name}: expressões dinâmicas não são um contrato de exportação. Use valores numéricos nos keyframes.`);
      }
    }
    for (const event of studio.events(animation)) {
      try { studio.validateEvent(event); } catch (e) { error(`${animation.name} / frame ${event.frame}: ${e.message}`); }
      if (!Number.isFinite(event.time) || event.time < 0 || event.time > animation.length) error(`${animation.name}: evento fora da duração do clip.`);
    }
  }
  return { valid: !errors.length, errors: [...new Set(errors)], warnings: [...new Set(warnings)] };
}
