"""Derive an untextured, tintable primitive from our original cube fixture."""
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parent.parent
source = (root / 'assets/core/models/cube.glb').read_bytes()
length = struct.unpack_from('<I', source, 12)[0]
document = json.loads(source[20:20 + length])
binary = source[28 + length:]
document['asset']['generator'] = 'ShardBound original solid primitive'
document['materials'] = [{'name': 'Solid', 'pbrMetallicRoughness': {
    'baseColorFactor': [1, 1, 1, 1], 'metallicFactor': 0}}]
# Drop the unused checker image and its buffer view/payload.
image_view = document['images'][0]['bufferView']
binary = binary[:document['bufferViews'][image_view]['byteOffset']]
document['bufferViews'] = document['bufferViews'][:image_view]
document.pop('images')
document.pop('textures')
document['buffers'][0]['byteLength'] = len(binary)
metadata = json.dumps(document, separators=(',', ':')).encode()
metadata += b' ' * (-len(metadata) % 4)
binary += b'\0' * (-len(binary) % 4)
result = struct.pack('<III', 0x46546C67, 2, 28 + len(metadata) + len(binary))
result += struct.pack('<II', len(metadata), 0x4E4F534A) + metadata
result += struct.pack('<II', len(binary), 0x004E4942) + binary
(root / 'assets/core/models/solid.glb').write_bytes(result)
