import test from 'node:test';
import assert from 'node:assert/strict';
import { tools, validateMesh } from '../shared/tools.mjs';

test('core MCP tool schemas accept expected minimal inputs', () => {
  assert.deepEqual(tools.status.schema.parse({}), {});
  assert.deepEqual(tools.read_project.schema.parse({}), { geometry: false, offset: 0, limit: 50 });
  assert.equal(tools.create_project.schema.parse({ name: 'Test' }).texture_width, 64);
  assert.equal(tools.export_project.schema.parse({ project_id: 'p' }).format, 'bbmodel');
});

test('cube schemas support per-face textures, UVs and rotation', () => {
  const parsed = tools.create_elements.schema.parse({
    project_id: 'project',
    elements: [{
      type: 'cube', name: 'stone', from: [0, 0, 0], to: [16, 16, 16],
      faces: { north: { uv: [0, 0, 16, 16], texture: 'texture-uuid', rotation: 90, enabled: true } },
    }],
  });
  assert.equal(parsed.elements[0].faces.north.rotation, 90);
  assert.equal(parsed.elements[0].faces.north.texture, 'texture-uuid');
});

test('asset file tools reject traversal and absolute paths', () => {
  assert.equal(tools.open_project_file.schema.safeParse({ path: '../secret.bbmodel' }).success, false);
  assert.equal(tools.open_project_file.schema.safeParse({ path: 'C:/secret.bbmodel' }).success, false);
  assert.equal(tools.open_project_file.schema.safeParse({ path: 'assets/core/models/source.bbmodel' }).success, true);
});

test('GLB is a supported embedded-texture export target', () => {
  const parsed = tools.export_project.schema.parse({ project_id: 'project', format: 'glb', path: 'assets/core/models/model.glb' });
  assert.equal(parsed.format, 'glb');
});

test('mesh validation accepts a triangle and rejects dangling vertices', () => {
  validateMesh({ vertices: { a: [0, 0, 0], b: [1, 0, 0], c: [0, 1, 0] }, faces: { f: { vertices: ['a', 'b', 'c'] } } });
  assert.throws(() => validateMesh({ vertices: { a: [0, 0, 0], b: [1, 0, 0] }, faces: { f: { vertices: ['a', 'b', 'missing'] } } }), /Invalid vertices/);
});

test('mtools_run rejects unknown actions', () => {
  const base = { project_id: 'project', action: 'not_real', parameters: {} };
  assert.equal(tools.mtools_run.schema.safeParse(base).success, false);
});
