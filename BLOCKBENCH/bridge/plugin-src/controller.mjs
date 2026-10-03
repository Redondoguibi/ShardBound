import { tools, mtools, validateMesh } from '../shared/tools.mjs';
import { zodToJsonSchema } from 'zod-to-json-schema';

export function createController(g) {
  let revision = 0;
  const changed = () => {
    revision++;
  };
  const events = [
    'finished_edit',
    'undo',
    'redo',
    'select_project',
    'update_selection',
  ];
  events.forEach((e) => g.Blockbench.on(e, changed));
  const snapshot = () => ({ project_id: g.Project?.uuid ?? null, revision });
  const find = (id) => {
    const node = [...g.Outliner.elements, ...g.Group.all].find(
      (e) => e.uuid === id,
    );
    if (!node) throw new Error(`Element not found: ${id}`);
    return node;
  };
  const requireProject = (args, edit = true) => {
    if (!g.Project || g.Format?.id !== 'free')
      throw new Error('Open a Generic Model project first.');
    if (args.project_id && g.Project.uuid !== args.project_id)
      throw new Error('Active project changed. Read the project again.');
    if (
      args.expected_revision !== undefined &&
      args.expected_revision !== revision
    )
      throw new Error('Project revision changed. Read the project again.');
    if (edit && (g.Dialog.open || g.Undo.current_save))
      throw new Error('Finish the active dialog/edit in Blockbench first.');
    if (edit && g.Modes.selected?.id !== 'edit')
      throw new Error('Switch Blockbench to Edit mode first.');
  };
  const refresh = () => {
    g.Canvas.updateAll();
    g.updateSelection();
    changed();
  };
  const transaction = (label, elements, groups, fn, extra = {}) => {
    g.Undo.initEdit({
      elements,
      groups,
      outliner: true,
      selection: true,
      ...extra,
    });
    try {
      const result = fn();
      g.Undo.finishEdit(label);
      refresh();
      return result;
    } catch (error) {
      g.Undo.cancelEdit(true);
      refresh();
      throw error;
    }
  };
  const summary = () => ({
    ...snapshot(),
    name: g.Project?.name,
    format: g.Format?.id,
    blockbench: g.Blockbench.version,
    mtools:
      g.Plugins.all.find(
        (p) => p.id === 'mesh_tools' && p.installed && !p.disabled,
      )?.version ?? null,
    elements: g.Project ? g.Outliner.elements.length : 0,
    undo: g.Project
      ? {
          index: g.Undo.index,
          total: g.Undo.history.length,
          previous: g.Undo.history[g.Undo.index - 1]?.action,
          next: g.Undo.history[g.Undo.index]?.action,
        }
      : null,
  });
  const textureId = (value) =>
    value === false || value == null ? null : value;
  const describe = (e, geometry = false) => {
    const data = {
      id: e.uuid,
      type: e.type,
      name: e.name,
      parent: e.parent?.uuid ?? 'root',
      origin: e.origin,
      rotation: e.rotation,
      visibility: e.visibility,
      selected: e.selected,
    };
    if (e instanceof g.Mesh) {
      data.vertex_count = Object.keys(e.vertices).length;
      data.face_count = Object.keys(e.faces).length;
      data.selection = {
        vertices: e.getSelectedVertices(),
        edges: e.getSelectedEdges(),
        faces: e.getSelectedFaces(),
      };
      if (geometry) {
        data.vertices = e.vertices;
        data.faces = Object.fromEntries(
          Object.entries(e.faces).map(([k, f]) => [
            k,
            { vertices: f.vertices, uv: f.uv, texture: textureId(f.texture) },
          ]),
        );
      }
    }
    if (e instanceof g.Cube) {
      data.from = e.from;
      data.to = e.to;
      data.box_uv = !!e.box_uv;
      data.uv_offset = e.uv_offset;
      data.inflate = e.inflate;
      data.shade = e.shade;
      data.mirror_uv = e.mirror_uv;
      if (geometry)
        data.faces = Object.fromEntries(
          Object.entries(e.faces).map(([direction, f]) => [
            direction,
            {
              uv: f.uv,
              texture: textureId(f.texture),
              rotation: f.rotation,
              enabled: f.enabled,
            },
          ]),
        );
    }
    if (e instanceof g.Group) data.children = e.children.map((c) => c.uuid);
    return data;
  };
  const validateTextures = (faces) => {
    for (const f of Object.values(faces || {}))
      if (f?.texture && !g.Texture.all.some((t) => t.uuid === f.texture))
        throw new Error(`Texture not found: ${f.texture}`);
  };
  const normalizeCubeFaces = (faces) =>
    Object.fromEntries(
      Object.entries(faces || {}).map(([direction, face]) => [
        direction,
        { ...face, ...(face.texture === null ? { texture: false } : {}) },
      ]),
    );
  const validateGeometry = () => {
    for (const m of g.Mesh.all)
      for (const v of Object.values(m.vertices))
        if (v.some((n) => !Number.isFinite(n)))
          throw new Error(
            'MTools produced invalid vertex coordinates; operation reverted.',
          );
  };
  const toBase64 = (data) => {
    const bytes =
      data instanceof Uint8Array
        ? data
        : new Uint8Array(
            data.buffer ?? data,
            data.byteOffset ?? 0,
            data.byteLength ?? undefined,
          );
    let binary = '';
    for (let i = 0; i < bytes.length; i += 0x8000)
      binary += String.fromCharCode(
        ...bytes.subarray(i, Math.min(i + 0x8000, bytes.length)),
      );
    return btoa(binary);
  };
  const handlers = {
    status: summary,
    read_project(a) {
      requireProject(a, false);
      const all = [...g.Group.all, ...g.Outliner.elements];
      return {
        ...summary(),
        selection_mode: g.BarItems.selection_mode.value,
        total: all.length,
        next_offset:
          a.offset + a.limit < all.length ? a.offset + a.limit : null,
        elements: all
          .slice(a.offset, a.offset + a.limit)
          .map((e) => describe(e, a.geometry)),
        textures: g.Texture.all.map((t) => ({
          id: t.uuid,
          name: t.name,
          width: t.width,
          height: t.height,
        })),
      };
    },
    create_project(a) {
      if (g.Dialog.open || (g.Project && g.Undo.current_save))
        throw new Error('Finish the active dialog/edit first.');
      if (!g.newProject(g.Formats.free))
        throw new Error('Blockbench could not create a project.');
      g.Project.name = a.name;
      g.Project.texture_width = a.texture_width;
      g.Project.texture_height = a.texture_height;
      changed();
      return summary();
    },
    create_elements(a) {
      requireProject(a);
      const parents = a.elements.map((e) =>
        e.parent ? find(e.parent) : 'root',
      );
      for (const p of parents)
        if (p !== 'root' && !(p instanceof g.Group))
          throw new Error('Parent must be a group.');
      for (const e of a.elements) {
        if (e.type === 'mesh') {
          validateMesh(e);
          validateTextures(e.faces);
        }
        if (e.type === 'cube') {
          if (e.from.some((n, i) => n > e.to[i]))
            throw new Error('Cube from must be <= to.');
          validateTextures(e.faces);
        }
      }
      const elements = [],
        groups = [];
      return transaction('Bridge: Create elements', elements, groups, () => {
        const created = a.elements.map((e, i) => {
          const { type, parent, faces, ...data } = e;
          const ctor = { group: g.Group, cube: g.Cube, mesh: g.Mesh }[type];
          const payload =
            type === 'cube'
              ? {
                  ...data,
                  ...(faces ? { faces: normalizeCubeFaces(faces) } : {}),
                }
              : type === 'mesh'
                ? { ...data, faces }
                : data;
          const node = new ctor(payload);
          (type === 'group' ? groups : elements).push(node);
          node.addTo(parents[i]).init();
          return describe(node, type === 'cube');
        });
        return { created };
      });
    },
    update_elements(a) {
      requireProject(a);
      const nodes = a.updates.map((u) => find(u.id));
      a.updates.forEach((u, i) => {
        const n = nodes[i];
        const cubeOnly =
          u.from ||
          u.to ||
          u.faces ||
          u.box_uv !== undefined ||
          u.uv_offset ||
          u.inflate !== undefined ||
          u.shade !== undefined ||
          u.mirror_uv !== undefined;
        if (cubeOnly && !(n instanceof g.Cube))
          throw new Error(
            'Cube bounds/UV/face properties are only valid for cubes.',
          );
        if (
          n instanceof g.Cube &&
          (u.from ?? n.from).some((v, k) => v > (u.to ?? n.to)[k])
        )
          throw new Error('Invalid cube bounds.');
        if (u.faces) validateTextures(u.faces);
      });
      return transaction(
        'Bridge: Update elements',
        nodes.filter((n) => !(n instanceof g.Group)),
        nodes.filter((n) => n instanceof g.Group),
        () => {
          a.updates.forEach(({ id, faces, ...data }, i) => {
            const node = nodes[i];
            node.extend(data);
            if (faces && node instanceof g.Cube)
              for (const [direction, face] of Object.entries(
                normalizeCubeFaces(faces),
              ))
                node.faces[direction].extend(face);
          });
          return { updated: nodes.map((n) => n.uuid) };
        },
      );
    },
    edit_mesh(a) {
      requireProject(a);
      const node = find(a.id);
      if (!(node instanceof g.Mesh)) throw new Error('Expected a mesh UUID.');
      const data = {
        vertices: structuredClone(node.vertices),
        faces: Object.fromEntries(
          Object.entries(node.faces).map(([k, f]) => [
            k,
            {
              vertices: [...f.vertices],
              uv: structuredClone(f.uv),
              texture: f.texture || null,
            },
          ]),
        ),
      };
      for (const k of a.delete_vertices) {
        if (!Object.hasOwn(data.vertices, k))
          throw new Error(`Unknown vertex: ${k}`);
        delete data.vertices[k];
      }
      for (const k of a.delete_faces) {
        if (!Object.hasOwn(data.faces, k))
          throw new Error(`Unknown face: ${k}`);
        delete data.faces[k];
      }
      Object.assign(data.vertices, a.vertices);
      Object.assign(data.faces, a.faces);
      validateMesh(data);
      validateTextures(data.faces);
      return transaction('Bridge: Edit mesh', [node], [], () => {
        node.vertices = data.vertices;
        node.faces = {};
        for (const [k, f] of Object.entries(data.faces))
          node.faces[k] = new g.MeshFace(node, f);
        return { updated: node.uuid };
      });
    },
    delete_elements(a) {
      requireProject(a);
      const nodes = a.ids.map(find);
      if (nodes.some((n) => n instanceof g.Group && n.children.length))
        throw new Error('Delete children before deleting a nonempty group.');
      return transaction(
        'Bridge: Delete elements',
        nodes.filter((n) => !(n instanceof g.Group)),
        nodes.filter((n) => n instanceof g.Group),
        () => {
          nodes.forEach((n) => n.remove());
          return { deleted: a.ids };
        },
      );
    },
    select_elements(a) {
      requireProject(a);
      const nodes = a.ids.map(find);
      if (nodes.some((n) => n instanceof g.Group))
        throw new Error(
          'Select individual meshes or cubes for component operations.',
        );
      const selection = new Map();
      for (const n of nodes.filter((n) => n instanceof g.Mesh)) {
        const c = a.components.find((c) => c.id === n.uuid) ?? {
          vertices: [],
          edges: [],
          faces: [],
        };
        const faces = a.all_components ? Object.keys(n.faces) : c.faces;
        const edges = a.all_components
          ? Object.values(n.faces).flatMap((f) =>
              f
                .getSortedVertices()
                .map((v, i, vs) => [v, vs[(i + 1) % vs.length]]),
            )
          : c.edges;
        if (
          faces.some((f) => !n.faces[f]) ||
          [...c.vertices, ...edges.flat()].some((v) => !n.vertices[v])
        )
          throw new Error(
            'Unknown vertex, edge endpoint or face in selection.',
          );
        const vertices = a.all_components
          ? Object.keys(n.vertices)
          : [
              ...new Set([
                ...c.vertices,
                ...edges.flat(),
                ...faces.flatMap((f) => n.faces[f].vertices),
              ]),
            ];
        selection.set(n.uuid, {
          vertices,
          faces: a.mode === 'face' ? faces : [],
          edges: a.mode === 'edge' ? edges : [],
        });
      }
      if (a.components.some((c) => !selection.has(c.id)))
        throw new Error('Components must belong to a selected mesh.');
      g.Undo.initSelection();
      g.unselectAllElements();
      g.Project.mesh_selection = {};
      g.BarItems.selection_mode.set(a.mode);
      nodes.forEach((n) => n.markAsSelected());
      for (const [id, c] of selection) g.Project.mesh_selection[id] = c;
      g.updateSelection();
      g.Canvas.updateView({ elements: nodes, selection: true });
      g.Undo.finishSelection('Bridge: Select elements');
      changed();
      return { selected: a.ids, mode: a.mode };
    },
    async capture_view(a) {
      requireProject(a, false);
      const url = await new Promise((resolve) =>
        g.Screencam.screenshotPreview(
          g.Preview.selected,
          { ...a, crop: false },
          resolve,
        ),
      );
      return { image: url };
    },
    undo_redo(a) {
      requireProject(a);
      if (
        (a.direction === 'undo' && g.Undo.index === 0) ||
        (a.direction === 'redo' && g.Undo.index === g.Undo.history.length)
      )
        throw new Error('No history entry in this direction.');
      g.Undo[a.direction]();
      refresh();
      return summary();
    },
    mtools_list() {
      return {
        version: summary().mtools,
        actions: Object.entries(g.BarItems)
          .filter(
            ([id, item]) =>
              id.startsWith('@mesh_tools/') && typeof item.click === 'function',
          )
          .map(([id, item]) => {
            const key = id.split('/')[1],
              spec = mtools[key];
            return {
              id: key,
              name: item.name,
              description: item.description,
              available: !!g.Condition(item.condition),
              automated: !!spec,
              selection_mode: spec?.mode,
              parameters: spec ? zodToJsonSchema(spec.schema) : undefined,
            };
          }),
      };
    },
    async mtools_run(a) {
      requireProject(a);
      const spec = mtools[a.action],
        params = spec.schema.parse(a.parameters);
      const actionParams = { ...params };
      for (const key of spec.adapterKeys ?? []) delete actionParams[key];
      const action = g.BarItems[`@mesh_tools/${a.action}`];
      if (!action || !summary().mtools)
        throw new Error('Install and enable the supplied MTools plugin first.');
      if (summary().mtools !== '2.1.0')
        throw new Error(
          'This adapter was validated for MTools 2.1.0. Review the adapter before using a different version.',
        );
      if (!spec.generator) {
        if (!g.Mesh.selected.length) throw new Error('Select meshes first.');
        const currentMode = g.BarItems.selection_mode.value;
        const allowedModes = Array.isArray(spec.mode) ? spec.mode : [spec.mode];
        if (!allowedModes.includes(currentMode))
          throw new Error(
            `Use selection mode ${allowedModes.join(' or ')} first.`,
          );
        if (
          currentMode === 'face' &&
          g.Mesh.selected.some((m) => !m.getSelectedFaces().length)
        )
          throw new Error('Select faces in each selected mesh.');
        if (
          currentMode === 'vertex' &&
          g.Mesh.selected.some((m) => !m.getSelectedVertices().length)
        )
          throw new Error('Select vertices first.');
        if (
          currentMode === 'edge' &&
          g.Mesh.selected.some((m) => !m.getSelectedEdges().length)
        )
          throw new Error('Select edges first.');
        if (a.action === 'scatter' && g.Mesh.selected.length < 2)
          throw new Error(
            'MTools Scatter requires at least two selected meshes; the last selected mesh is the target surface.',
          );
        if (a.action === 'boolean' && g.Mesh.selected.length !== 2)
          throw new Error(
            'MTools Boolean requires exactly two selected meshes.',
          );
        if (
          a.action === 'bridge_edge_loops' &&
          g.Mesh.selected.some((m) =>
            g.BarItems.selection_mode.value === 'face'
              ? m.getSelectedFaces().length < 2
              : m.getSelectedEdges().length < 6,
          )
        )
          throw new Error(
            'Select at least two faces or two edge loops per mesh.',
          );
        // MTools 2.1.0 smooth reads every neighbor from its selected-vertex cache.
        if (a.action === 'laplacian_smooth')
          for (const m of g.Mesh.selected) {
            const selected = new Set(m.getSelectedVertices());
            for (const v of selected) {
              const neighbors = new Set(
                Object.values(m.faces)
                  .filter((f) => f.vertices.includes(v))
                  .flatMap((f) => f.vertices.filter((k) => k !== v)),
              );
              if (
                !neighbors.size ||
                [...neighbors].some((k) => !selected.has(k))
              )
                throw new Error(
                  'MTools smooth requires a full connected component selection; use all_components.',
                );
            }
          }
      }
      if (!g.Condition(action.condition))
        throw new Error(
          'MTools action is unavailable for the current selection.',
        );
      const undo = g.Undo,
        history = undo.history.slice(),
        index = undo.index;
      const aspects = {
        elements: g.Outliner.elements.slice(),
        groups: g.Group.all.slice(),
        outliner: true,
        selection: true,
        textures: g.Texture.all.slice(),
      };
      const before = new g.UndoSystem.save(aspects),
        selectionBefore = new g.UndoSystem.selectionSave();
      const oldAmend = undo.amendEdit,
        oldInit = undo.initEdit,
        oldFinish = undo.finishEdit,
        oldShow = g.Dialog.prototype.show,
        oldMessageBox = g.Blockbench.showMessageBox;
      let amendment, dialog;
      undo.amendEdit = function (form, callback) {
        amendment = { form, callback };
      };
      // MTools Scatter omits its new groups from native Undo on Blockbench 5.2.
      // Track them for both the initial action and its parameter amendment.
      if (spec.trackGroups) {
        undo.initEdit = function (aspects, amended) {
          return oldInit.call(
            this,
            { ...aspects, groups: g.Group.all.slice() },
            amended,
          );
        };
        undo.finishEdit = function (label, aspects) {
          return oldFinish.call(this, label, {
            ...(aspects ?? this.current_save?.aspects),
            groups: g.Group.all.slice(),
          });
        };
      }
      if (spec.dialog)
        g.Dialog.prototype.show = function (...args) {
          if (dialog) throw new Error('Unexpected additional dialog.');
          dialog = this;
          return oldShow.apply(this, args);
        };
      if (spec.autoConfirmMessage)
        g.Blockbench.showMessageBox = function (options, callback) {
          if (typeof callback === 'function') {
            callback(0, { dont_show_again: false });
            return;
          }
          return oldMessageBox.apply(this, arguments);
        };
      try {
        await action.click.call(action);
        if (spec.dialog) {
          if (!dialog)
            throw new Error(
              'Expected MTools parameters dialog was not opened.',
            );
          dialog.setFormValues(actionParams);
          dialog.confirm();
          if (g.Dialog.open === dialog)
            throw new Error('MTools did not accept the dialog parameters.');
        } else if (spec.amend) {
          if (!amendment || undo.index === index)
            throw new Error('MTools did not create an amendable operation.');
          for (const k of Object.keys(actionParams))
            if (!Object.hasOwn(amendment.form, k))
              throw new Error(`MTools parameter changed: ${k}`);
          undo.undo(null, true);
          amendment.callback(actionParams);
        }
        validateGeometry();
        refresh();
        return {
          action: a.action,
          parameters: params,
          selection: g.Mesh.selected.map((m) => m.uuid),
          elements: g.Outliner.elements.map((e) => ({
            id: e.uuid,
            name: e.name,
            type: e.type,
          })),
        };
      } catch (error) {
        if (dialog && g.Dialog.open === dialog) dialog.hide();
        if (undo.current_save) undo.cancelEdit(true);
        undo.loadSave(
          before,
          new g.UndoSystem.save({
            ...aspects,
            elements: g.Outliner.elements.slice(),
            groups: g.Group.all.slice(),
            textures: g.Texture.all.slice(),
          }),
        );
        selectionBefore.load(new g.UndoSystem.selectionSave());
        undo.history = history;
        undo.index = index;
        refresh();
        throw error;
      } finally {
        undo.amendEdit = oldAmend;
        undo.initEdit = oldInit;
        undo.finishEdit = oldFinish;
        g.Dialog.prototype.show = oldShow;
        g.Blockbench.showMessageBox = oldMessageBox;
      }
    },
    async put_texture(a) {
      requireProject(a);
      let url = a.png;
      if (!url) {
        const canvas = g.document.createElement('canvas');
        canvas.width = a.width;
        canvas.height = a.height;
        const ctx = canvas.getContext('2d');
        ctx.fillStyle = a.color;
        ctx.fillRect(0, 0, a.width, a.height);
        url = canvas.toDataURL('image/png');
      }
      const image = await new Promise((resolve, reject) => {
        const i = new g.Image();
        i.onload = () => resolve(i);
        i.onerror = () => reject(new Error('Invalid PNG'));
        i.src = url;
      });
      if (image.width > 2048 || image.height > 2048)
        throw new Error('Texture limit is 2048 x 2048.');
      requireProject(a);
      let texture = a.id ? g.Texture.all.find((t) => t.uuid === a.id) : null;
      if (a.id && !texture) throw new Error('Texture UUID not found.');
      const textures = texture ? [texture] : [];
      return transaction(
        'Bridge: Texture',
        [],
        [],
        () => {
          if (!texture) {
            texture = new g.Texture({ name: a.name });
            textures.push(texture);
            texture.fromDataURL(url).add();
          } else {
            texture.name = a.name;
            texture.fromDataURL(url);
          }
          texture.saved = false;
          return { texture_id: texture.uuid };
        },
        { textures, bitmap: true },
      );
    },
    get_texture(a) {
      requireProject(a, false);
      const texture = g.Texture.all.find((t) => t.uuid === a.id);
      if (!texture) throw new Error('Texture UUID not found.');
      return { image: `data:image/png;base64,${texture.getBase64()}` };
    },
    save_project(a) {
      requireProject(a, false);
      const content = g.Codecs.project.compile({
        bitmaps: true,
        absolute_paths: false,
      });
      return { format: 'bbmodel', content };
    },
    async export_project(a) {
      requireProject(a, false);
      const codec =
        g.Codecs[
          a.format === 'bbmodel'
            ? 'project'
            : a.format === 'glb'
              ? 'gltf'
              : a.format
        ];
      if (!codec) throw new Error(`Codec unavailable: ${a.format}`);
      let content;
      if (a.format === 'gltf' || a.format === 'glb')
        content = await codec.compile({
          encoding: a.format === 'glb' ? 'binary' : 'ascii',
          embed_textures: true,
          animations: false,
          armature: false,
          merge_armature: false,
        });
      else if (a.format === 'bbmodel')
        content = await codec.compile({
          compressed: false,
          raw: false,
          bitmaps: true,
          absolute_paths: false,
        });
      else content = await codec.compile({ compressed: false, raw: false });
      if (a.format === 'obj')
        content = content.replace(/^(?:mtllib|usemtl)\s.*\r?\n/gm, '');
      if (content instanceof ArrayBuffer || ArrayBuffer.isView(content))
        return {
          format: a.format,
          binary: true,
          content_base64: toBase64(content),
        };
      return {
        format: a.format,
        binary: false,
        content:
          typeof content === 'string' ? content : JSON.stringify(content),
      };
    },
  };
  const internalHandlers = {
    __open_project_data(a) {
      if (g.Dialog.open || (g.Project && g.Undo.current_save))
        throw new Error('Finish the active dialog/edit first.');
      let model;
      try {
        model = JSON.parse(a.content);
      } catch {
        throw new Error('Invalid .bbmodel JSON.');
      }
      if (!model?.meta) throw new Error('Invalid .bbmodel: missing meta.');
      if (model.meta.model_format && model.meta.model_format !== 'free')
        throw new Error(
          `Expected Generic Model (.meta.model_format=free), got ${model.meta.model_format}.`,
        );
      g.Codecs.project.load(model, { path: a.path, no_file: false });
      if (g.Format?.id !== 'free')
        throw new Error('Loaded project is not a Generic Model.');
      changed();
      return summary();
    },
  };
  return {
    async call(name, args) {
      if (Object.hasOwn(internalHandlers, name))
        return { ...(await internalHandlers[name](args)), ...snapshot() };
      if (!Object.hasOwn(tools, name)) throw new Error('Unknown tool');
      const parsed = tools[name].schema.parse(args);
      if (!Object.hasOwn(handlers, name))
        throw new Error(
          `Tool ${name} is handled by the MCP server and should not reach Blockbench directly.`,
        );
      const result = await handlers[name](parsed);
      return { ...result, ...snapshot() };
    },
    dispose() {
      events.forEach((e) => g.Blockbench.removeListener(e, changed));
    },
  };
}
