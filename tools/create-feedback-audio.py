"""Synthesize original short feedback sounds (no external recordings)."""
from pathlib import Path
import math
import random
import struct
import wave

root = Path(__file__).resolve().parent.parent / "assets/core/audio"
random.seed(927)
for name, duration in [("swing", .18), ("waystone", .8), ("pickup", .3)]:
    values = []
    low = 0
    for i in range(int(48000 * duration)):
        t = i / 48000
        u = t / duration
        envelope = math.sin(math.pi * u) ** 1.2
        if name == "swing":
            low = .82 * low + .18 * random.uniform(-1, 1)
            sample = low * envelope * .45
        else:
            frequency = 440 if name == "waystone" else 880
            sample = (math.sin(2 * math.pi * frequency * t)
                      + .45 * math.sin(2 * math.pi * frequency * 1.5 * t)
                      + .2 * math.sin(2 * math.pi * frequency * 2 * t)) * envelope * (1 - u) * .12
        values.append(struct.pack("<h", int(sample * 32767)))
    with wave.open(str(root / (name + ".wav")), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(48000)
        output.writeframes(b"".join(values))
