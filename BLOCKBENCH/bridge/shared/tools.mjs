import { z } from 'zod';

const number = z.number().finite().min(-1e6).max(1e6);
export const vec3 = z.tuple([number, number, number]);
const vec2 = z.tuple([number, number]);
const key = z
  .string()
  .min(1)
  .max(100)
  .regex(/^[a-zA-Z0-9_-]+$/)
  .refine((v) => !['__proto__', 'constructor', 'prototype'].includes(v));
const id = z.string().min(1).max(100);
const name = z.string().min(1).max(160);
const face = z
  .object({
    vertices: z.array(key).min(3).max(4),
    uv: z.record(key, vec2).optional(),
    texture: id.nullable().optional(),
  })
  .strict();
const mesh = { vertices: z.record(key, vec3), faces: z.record(key, face) };
const common = {
  name,
  parent: id.optional(),
  origin: vec3.optional(),
  rotation: vec3.optional(),
};
const element = z.discriminatedUnion('type', [
  z.object({ type: z.literal('group'), ...common }).strict(),
  z
    .object({ type: z.literal('cube'), ...common, from: vec3, to: vec3 })
    .strict(),
  z.object({ type: z.literal('mesh'), ...common, ...mesh }).strict(),
]);
const guard = {
  project_id: id.describe(
    'UUID from read_project/status. Prevents editing another open tab.',
  ),
  expected_revision: z.number().int().nonnegative().optional(),
};
const bounded = (min, max, value) =>
  z.number().min(min).max(max).default(value);
