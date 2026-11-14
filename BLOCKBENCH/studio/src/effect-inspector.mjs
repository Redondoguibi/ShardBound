import { CHANNEL, clone, validateEffect } from './core.mjs';

// Extends the native keyframe properties. No Studio panel, window or second viewport.
export function createEffectInspector(g, studio, { paintSprite, addAudio, chooseResource }) {
  const host = g.document.createElement('div'); g.Panels.keyframe.node.append(host);
  const field = (key, label, min = 0, max = 10000, step = 0.1) => ({ key, label, type: 'number', min, max, step });
  const choice = (key, label, options) => ({ key, label, type: 'select', options });
  const vector = (key, label) => ({ key, label, type: 'vector' });
  const color = (key, label) => ({ key, label, type: 'color' });
  const groups = e => [
    { name: 'Aparência', open: true, fields: [color('color', 'Cor inicial'), color('colorEnd', 'Cor final'),
      field('opacity', 'Opacidade inicial', 0, 1, 0.01), field('opacityEnd', 'Opacidade final', 0, 1, 0.01),
      ...(e.kind === 'flash' ? [] : [field('size', 'Tamanho inicial'), field('sizeEnd', 'Tamanho final'),
        choice('blend', 'Mistura', { alpha: 'Transparência', additive: 'Aditiva' })])] },
    { name: 'Emissão e duração', open: true, fields: [field('duration', 'Duração (s)', 0.001, 3600),
      ...(e.kind === 'particle' ? [field('burst', 'Quantidade no disparo', 0, 4096, 1), field('rate', 'Quantidade por segundo', 0, 4096, 1),
        field('lifetime', 'Vida da partícula (s)', 0.001, 120), choice('shape', 'Forma do emissor', { point: 'Ponto', box: 'Caixa', sphere: 'Esfera', cone: 'Cone' }), vector('shapeSize', 'Dimensões')] : [])] },
    { name: 'Movimento', visible: e.kind === 'particle', fields: [vector('direction', 'Direção'), field('spread', 'Abertura (°)', 0, 180),
      field('speed', 'Velocidade'), field('speedSpread', 'Variação'), vector('gravity', 'Gravidade'), field('drag', 'Arrasto', 0, 100),
      field('rotation', 'Rotação (°)', -36000, 36000), field('angularVelocity', 'Rotação por segundo', -36000, 36000)] },
    { name: 'Rastro', visible: e.kind === 'trail', fields: [field('trailLength', 'Comprimento temporal (s)', 0.001, 30),
      field('trailSamples', 'Amostras', 2, 240, 1), field('width', 'Largura', 0.001), vector('startOffset', 'Início da faixa'), vector('endOffset', 'Fim da faixa')] },
    { name: 'Iluminação', visible: e.kind === 'glow', fields: [field('intensity', 'Intensidade'), field('distance', 'Alcance')] },
    { name: 'Sprite e atlas', visible: e.kind !== 'flash', fields: e.kind === 'particle' ? [field('columns', 'Colunas', 1, 256, 1),
      field('rows', 'Linhas', 1, 256, 1), field('frames', 'Frames', 1, 65536, 1), field('fps', 'FPS', 0, 240)] : [] },
    { name: 'Avançado', fields: [{ key: 'id', label: 'ID do recurso', type: 'text' }, choice('kind', 'Aplicação visual', { particle: 'Sprites emitidos', trail: 'Faixa ao longo do movimento', flash: 'Cor sobre a superfície', glow: 'Sprite com luz' }),
      ...(e.kind === 'particle' ? [field('seed', 'Semente', 0, 0xffffffff, 1), { key: 'follow', label: 'Partículas acompanham a origem', type: 'checkbox' }] : [])] },
  ].filter(section => section.visible !== false);
  let syncing = false, signature = '', activeSelection = '', project = null, disposed = false;
  const vm = new g.Vue({
    data: () => ({ visible: false, events: [], eventId: '', event: null, definition: null, textures: [], targets: [],
      error: '', loopPreview: false, curveChannel: 'size', curvePoints: [], resources: [] }),
    computed: {
      sections() { return this.definition ? groups(this.definition) : []; },
      curveOptions() { return this.definition?.kind === 'flash' ? { opacity: 'Opacidade', color: 'Cor' }
        : { size: 'Tamanho', opacity: 'Opacidade', color: 'Cor', ...(this.definition?.kind === 'particle' ? { rotation: 'Rotação' } : {}) }; },
    },
    methods: {
      selectEvent() { signature = ''; sync(true); },
      moveEvent() {
        try { studio.updateEvent(this.eventId, {}, Number(this.event.time)); this.error = ''; signature = ''; sync(true); }
        catch (error) { this.error = error.message; signature = ''; sync(true); }
      },
      fieldInput() {
        const errors = validateEffect(this.definition); this.error = errors.join('\n');
        if (errors.length) return;
        studio.preview.setDraft(clone(this.definition));
        if (this.loopPreview) this.startPreview(); else studio.preview.render();
      },
      fieldChange() {
        if (syncing || !this.definition) return;
        try {
          const id = studio.events().find(e => e.id === this.eventId)?.resource; if (!id) return;
          syncing = true; studio.updateEffect(id, clone(this.definition)); this.error = ''; studio.preview.clearDraft();
        } catch (error) { this.error = error.message; studio.preview.clearDraft(); }
        finally { syncing = false; signature = ''; sync(true); if (this.loopPreview) this.startPreview(); }
      },
      eventChange() {
        try {
          const e = this.event; studio.updateEvent(this.eventId, { resource: e.resource,
            type: studio.state().effects.find(d => d.id === e.resource)?.kind || 'sound', action: e.action,
            target: e.target, offset: e.offset.slice(), follow: !!e.follow, instance: e.instance, volume: e.volume, pitch: e.pitch, loop: !!e.loop });
          this.error = ''; if (this.loopPreview && this.definition) this.startPreview();
        } catch (error) { this.error = error.message; signature = ''; sync(true); }
      },
      startPreview() {
        try { studio.preview.solo(this.definition.id, this.event.target, this.event); this.loopPreview = true; }
        catch (error) { this.error = error.message; this.loopPreview = false; }
      },
      togglePreview() { if (this.loopPreview) { this.loopPreview = false; studio.preview.stopSolo(); studio.preview.clearDraft(); g.Animator.preview(false); } else this.startPreview(); },
      sprite() { paintSprite(this.definition.id); },
      audio() { addAudio(g.Animation.selected, g.Timeline.time, this.eventId); },
      add() { chooseResource(g.Animation.selected, g.Timeline.time); },
      loadCurve() {
        const e = this.definition, key = this.curveChannel;
        const start = key === 'rotation' ? e.rotation : e[key], end = key === 'rotation' ? e.rotation + e.angularVelocity * e.lifetime : e[key + 'End'];
        this.curvePoints = clone(e.curves[key] || [{ age: 0, value: start }, { age: 1, value: end }]);
      },
      curveInput(commit = false) {
        g.Vue.set(this.definition.curves, this.curveChannel, clone(this.curvePoints)); this.fieldInput(); if (commit && !this.error) this.fieldChange();
      },
      addPoint() {
        if (this.curvePoints.length >= 64) return;
        const sorted = this.curvePoints.slice().sort((a, b) => a.age - b.age);
        let gap = 0, age = 0.5; for (let i = 1; i < sorted.length; i++) if (sorted[i].age - sorted[i - 1].age > gap) { gap = sorted[i].age - sorted[i - 1].age; age = sorted[i - 1].age + gap / 2; }
        this.curvePoints.push({ age, value: this.curvePoints[0].value }); this.curveInput(true);
      },
      removePoint(index) { if (this.curvePoints.length > 2) { this.curvePoints.splice(index, 1); this.curveInput(true); } },
      curveLine() { const max = Math.max(1, ...this.curvePoints.map(p => Number(p.value) || 0)); return this.curvePoints.slice().sort((a, b) => a.age - b.age).map(p => `${p.age * 240},${90 - (Number(p.value) || 0) / max * 80}`).join(' '); },
      dragCurve(index, event) {
        if (this.curveChannel === 'color') return;
        const svg = event.currentTarget.ownerSVGElement, bounds = svg.getBoundingClientRect();
        const max = Math.max(1, ...this.curvePoints.map(p => Number(p.value) || 0));
        const move = ev => { const point = this.curvePoints[index]; point.age = Math.max(0, Math.min(1, (ev.clientX - bounds.left) / bounds.width));
          point.value = Math.max(0, (90 - (ev.clientY - bounds.top) / bounds.height * 100) / 80 * max); this.curveInput(); };
        const up = () => { g.document.removeEventListener('pointermove', move); g.document.removeEventListener('pointerup', up); if (!disposed) this.curveInput(true); cleanupDrag = null; };
        cleanupDrag?.(); cleanupDrag = () => { g.document.removeEventListener('pointermove', move); g.document.removeEventListener('pointerup', up); };
        g.document.addEventListener('pointermove', move); g.document.addEventListener('pointerup', up, { once: true }); event.preventDefault();
      },
      curveY(point) { const max = Math.max(1, ...this.curvePoints.map(p => Number(p.value) || 0)); return 90 - (Number(point.value) || 0) / max * 80; },
    },
    template: `<div v-show="visible" class="sbstudio-keyframe-properties">
      <select v-if="events.length > 1" aria-label="Evento selecionado" v-model="eventId" @change="selectEvent"><option v-for="e in events" :key="e.id" :value="e.id">{{e.resource}} · {{e.action}}</option></select>
      <div v-if="event">
        <details><summary>Evento e origem</summary>
        <label>Tempo do evento (s) <input aria-label="Tempo do evento" type="number" min="0" step="0.01" v-model.number="event.time" @change="moveEvent"></label>
        <label>Recurso <select v-model="event.resource" @change="eventChange"><option v-for="r in resources" :value="r.id">{{r.id}}</option></select></label>
        <label>Ação <select v-model="event.action" @change="eventChange"><option value="burst">Disparar uma vez</option><option value="start">Iniciar</option><option value="stop">Parar instância</option></select></label>
        <label v-if="definition">Origem <select v-model="event.target" @change="eventChange"><option value="">Mundo / origem</option><option v-for="t in targets" :value="t.uuid">{{t.name}}</option></select></label>
        <div v-if="definition" class="sbstudio-field"><span>Deslocamento</span><div class="sbstudio-vector"><input v-for="(n,i) in event.offset" :aria-label="'Deslocamento '+i" type="number" step="0.1" v-model.number="event.offset[i]" @change="eventChange"></div></div>
        <label v-if="definition && definition.kind !== 'trail' && definition.kind !== 'flash'"><input type="checkbox" v-model="event.follow" @change="eventChange"> Acompanhar origem após emitir</label>
        <details><summary>Instância para iniciar/parar</summary><input aria-label="Instância" v-model="event.instance" @change="eventChange"></details>
        </details>
        <div v-if="event.type === 'sound'">
          <label>Volume <input aria-label="Volume" type="range" min="0" max="1" step="0.01" v-model.number="event.volume" @change="eventChange"></label>
          <label>Velocidade <input aria-label="Velocidade do som" type="number" min="0.1" max="4" step="0.1" v-model.number="event.pitch" @change="eventChange"></label>
          <label><input type="checkbox" v-model="event.loop" @change="eventChange"> Repetir até parar</label><button @click="audio">Escolher arquivo de áudio…</button>
        </div>
        <div v-if="definition">
          <div class="sbstudio-preview-controls"><button @click="togglePreview" :title="loopPreview ? 'Voltar ao tempo da timeline' : 'Prévia em loop na viewport'"><i class="material-icons">{{loopPreview ? 'stop' : 'play_arrow'}}</i>{{loopPreview ? 'Tempo da timeline' : 'Prévia em loop'}}</button></div>
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
            <div v-for="(p,i) in curvePoints" class="sbstudio-curve-point"><input aria-label="Idade normalizada" type="number" min="0" max="1" step="0.01" v-model.number="p.age" @input="curveInput(false)" @change="curveInput(true)"><input v-if="curveChannel==='color'" type="color" v-model="p.value" @input="curveInput(false)" @change="curveInput(true)"><input v-else aria-label="Valor da curva" type="number" step="0.01" v-model.number="p.value" @input="curveInput(false)" @change="curveInput(true)"><button :disabled="curvePoints.length<=2" @click="removePoint(i)">×</button></div>
            <button :disabled="curvePoints.length>=64" @click="addPoint">Adicionar ponto</button><small>0 = nascimento; 1 = fim da vida. Arraste os pontos para ajustar.</small>
          </details>
        </div>
        <p v-if="error" class="sbstudio-error">{{error}}</p>
      </div>
    </div>`,
  }).$mount(host);
  let cleanupDrag = null;
  function sync(force = false) {
    if (syncing || disposed) return;
    const selected = studio.isStudio() && g.Modes.animate ? g.Timeline.selected.filter(k => k.channel === CHANNEL) : [];
    const events = selected.length ? studio.events().filter(e => selected.some(k => k.uuid === e.keyframe)) : [];
    const next = `${g.Project?.uuid}:${events.map(e => e.id).join(',')}`;
    if (activeSelection !== next || project !== g.Project) { studio.preview.clearDraft(); if (studio.preview.soloId) studio.preview.stopSolo(); vm.loopPreview = false; vm.error = ''; }
    activeSelection = next;
    vm.visible = !!events.length; if (!events.length) { signature = next; return; }
    vm.events = events; if (!events.some(e => e.id === vm.eventId)) vm.eventId = events[0].id;
    const current = events.find(e => e.id === vm.eventId), definition = studio.state().effects.find(e => e.id === current.resource);
    const dataSignature = next + ':' + vm.eventId + ':' + JSON.stringify([current, definition]);
    if (!force && signature === dataSignature) return;
    project = g.Project; signature = dataSignature;
    vm.event = clone(current); vm.definition = definition ? clone(definition) : null;
    vm.textures = g.Texture.all.map(t => ({ uuid: t.uuid, name: t.name })); vm.targets = [...g.Group.all, ...g.Locator.all].map(t => ({ uuid: t.uuid, name: t.name }));
    vm.resources = [...studio.state().effects, ...studio.state().sounds].map(r => ({ id: r.id }));
    if (vm.definition) { if (!vm.curveOptions[vm.curveChannel]) vm.curveChannel = Object.keys(vm.curveOptions)[0]; vm.loadCurve(); }
  }
  const onPlay = () => { vm.loopPreview = false; studio.preview.clearDraft(); };
  const onMode = () => sync(true);
  g.Blockbench.on('timeline_play', onPlay); g.Blockbench.on('select_mode', onMode);
  const css = g.Blockbench.addCSS(`#panel_keyframe:has(>.sbstudio-keyframe-properties:not([style*="display: none"])){max-height:65vh;min-height:0;overflow-y:auto}.sbstudio-keyframe-properties{padding:8px 6px;max-height:48vh;min-height:0;overflow-y:auto;flex-shrink:1}.sbstudio-keyframe-properties input{max-width:100%;min-width:0}.sbstudio-keyframe-properties input[type=number]{width:100%}.sbstudio-keyframe-properties select{max-width:100%;width:100%}.sbstudio-keyframe-properties label{display:block;margin:5px 0}.sbstudio-keyframe-properties details{border-top:1px solid var(--color-border);padding:6px 0}.sbstudio-keyframe-properties summary{cursor:pointer;font-weight:600;margin:3px 0 8px}.sbstudio-field{display:grid;grid-template-columns:minmax(80px,1fr) minmax(80px,1fr);align-items:center;gap:6px;margin:5px 0}.sbstudio-field input{min-width:0;width:100%}.sbstudio-field input[type=checkbox]{width:auto}.sbstudio-vector,.sbstudio-curve-point{display:flex;gap:3px}.sbstudio-vector input,.sbstudio-curve-point input{min-width:0;width:100%}.sbstudio-curves svg{width:100%;height:100px;background:var(--color-back);overflow:visible}.sbstudio-curves polyline{fill:none;stroke:var(--color-accent);stroke-width:2}.sbstudio-curves circle{fill:var(--color-accent);cursor:move}.sbstudio-curves small{display:block;margin-top:6px;color:var(--color-subtle_text)}.sbstudio-error{color:var(--color-error);white-space:pre-line}.sbstudio-preview-controls button{display:flex;align-items:center;gap:5px;margin:8px 0}`);
  return { vm, refresh: sync, dispose() { disposed = true; cleanupDrag?.(); studio.preview.clearDraft(); studio.preview.stopSolo(); g.Blockbench.removeListener('timeline_play', onPlay); g.Blockbench.removeListener('select_mode', onMode); vm.$destroy(); vm.$el.remove(); css.delete(); } };
}
