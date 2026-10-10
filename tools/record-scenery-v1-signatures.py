"""Record exact legacy visual keys from the frozen v1 presentation test save.

Run presentation_tests first, then pass its user/presentation-saves/scenery_v1.json.
No persistent IDs, player state or physics bodies are included in the output.
"""
import argparse
import json
import struct
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("save", type=Path)
args = parser.parse_args()
data = json.loads(args.save.read_text(encoding="utf-8"))
keys = set()
for entity in data["entities"]:
    definition = entity.get("definition")
    if definition not in ("adventure:Block", "adventure:Pine"):
        continue
    if entity.get("name") != definition or "physics" in entity or "model" not in entity:
        continue
    values = sum(entity["transform"], []) + entity["tint"]
    key = "v" + entity["model"].encode("utf-8").hex()
    key += "".join("_" + struct.pack(">f", float(value)).hex() for value in values)
    keys.add(key)
root = Path(__file__).resolve().parent.parent
output = root / "content/adventure/scenery_v1_shapes.sl"
header = "? Frozen exact visual signatures from presentation v1; no entity IDs or physics bodies.\n@Globals\nlegacy_scenery_shapes = record()\n"
output.write_text(header + "\n".join("legacy_scenery_shapes." + key + " = true" for key in sorted(keys)) + "\n", encoding="utf-8", newline="\n")
print(f"Recorded {len(keys)} legacy visual signatures")
