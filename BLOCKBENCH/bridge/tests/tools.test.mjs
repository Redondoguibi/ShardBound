import test from 'node:test';
import assert from 'node:assert/strict';
import { tools, validateMesh } from '../shared/tools.mjs';

test('core MCP tool schemas accept expected minimal inputs', () => {
  assert.deepEqual(tools.status.schema.parse({}), {});
  assert.deepEqual(tools.read_project.schema.parse({}), { geometry: false, offset: 0, limit: 50 });
  assert.equal(tools.create_project.schema.parse({ name: 'Test' }).texture_width, 64);
  assert.equal(tools.export_project.schema.parse({ project_id: 'p' }).format, 'bbmodel');
});

test('mesh validation accepts a triangle and rejects dangling vertices', () => {
  validateMesh({ vertices: { a: [0, 0, 0], b: [1, 0, 0], c: [0, 1, 0] }, faces: { f: { vertices: ['a', 'b', 'c'] } } });
  assert.throws(() => validateMesh({ vertices: { a: [0, 0, 0], b: [1, 0, 0] }, faces: { f: { vertices: ['a', 'b', 'missing'] } } }), /Invalid vertices/);
});

test('mtools_run rejects unknown actions', () => {
  const base = { project_id: 'project', action: 'not_real', parameters: {} };
  assert.equal(tools.mtools_run.schema.safeParse(base).success, false);
});