export const mtools = {
  bridge_edge_loops: {
    mode: ['edge', 'face'],
    amend: true,
    schema: z
      .object({
        num_cuts: bounded(1, 32, 1).refine(Number.isInteger),
        twist: bounded(-32, 32, 0).refine(Number.isInteger),
        blend_influence: bounded(-200, 200, 100),
        blend_path: z.boolean().default(true),
        cut_holes: z.boolean().default(true),
      })
      .strict(),
  },
  uv_turnaround_projection: {
    mode: 'face',
    amend: true,
    schema: z
      .object({ margin: bounded(0, 100, 0), split: z.boolean().default(true) })
      .strict(),
  },
  subdivide: { mode: 'object', schema: z.object({}).strict() },
  split_edges: {
    mode: 'object',
    amend: true,
    schema: z.object({ angle: bounded(0, 180, 180) }).strict(),
  },
  triangulate: { mode: 'face', schema: z.object({}).strict() },
  poke: {
    mode: 'face',
    amend: true,
    schema: z.object({ depth: bounded(-1000, 1000, 0) }).strict(),
  },
  tris_to_quad: {
    mode: 'face',
    amend: true,
    schema: z
      .object({
        max_angle: bounded(0, 180, 45),
        ignore_disjoint_uvs: z.boolean().default(true),
      })
      .strict(),
  },
  laplacian_smooth: {
    mode: 'vertex',
    amend: true,
    schema: z
      .object({
        influence: bounded(0, 100, 100),
        iterations: bounded(1, 10, 1).refine(Number.isInteger),
      })
      .strict(),
  },
  to_sphere: {
    mode: 'vertex',
    amend: true,
    schema: z.object({ influence: bounded(0, 100, 100) }).strict(),
  },
  expand_selection: {
    mode: 'vertex',
    schema: z.object({}).strict(),
    selection: true,
  },
  shrink_selection: {
    mode: 'vertex',
    schema: z.object({}).strict(),
    selection: true,
  },
  array_elements: {
    mode: 'object',
    amend: true,
    schema: z
      .object({
        x: bounded(-100, 100, 1),
        y: bounded(-100, 100, 0),
        z: bounded(-100, 100, 0),
        count: bounded(1, 50, 1).refine(Number.isInteger),
      })
      .strict(),
  },
  polyhedron: {
    generator: true,
    dialog: true,
    schema: z
      .object({
        select: z
          .enum(['Icosahedron', 'Dodecahedron', 'Octahedron', 'Tetrahedron'])
          .default('Icosahedron'),
        radius: bounded(0.01, 1000, 8),
        detail: bounded(1, 5, 2).refine(Number.isInteger),
      })
      .strict(),
  },
  torusknot: {
    generator: true,
    dialog: true,
    schema: z
      .object({
        r: bounded(0.01, 1000, 8),
        t: bounded(0.01, 1000, 3.4),
        ts: bounded(3, 256, 25).refine(Number.isInteger),
        rs: bounded(3, 64, 5).refine(Number.isInteger),
        p: bounded(1, 32, 2).refine(Number.isInteger),
        q: bounded(1, 32, 3).refine(Number.isInteger),
      })
      .strict(),
  },
};
export const tools = {
  status: {
    read: true,
    description:
      'Check the live Blockbench connection, active project, revision and MTools version.',
    schema: z.object({}).strict(),
  },
  read_project: {
    read: true,
    description:
      'Inspect the active Generic Model. Returns UUIDs, hierarchy, selection, textures and revision; request geometry for mesh vertices/faces/UVs. Paginated.',
    schema: z
      .object({
        geometry: z.boolean().default(false),
        offset: z.number().int().min(0).default(0),
        limit: z.number().int().min(1).max(200).default(50),
      })
      .strict(),
  },
  create_project: {
    description: 'Create a new Generic Model tab, keeping existing tabs open.',
    schema: z
      .object({
        name,
        texture_width: z.number().int().min(1).max(4096).default(64),
        texture_height: z.number().int().min(1).max(4096).default(64),
      })
      .strict(),
  },
  create_elements: {
    description:
      'Create groups, cubes or meshes in one undo step. Parent is an existing group UUID. Mesh vertices/faces use caller-chosen keys; faces are triangles or quads.',
    schema: z
      .object({ ...guard, elements: z.array(element).min(1).max(100) })
      .strict(),
  },
  update_elements: {
    description:
      'Update name, pivot/origin, rotation, visibility or cube bounds. Mesh vertex coordinates stay local to the origin. One undo step.',
    schema: z
      .object({
        ...guard,
        updates: z
          .array(
            z
              .object({
                id,
                name: name.optional(),
                origin: vec3.optional(),
                rotation: vec3.optional(),
                visibility: z.boolean().optional(),
                from: vec3.optional(),
                to: vec3.optional(),
              })
              .strict(),
          )
          .min(1)
          .max(100),
      })
      .strict(),
  },
  edit_mesh: {
    description:
      'Patch mesh vertices and faces (including UVs and texture UUID). Faces referencing deleted vertices must also be removed or replaced. Atomic, undoable.',
    schema: z
      .object({
        ...guard,
        id,
        vertices: mesh.vertices.optional(),
        faces: mesh.faces.optional(),
        delete_vertices: z.array(key).max(10000).default([]),
        delete_faces: z.array(key).max(10000).default([]),
      })
      .strict(),
  },
  delete_elements: {
    description:
      'Delete specified elements or empty groups in one undo step. Nonempty groups are rejected.',
    schema: z.object({ ...guard, ids: z.array(id).min(1).max(100) }).strict(),
  },
  select_elements: {
    description:
      'Set exact element/component selection before running MTools. all_components selects every component of selected meshes. IDs must be mesh/cube UUIDs.',
    schema: z
      .object({
        ...guard,
        ids: z.array(id).max(100),
        mode: z.enum(['object', 'vertex', 'edge', 'face']).default('object'),
        all_components: z.boolean().default(false),
        components: z
          .array(
            z
              .object({
                id,
                vertices: z.array(key).default([]),
                faces: z.array(key).default([]),
                edges: z.array(z.tuple([key, key])).default([]),
              })
              .strict(),
          )
          .max(100)
          .default([]),
      })
      .strict(),
  },
  capture_view: {
    read: true,
    description: 'Return a PNG image of the current Blockbench viewport.',
    schema: z
      .object({
        width: z.number().int().min(64).max(1600).default(900),
        height: z.number().int().min(64).max(1600).default(700),
      })
      .strict(),
  },
  undo_redo: {
    description:
      'Undo or redo one operation in the active project. Can include manual user edits; inspect history in status first.',
    schema: z
      .object({ ...guard, direction: z.enum(['undo', 'redo']) })
      .strict(),
  },
  mtools_list: {
    read: true,
    description:
      'Discover installed MTools actions, conditions and supported parameter schemas. Only actions marked automated are callable by mtools_run.',
    schema: z.object({}).strict(),
  },
  mtools_run: {
    description:
      'Run an adapted MTools 2.1.0 action on the current selection. Read mtools_list for parameters and select_elements first. Each modeling operation uses native Undo. Does not execute arbitrary JavaScript.',
    schema: z
      .object({
        ...guard,
        action: z.enum(Object.keys(mtools)),
        parameters: z
          .record(
            z.string(),
            z.union([number, z.string().max(100), z.boolean()]),
          )
          .default({}),
      })
      .strict(),
  },
  put_texture: {
    description:
      'Create or replace a texture with a PNG data URL, or a solid color. Returns texture UUID for mesh faces. Replacement preserves UUID and supports Undo.',
    schema: z
      .object({
        ...guard,
        id: id.optional(),
        name,
        width: z.number().int().min(1).max(2048).default(64),
        height: z.number().int().min(1).max(2048).default(64),
        color: z
          .string()
          .regex(/^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/)
          .default('#ffffffff'),
        png: z
          .string()
          .max(4e6)
          .regex(/^data:image\/png;base64,[A-Za-z0-9+/=]+$/)
          .optional(),
      })
      .strict(),
  },
  get_texture: {
    read: true,
    description: 'Read a project texture as a PNG image.',
    schema: z.object({ id }).strict(),
  },
  export_project: {
    read: false,
    destructive: false,
    description:
      'Export the current project to the bridge outputs directory. bbmodel preserves the editable project; obj exports geometry; gltf embeds resources. Creates a NEW file on every call and never overwrites. Returns an absolute local path.',
    schema: z
      .object({
        project_id: id,
        format: z.enum(['bbmodel', 'obj', 'gltf']).default('bbmodel'),
        filename: z
          .string()
          .regex(/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,79}$/)
          .default('model'),
      })
      .strict(),
  },
};

export function validateMesh(data) {
  if (
    Object.keys(data.vertices).length > 20000 ||
    Object.keys(data.faces).length > 20000
  )
    throw new Error('Mesh limit: 20000 vertices/faces per operation.');
  for (const [k, f] of Object.entries(data.faces)) {
    if (
      new Set(f.vertices).size !== f.vertices.length ||
      f.vertices.some((v) => !Object.hasOwn(data.vertices, v))
    )
      throw new Error(`Invalid vertices in face ${k}`);
    if (f.uv && Object.keys(f.uv).some((v) => !f.vertices.includes(v)))
      throw new Error(`UV key outside face ${k}`);
  }
}
