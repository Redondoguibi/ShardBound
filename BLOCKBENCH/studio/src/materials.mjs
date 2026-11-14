// Native buffer groups use different material indexing for cuboids and polygon meshes.
export function faceTextures(g, node) {
  const faces = node instanceof g.Cube ? g.Canvas.face_order.map(key => node.faces[key]).filter(face => face.texture !== null)
    : Object.values(node.faces).filter(face => face.vertices.length >= 3);
  const textures = faces.map(face => face.getTexture());
  return node instanceof g.Mesh ? textures.filter((texture, index, all) => !index || texture !== all[index - 1]) : textures;
}
// Keep native painting, selection and lighting; apply the exported alpha/face contract to its shader.
export function createMaterialPreview(g, studio) {
  const originals = new Map();
  function restore() {
    for (const [material, old] of originals) {
      Object.assign(material, old); delete material.uniforms.STUDIO_OPAQUE; delete material.uniforms.STUDIO_CUTOFF; material.needsUpdate = true;
    }
    originals.clear();
  }
  function update() {
    if (!studio.isStudio()) { restore(); return; }
    for (const texture of g.Texture.all) {
      const material = texture.material, settings = studio.state().materials[texture.uuid] || { alphaMode: 'MASK', alphaCutoff: 0.5, doubleSided: true };
      if (!material?.isShaderMaterial) continue;
      if (!originals.has(material)) {
        originals.set(material, { fragmentShader: material.fragmentShader, side: material.side, transparent: material.transparent, blending: material.blending, depthWrite: material.depthWrite });
        const shader = material.fragmentShader.replace(/\bdiscard\s*;/g, ';'), end = shader.lastIndexOf('}');
        material.fragmentShader = 'uniform bool STUDIO_OPAQUE;\nuniform float STUDIO_CUTOFF;\n' + shader.slice(0, end) +
          '\nif (!STUDIO_OPAQUE && gl_FragColor.a < STUDIO_CUTOFF) discard;\ngl_FragColor.a = 1.0;\n' + shader.slice(end);
        material.uniforms.STUDIO_OPAQUE = { value: false }; material.uniforms.STUDIO_CUTOFF = { value: 0.5 }; material.needsUpdate = true;
      }
      const side = settings.doubleSided ? g.THREE.DoubleSide : g.THREE.FrontSide;
      if (material.side !== side) { material.side = side; material.needsUpdate = true; }
      material.uniforms.STUDIO_OPAQUE.value = settings.alphaMode === 'OPAQUE'; material.uniforms.STUDIO_CUTOFF.value = settings.alphaCutoff;
      material.transparent = false; material.depthWrite = true; material.blending = g.THREE.NoBlending;
    }
  }
  g.Blockbench.on('render_frame', update);
  update();
  return { update, dispose() { g.Blockbench.removeListener('render_frame', update); restore(); } };
}
