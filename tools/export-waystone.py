"""Rebuild waystone.glb from the author's static Blockbench mesh project.

Run from any directory with Python 3.11+. No third-party dependencies.
This deliberately supports the current model's flat meshes and unrotated
group, not every Blockbench format. Unsupported edits fail before writing.
Coordinates stay in authoring units, matching sword.glb and tree.glb.
"""
import base64
import json
import math
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parent.parent
MODELS = ROOT / 'assets/core/models'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def subtract(a, b):
    return [x - y for x, y in zip(a, b)]


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def normal(points):
    n = cross(subtract(points[1], points[0]), subtract(points[2], points[0]))
    length = math.sqrt(sum(x*x for x in n))
    require(length > 1e-10, 'Degenerate face')
    return [x / length for x in n]


def export():
    source = json.loads((MODELS / 'waystone.bbmodel').read_text(encoding='utf-8'))
    require(source['meta']['model_format'] == 'free', 'Expected a free Blockbench model')
    require(not source.get('animations'), 'Animated exports are not supported')
    require(len(source['groups']) == 1 and len(source['outliner']) == 1,
            'Expected one root group')
    group = source['groups'][0]
    outline = source['outliner'][0]
    require(outline['uuid'] == group['uuid'], 'Unexpected root group')
    require(group.get('export', True) and group.get('visibility', True), 'Group is hidden')
    require(group['rotation'] == [0, 0, 0], 'Rotated groups are not supported')
    elements = {e['uuid']: e for e in source['elements']}
    require(len(elements) == len(source['elements']), 'Duplicate element IDs')
    require(all(isinstance(child, str) for child in outline['children']), 'Nested groups unsupported')
    require(len(outline['children']) == len(elements)
            and set(outline['children']) == set(elements), 'Orphan or duplicate meshes')
    require(len(source['textures']) == 1, 'Expected one texture')
    texture = source['textures'][0]
    require(texture.get('render_mode', 'default') == 'default', 'Unsupported texture mode')
    prefix, encoded = texture['source'].split(',', 1)
    require(prefix == 'data:image/png;base64', 'Expected embedded PNG')
    png = base64.b64decode(encoded, validate=True)
    require(png == (MODELS / 'waystone.png').read_bytes(),
            'Embedded and external textures differ; save matching sources first')
    require(png[:8] == b'\x89PNG\r\n\x1a\n', 'Invalid PNG signature')
    width, height = struct.unpack_from('>II', png, 16)
    require(width == texture['width'] and height == texture['height'], 'Texture size mismatch')
    uv_width, uv_height = texture['uv_width'], texture['uv_height']
    require(uv_width > 0 and uv_height > 0, 'Invalid UV resolution')

    payload = bytearray()
    views, accessors, meshes = [], [], []
    nodes = [{'name': group['name'], 'translation': group['origin'], 'children': []}]
    triangles = 0

    def view(data, target=None):
        payload.extend(b'\0' * (-len(payload) % 4))
        item = {'buffer': 0, 'byteOffset': len(payload), 'byteLength': len(data)}
        if target is not None:
            item['target'] = target
        views.append(item)
        payload.extend(data)
        return len(views) - 1

    def accessor(values, kind, components, bounds=False):
        is_index = kind == 'SCALAR'
        flat = values if is_index else [x for row in values for x in row]
        require(all(math.isfinite(x) for x in flat), 'Non-finite geometry')
        data = struct.pack('<' + ('I' if is_index else 'f') * len(flat), *flat)
        item = {'bufferView': view(data, 34963 if is_index else 34962),
                'componentType': 5125 if is_index else 5126, 'count': len(values), 'type': kind}
        if bounds:
            item['min'] = [min(p[i] for p in values) for i in range(components)]
            item['max'] = [max(p[i] for p in values) for i in range(components)]
        accessors.append(item)
        return len(accessors) - 1

    for uid in outline['children']:
        element = elements[uid]
        require(element['type'] == 'mesh' and element.get('shading') == 'flat',
                'Only flat-shaded meshes are supported')
        require(element.get('export', True) and element.get('visibility', True), 'Mesh is hidden')
        angles = element['rotation']
        require(sum(abs(a) > 1e-10 for a in angles) <= 1, 'Multi-axis rotation unsupported')
        axis = next((i for i, a in enumerate(angles) if abs(a) > 1e-10), 0)
        half_angle = math.radians(angles[axis]) / 2
        quaternion = [0., 0., 0., math.cos(half_angle)]
        quaternion[axis] = math.sin(half_angle)
        positions, normals, uvs, indices = [], [], [], []
        for face in element['faces'].values():
            keys = face['vertices']
            require(len(keys) in (3, 4) and len(set(keys)) == len(keys), 'Expected triangle/quad')
            require(face['texture'] == 0, 'Unexpected face texture')
            points = [element['vertices'][key] for key in keys]
            n = normal(points[:3])
            # Blockbench saves sorted face vertices. Reject a folded/reversed
            # second triangle rather than silently changing the author's faces.
            if len(keys) == 4:
                other = normal([points[0], points[2], points[3]])
                require(sum(a*b for a, b in zip(n, other)) > 0, 'Invalid quad winding')
            start = len(positions)
            positions.extend(points)
            normals.extend([n] * len(keys))
            uvs.extend([[face['uv'][key][0] / uv_width, face['uv'][key][1] / uv_height]
                        for key in keys])
            indices.extend([start, start+1, start+2])
            if len(keys) == 4:
                indices.extend([start, start+2, start+3])
        require(positions, 'Empty mesh')
        primitive = {'attributes': {'POSITION': accessor(positions, 'VEC3', 3, True),
                                    'NORMAL': accessor(normals, 'VEC3', 3),
                                    'TEXCOORD_0': accessor(uvs, 'VEC2', 2)},
                     'indices': accessor(indices, 'SCALAR', 1), 'material': 0, 'mode': 4}
        nodes[0]['children'].append(len(nodes))
        nodes.append({'name': element['name'], 'mesh': len(meshes),
                      'translation': subtract(element['origin'], group['origin']),
                      'rotation': quaternion})
        meshes.append({'name': element['name'], 'primitives': [primitive]})
        triangles += len(indices) // 3

    image_view = view(png)
    document = {
        'asset': {'version': '2.0', 'generator': 'ShardBound tools/export-waystone.py'},
        'scene': 0, 'scenes': [{'name': source['name'], 'nodes': [0]}],
        'nodes': nodes, 'meshes': meshes, 'accessors': accessors, 'bufferViews': views,
        'buffers': [{'byteLength': len(payload)}],
        'images': [{'name': texture['name'], 'mimeType': 'image/png', 'bufferView': image_view}],
        'samplers': [{'magFilter': 9728, 'minFilter': 9728}],
        'textures': [{'source': 0, 'sampler': 0}],
        'materials': [{'name': 'waystone', 'alphaMode': 'MASK', 'alphaCutoff': 0.05,
                       'doubleSided': True, 'pbrMetallicRoughness': {
                           'baseColorTexture': {'index': 0}, 'metallicFactor': 0, 'roughnessFactor': 1}}],
    }
    metadata = json.dumps(document, separators=(',', ':'), allow_nan=False).encode('utf-8')
    metadata += b' ' * (-len(metadata) % 4)
    payload.extend(b'\0' * (-len(payload) % 4))
    result = struct.pack('<III', 0x46546C67, 2, 28 + len(metadata) + len(payload))
    result += struct.pack('<II', len(metadata), 0x4E4F534A) + metadata
    result += struct.pack('<II', len(payload), 0x004E4942) + payload
    target = MODELS / 'waystone.glb'
    target.write_bytes(result)
    print(f'{target.name}: {len(meshes)} meshes, {triangles} triangles, {width}x{height} embedded PNG')


if __name__ == '__main__':
    export()
