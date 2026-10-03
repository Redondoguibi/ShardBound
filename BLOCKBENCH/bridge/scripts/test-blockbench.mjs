import { _electron as electron } from 'playwright-core';
import { build } from 'esbuild';
import { mkdir, writeFile } from 'node:fs/promises';
import { randomBytes } from 'node:crypto';
import path from 'node:path';
import assert from 'node:assert/strict';
import { root } from '../mcp/config.mjs';
import { startDaemon } from '../mcp/daemon.mjs';

// Uses a disposable Blockbench profile and creates new test tabs only.
const executable =
  process.env.BLOCKBENCH_EXE ||
  path.join(
    process.env.LOCALAPPDATA || '',
    'Programs/Blockbench/Blockbench.exe',
  );
const work = path.join(root, '.local/e2e', String(Date.now()));
await mkdir(work, { recursive: true });
const token = randomBytes(32).toString('base64url');
const daemon = await startDaemon({
  token,
  bridgePort: 0,
  outputs: path.join(work, 'outputs'),
});
const port = daemon.httpServer.address().port;
await build({
  entryPoints: [path.join(root, 'plugin-src/main.mjs')],
  outfile: path.join(work, 'shardbound_bridge.js'),
  bundle: true,
  format: 'iife',
  platform: 'browser',
  target: 'chrome120',
  define: {
    __PAIRING_TOKEN__: JSON.stringify(token),
    __BRIDGE_PORT__: String(port),
  },
  logLevel: 'warning',
});
let app,
  page,
  phase = 'loading';
