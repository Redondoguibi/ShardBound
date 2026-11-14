"""Original shared arrow visual, in metres, pointing toward local -Z.

Uses the project's cuboid primitive; no downloaded assets. Gameplay supplies
velocity, collision and damage independently of this visual.
"""
import json
import struct
from pathlib import Path

root = Path(__file__).resolve().parent.parent
source = (root / 'assets/core/models/cube.glb').read_bytes()
length = struct.unpack_from('<I', source, 12)[0]
doc = json.loads(source[20:20 + length])
binary = source[28 + length:]
doc['materials'] = [dict(pbrMetallicRoughness=dict(baseColorFactor=c, metallicFactor=0, roughnessFactor=1))
                    for c in ([.34, .18, .07, 1], [.6, .65, .7, 1], [.9, .85, .65, 1])]
primitive = doc['meshes'][0]['primitives'][0]
doc['meshes'] = [dict(primitives=[dict(primitive, material=i)]) for i in range(3)]
doc['nodes'] = [
    dict(name='Shaft', mesh=0, translation=[0, 0, 0], scale=[.012, .012, .3]),
    dict(name='Tip', mesh=1, translation=[0, 0, -.34], scale=[.027, .027, .05]),
    dict(name='FeatherX', mesh=2, translation=[0, 0, .22], scale=[.07, .004, .065]),
    dict(name='FeatherY', mesh=2, translation=[0, 0, .22], scale=[.004, .07, .065]),
]
doc['scenes'] = [dict(nodes=list(range(4)))]
doc['scene'] = 0
metadata = json.dumps(doc, separators=(',', ':')).encode()
metadata += b' ' * (-len(metadata) % 4)
result = struct.pack('<III', 0x46546C67, 2, 28 + len(metadata) + len(binary))
result += struct.pack('<II', len(metadata), 0x4E4F534A) + metadata
result += struct.pack('<II', len(binary), 0x004E4942) + binary
(root / 'assets/core/models/arrow.glb').write_bytes(result)
