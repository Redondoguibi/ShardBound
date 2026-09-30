"""Generate an original cuboid GLB fixture with an embedded checker texture."""
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parent.parent
vertices, normals, uvs, indices = [], [], [], []
def face(points, normal):
    offset = len(vertices)
    vertices.extend(points)
    normals.extend([normal] * 4)
    uvs.extend([(0, 1), (0, 0), (1, 0), (1, 1)])
    indices.extend(offset + i for i in [0, 1, 2, 0, 2, 3])
face([(-1,-1,-1),(-1,1,-1),(1,1,-1),(1,-1,-1)], (0,0,-1))
face([(1,-1,1),(1,1,1),(-1,1,1),(-1,-1,1)], (0,0,1))
face([(-1,-1,1),(-1,1,1),(-1,1,-1),(-1,-1,-1)], (-1,0,0))
face([(1,-1,-1),(1,1,-1),(1,1,1),(1,-1,1)], (1,0,0))
face([(-1,1,-1),(-1,1,1),(1,1,1),(1,1,-1)], (0,1,0))
face([(-1,-1,1),(-1,-1,-1),(1,-1,-1),(1,-1,1)], (0,-1,0))
payload, views = bytearray(), []
def add(data):
    while len(payload) % 4: payload.append(0)
    views.append({'buffer': 0, 'byteOffset': len(payload), 'byteLength': len(data)})
    payload.extend(data)
for values in (vertices, normals, uvs):
    add(b''.join(struct.pack('<' + 'f' * len(v), *v) for v in values))
add(struct.pack('<' + 'I' * len(indices), *indices))
add((root / 'assets/core/textures/checker.png').read_bytes())
document = {
    'asset': {'version': '2.0', 'generator': 'Game Runtime original fixture'},
    'buffers': [{'byteLength': len(payload)}], 'bufferViews': views,
    'accessors': [
        {'bufferView': 0, 'componentType': 5126, 'count': 24, 'type': 'VEC3', 'min': [-1,-1,-1], 'max': [1,1,1]},
        {'bufferView': 1, 'componentType': 5126, 'count': 24, 'type': 'VEC3'},
        {'bufferView': 2, 'componentType': 5126, 'count': 24, 'type': 'VEC2'},
        {'bufferView': 3, 'componentType': 5125, 'count': 36, 'type': 'SCALAR'}],
    'images': [{'bufferView': 4, 'mimeType': 'image/png', 'name': 'Body'}],
    'textures': [{'source': 0}],
    'materials': [{'name': 'Body', 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0}}],
    'meshes': [{'name': 'Body', 'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2}, 'indices': 3, 'material': 0}]}],
    'nodes': [{'mesh': 0, 'name': 'Body'}], 'scenes': [{'nodes': [0]}], 'scene': 0
}
metadata = json.dumps(document, separators=(',', ':')).encode()
metadata += b' ' * (-len(metadata) % 4)
payload += b'\0' * (-len(payload) % 4)
glb = struct.pack('<III', 0x46546C67, 2, 28 + len(metadata) + len(payload))
glb += struct.pack('<II', len(metadata), 0x4E4F534A) + metadata
glb += struct.pack('<II', len(payload), 0x004E4942) + payload
target = root / 'assets/core/models/cube.glb'
target.parent.mkdir(parents=True, exist_ok=True)
target.write_bytes(glb)
