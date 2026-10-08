"""Rebuild original, seamless pixel materials and embedded-texture GLB primitives."""
from pathlib import Path
import copy
import io
import json
import random
import struct
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "assets/core/models/solid.glb").read_bytes()
json_size = struct.unpack_from("<I", source, 12)[0]
base = json.loads(source[20:20 + json_size])
binary = source[28 + json_size:]
textures = ROOT / "assets/core/textures/woodland"
textures.mkdir(parents=True, exist_ok=True)

for name in ("grass", "earth", "plaster", "roof", "stone"):
    rng = random.Random(name)
    image = Image.new("RGBA", (32, 32))
    for y in range(32):
        for x in range(32):
            value = rng.choice((215, 218, 221, 224, 227))
            if name == "grass":
                value += 10 if (x * 7 + y * 11) % 47 < 4 else 0
                value -= 15 if (x * 5 + y * 3) % 59 < 2 else 0
            elif name == "earth":
                value -= 25 if (x * 7 + y * 17) % 83 < 4 else 0
            elif name == "plaster":
                value = rng.choice((240, 242, 244, 246))
            elif name == "roof":
                value = 190 if y % 8 == 7 or (x + (y // 8 % 2) * 8) % 16 == 15 else rng.choice((222, 228, 234))
            elif name == "stone":
                value = 165 if y % 16 == 15 or (x + (y // 16 % 2) * 16) % 32 == 31 else rng.choice((213, 218, 223))
            image.putpixel((x, y), (value, value, value, 255))
    image.save(textures / f"{name}.png")
    png = io.BytesIO()
    image.save(png, format="PNG")
    model = copy.deepcopy(base)
    blob = binary + png.getvalue()
    model["asset"]["generator"] = "ShardBound original pixel material generator"
    model["buffers"][0]["byteLength"] = len(blob)
    model["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(png.getvalue())})
    model["images"] = [{"bufferView": len(model["bufferViews"]) - 1, "mimeType": "image/png"}]
    model["samplers"] = [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}]
    model["textures"] = [{"sampler": 0, "source": 0}]
    model["materials"][0]["name"] = name
    model["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
    header = json.dumps(model, separators=(",", ":")).encode()
    header += b" " * (-len(header) % 4)
    blob += b"\0" * (-len(blob) % 4)
    glb = struct.pack("<III", 0x46546C67, 2, 28 + len(header) + len(blob))
    glb += struct.pack("<II", len(header), 0x4E4F534A) + header
    glb += struct.pack("<II", len(blob), 0x004E4942) + blob
    (ROOT / f"assets/core/models/{name}_block.glb").write_bytes(glb)