const messages = [];
try {
  app = await electron.launch({
    executablePath: executable,
    args: ['--userData', path.join(work, 'profile'), '--no-sandbox'],
    timeout: 60000,
  });
  page = await app.firstWindow();
  await app.evaluate(({ BrowserWindow }) =>
    BrowserWindow.getAllWindows().forEach((w) => w.hide()),
  );
  page.on('pageerror', (e) =>
    messages.push({ phase, message: e.message, stack: e.stack }),
  );
  await page.waitForFunction(
    () =>
      window.Blockbench?.setup_successful &&
      window.Plugins &&
      window.Formats?.free &&
      window.Mesh,
    {},
    { timeout: 60000 },
  );
  await new Promise((r) => setTimeout(r, 750));
  const startupErrors = messages.length;
  console.log(
    'Blockbench loaded:',
    await page.evaluate(() => Blockbench.version),
  );
  await page.evaluate(
    async (paths) => {
      Dialog.open?.hide();
      for (const p of paths)
        await new Plugin().loadFromFile({ path: p }, false);
    },
    [
      path.resolve(root, '../MTools/mesh_tools.js'),
      path.join(work, 'shardbound_bridge.js'),
    ],
  );
  let status;
  for (let i = 0; i < 50; i++) {
    status = await daemon.call('status', {});
    if (status.connected !== false) break;
    await new Promise((r) => setTimeout(r, 200));
  }
  assert.notEqual(status.connected, false, 'Plugin failed to connect');
  console.log('Connected:', JSON.stringify(status));
  let project = await daemon.call('create_project', {
    name: 'Bridge disposable test',
  });
  const call = (name, args = {}) => {
    phase = name + (args.action ? ' ' + args.action : '');
    return daemon.call(name, {
      ...args,
      ...(![
        'status',
        'read_project',
        'capture_view',
        'mtools_list',
        'get_texture',
        'create_project',
      ].includes(name)
        ? { project_id: project.project_id }
        : {}),
    });
  };
  const vertices = {
    a: [-4, -4, -4],
    b: [4, -4, -4],
    c: [4, 4, -4],
    d: [-4, 4, -4],
    e: [-4, -4, 4],
    f: [4, -4, 4],
    g: [4, 4, 4],
    h: [-4, 4, 4],
  };
  const faces = {
    front: { vertices: ['e', 'f', 'g', 'h'] },
    back: { vertices: ['b', 'a', 'd', 'c'] },
    left: { vertices: ['a', 'e', 'h', 'd'] },
    right: { vertices: ['f', 'b', 'c', 'g'] },
    top: { vertices: ['d', 'h', 'g', 'c'] },
    bottom: { vertices: ['a', 'b', 'f', 'e'] },
  };
  let created = await call('create_elements', {
    elements: [{ type: 'mesh', name: 'Test mesh', vertices, faces }],
  });
  const meshId = created.created[0].id;
  assert.equal(
    (await call('read_project', { geometry: true })).elements[0].face_count,
    6,
  );
  await call('select_elements', {
    ids: [meshId],
    mode: 'face',
    all_components: true,
  });
  await call('mtools_run', { action: 'triangulate' });
  assert.equal((await call('read_project')).elements[0].face_count, 12);
  await call('undo_redo', { direction: 'undo' });
  assert.equal((await call('read_project')).elements[0].face_count, 6);
  await call('undo_redo', { direction: 'redo' });
  assert.equal((await call('read_project')).elements[0].face_count, 12);
  await call('undo_redo', { direction: 'undo' });
  console.log('PASS mesh creation, MTools triangulate, undo and redo');
  await call('edit_mesh', { id: meshId, vertices: { a: [-5, -4, -4] } });
  await call('update_elements', {
    updates: [
      {
        id: meshId,
        name: 'Edited mesh',
        origin: [1, 2, 3],
        rotation: [0, 10, 0],
      },
    ],
  });
  await assert.rejects(
    call('edit_mesh', { id: meshId, delete_vertices: ['a'] }),
  );
  assert.equal(
    (await call('read_project', { geometry: true })).elements[0].vertices.a[0],
    -5,
  );
  console.log('PASS mesh patch and rejected invalid topology');
  const state = await call('read_project');
  await assert.rejects(
    call('update_elements', {
      expected_revision: state.revision - 1,
      updates: [{ id: meshId, name: 'stale' }],
    }),
    /revision/,
  );
  await assert.rejects(
    daemon.call('delete_elements', { project_id: 'wrong-tab', ids: [meshId] }),
    /Active project changed/,
  );
  const group = await call('create_elements', {
    elements: [{ type: 'group', name: 'Group' }],
  });
  const cube = await call('create_elements', {
    elements: [
      {
        type: 'cube',
        name: 'Cube',
        from: [0, 0, 0],
        to: [2, 2, 2],
        parent: group.created[0].id,
      },
    ],
  });
  await assert.rejects(
    call('delete_elements', { ids: [group.created[0].id] }),
    /children/,
  );
  await call('delete_elements', { ids: [cube.created[0].id] });
  await call('delete_elements', { ids: [group.created[0].id] });
  await call('undo_redo', { direction: 'undo' });
  assert.ok(
    (await call('read_project')).elements.some(
      (e) => e.id === group.created[0].id,
    ),
  );
  console.log(
    'PASS groups, cubes, deletion guards, stale revision and wrong-tab protection',
  );
  for (const [action, mode, parameters] of [
    ['subdivide', 'object', {}],
    ['split_edges', 'object', { angle: 120 }],
    ['poke', 'face', { depth: 1 }],
    ['tris_to_quad', 'face', { max_angle: 30 }],
    ['laplacian_smooth', 'vertex', { influence: 20, iterations: 2 }],
    ['to_sphere', 'vertex', { influence: 50 }],
    ['array_elements', 'object', { count: 2, x: 2 }],
    ['expand_selection', 'vertex', {}],
    ['shrink_selection', 'vertex', {}],
    ['uv_turnaround_projection', 'face', { margin: 2 }],
  ]) {
    project = await call('create_project', { name: 'MTools ' + action });
    created = await call('create_elements', {
      elements: [{ type: 'mesh', name: 'Fixture', vertices, faces }],
    });
    await call('select_elements', {
      ids: [created.created[0].id],
      mode,
      all_components: true,
    });
    await call('mtools_run', { action, parameters });
    console.log('PASS MTools ' + action);
  }
  project = await call('create_project', { name: 'Bridge loops' });
  created = await call('create_elements', {
    elements: [
      {
        type: 'mesh',
        name: 'Two loops',
        vertices,
        faces: { front: faces.front, back: faces.back },
      },
    ],
  });
  await call('select_elements', {
    ids: [created.created[0].id],
    mode: 'face',
    all_components: true,
  });
  await call('mtools_run', {
    action: 'bridge_edge_loops',
    parameters: { num_cuts: 2 },
  });
  console.log('PASS MTools bridge_edge_loops');
  await call('undo_redo', { direction: 'undo' });
  await call('select_elements', {
    ids: [created.created[0].id],
    mode: 'face',
    all_components: true,
  });
  await call('mtools_run', {
    action: 'bridge_edge_loops',
    parameters: { num_cuts: 64, twist: 40 },
  });
  assert.ok((await call('read_project')).elements[0].vertex_count > 32 * 4);
  console.log('PASS expanded Bridge Edge Loops parameters');
  for (const action of ['polyhedron', 'torusknot']) {
    project = await call('create_project', { name: action });
    await call('mtools_run', { action });
    assert.ok((await call('read_project')).elements.length > 0);
    console.log('PASS MTools ' + action);
  }
  for (const [action, parameters] of [
    ['textmesh', { text: 'Test', size: 4 }],
    ['terrain_action', { width: 8, height: 8 }],
    [
      'xyzmathsurfacefunction',
      {
        x: 'u',
        y: '0',
        z: 'v',
        uDivs: 4,
        vDivs: 4,
        uWrap: false,
        vWrap: false,
        vClose: false,
      },
    ],
  ]) {
    project = await call('create_project', { name: action });
    await call('mtools_run', { action, parameters });
    const generated = await call('read_project', { geometry: true });
    assert.ok(
      generated.elements.some((e) => e.type === 'mesh' && e.face_count > 0),
      action,
    );
    await call('undo_redo', { direction: 'undo' });
    assert.equal(
      (await call('read_project')).elements.length,
      0,
      action + ' Undo',
    );
    await call('undo_redo', { direction: 'redo' });
    assert.ok(
      (await call('read_project')).elements.length > 0,
      action + ' Redo',
    );
    console.log('PASS MTools ' + action + ' with Undo/Redo');
  }
  for (const [action, parameters] of [
    ['scatter', { density: 1 }],
    ['boolean', { operation: 'subtraction', acknowledge_experimental: true }],
  ]) {
    project = await call('create_project', { name: action });
    const pair = await call('create_elements', {
      elements: [
        { type: 'mesh', name: 'A', vertices, faces },
        {
          type: 'mesh',
          name: 'B',
          vertices: Object.fromEntries(
            Object.entries(vertices).map(([k, v]) => [
              k,
              [v[0] + 2, v[1] + 2, v[2] + 2],
            ]),
          ),
          faces,
        },
      ],
    });
    await call('select_elements', {
      ids: pair.created.map((e) => e.id),
      mode: 'object',
    });
    await call('mtools_run', { action, parameters });
    assert.ok((await call('read_project')).elements.length > 2, action);
    await call('undo_redo', { direction: 'undo' });
    const afterUndo = await call('read_project');
    assert.equal(afterUndo.elements.length, 2, action + ' Undo');
    await call('undo_redo', { direction: 'redo' });
    assert.ok(
      (await call('read_project')).elements.length > 2,
      action + ' Redo',
    );
    console.log('PASS MTools ' + action + ' with Undo/Redo');
  }
  const texture = await call('put_texture', {
    name: 'Test texture',
    width: 16,
    height: 16,
    color: '#cc8844ff',
  });
  await new Promise((r) => setTimeout(r, 300));
  const oldTexture = (await call('get_texture', { id: texture.texture_id }))
    .image;
  assert.match(oldTexture, /^data:image\/png;base64,/);
  await call('put_texture', {
    id: texture.texture_id,
    name: 'Updated texture',
    width: 16,
    height: 16,
    color: '#112233ff',
  });
  await new Promise((r) => setTimeout(r, 100));
  assert.notEqual(
    (await call('get_texture', { id: texture.texture_id })).image,
    oldTexture,
  );
  await call('undo_redo', { direction: 'undo' });
  await new Promise((r) => setTimeout(r, 100));
  assert.equal(
    (await call('get_texture', { id: texture.texture_id })).image,
    oldTexture,
  );
  const texturedMesh = (
    await call('read_project', { geometry: true })
  ).elements.find((e) => e.type === 'mesh');
  await call('edit_mesh', {
    id: texturedMesh.id,
    faces: Object.fromEntries(
      Object.entries(texturedMesh.faces).map(([k, f]) => [
        k,
        { ...f, texture: texture.texture_id },
      ]),
    ),
  });
  await page.evaluate(() => {
    Project.export_options.gltf = {
      encoding: 'binary',
      armature: true,
      animations: true,
    };
  });
  for (const format of ['bbmodel', 'gltf', 'obj']) {
    const result = await call('export_project', { format, filename: 'test' });
    assert.ok(result.content.length > 50);
    if (format !== 'obj') {
      const data = JSON.parse(result.content);
      if (format === 'gltf') {
        assert.ok(data.buffers.every((b) => b.uri.startsWith('data:')));
        assert.ok(data.images.length > 0);
        assert.ok(
          data.images.every(
            (i) => i.uri?.startsWith('data:') || i.bufferView !== undefined,
          ),
        );
        assert.equal(data.animations, undefined);
      }
      if (format === 'bbmodel')
        assert.ok(data.textures[0].source.startsWith('data:'));
    } else assert.doesNotMatch(result.content, /^mtllib /m);
    await writeFile(path.join(work, 'test.' + format), result.content);
    console.log('PASS export ' + format);
  }
  const glb = await call('export_project', { format: 'glb' });
  assert.equal(glb.binary, true);
  const glbBytes = Buffer.from(glb.content_base64, 'base64');
  assert.equal(glbBytes.toString('ascii', 0, 4), 'glTF');
  assert.equal(glbBytes.readUInt32LE(8), glbBytes.length);
  await writeFile(path.join(work, 'test.glb'), glbBytes);
  console.log('PASS export glb');
  const saved = await call('save_project', { path: 'assets/test.bbmodel' });
  const originalProject = project.project_id;
  project = await daemon.call('__open_project_data', {
    path: path.join(work, 'roundtrip.bbmodel'),
    content: saved.content,
  });
  assert.notEqual(project.project_id, originalProject);
  const roundtrip = await call('read_project', { geometry: true });
  assert.ok(roundtrip.elements.some((e) => e.type === 'mesh'));
  assert.ok(roundtrip.textures.length > 0);
  console.log('PASS save/open project roundtrip');
  const image = await call('capture_view', { width: 640, height: 480 });
  await writeFile(
    path.join(work, 'preview.png'),
    Buffer.from(image.image.split(',')[1], 'base64'),
  );
  console.log('PASS texture and screenshot');
  console.log('Artifacts: ' + work);
  console.log('Startup page errors (before plugins):', startupErrors);
  assert.equal(
    messages.length,
    startupErrors,
    'New Blockbench page errors during plugin tests: ' +
      JSON.stringify(messages.slice(startupErrors)),
  );
} finally {
  if (page && !page.isClosed())
    await page
      .evaluate(() => {
        for (const p of ModelProject.all) p.saved = true;
        window.onbeforeunload = null;
      })
      .catch(() => {});
  await app?.close();
  await daemon.close();
}
