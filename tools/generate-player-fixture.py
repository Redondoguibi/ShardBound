"""Build an original static six-part player fixture from our cuboid mesh.

Classic 8x8x8 head, 8x12x4 torso, 4x12x4 limbs; not the final rig/skin format.
"""
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parent.parent
source = (root / 'assets/core/models/cube.glb').read_bytes()
length = struct.unpack_from('<I', source, 12)[0]
document = json.loads(source[20:20 + length])
binary = source[28 + length:]
parts = [
    ('Head', (0, .75, 0), (.25, .25, .25)),
    ('Body', (0, .125, 0), (.25, .375, .125)),
    ('LeftArm', (-.375, .125, 0), (.125, .375, .125)),
    ('RightArm', (.375, .125, 0), (.125, .375, .125)),
    ('LeftLeg', (-.125, -.625, 0), (.125, .375, .125)),
    ('RightLeg', (.125, -.625, 0), (.125, .375, .125)),
]
document['nodes'] = [dict(name=name, mesh=0, translation=p, scale=s) for name, p, s in parts]
document['scenes'] = [{'nodes': list(range(6))}]
metadata = json.dumps(document, separators=(',', ':')).encode()
metadata += b' ' * (-len(metadata) % 4)
result = struct.pack('<III', 0x46546C67, 2, 28 + len(metadata) + len(binary))
result += struct.pack('<II', len(metadata), 0x4E4F534A) + metadata
result += struct.pack('<II', len(binary), 0x004E4942) + binary
(root / 'examples/gameplay/sandbox/player.glb').write_bytes(result)
