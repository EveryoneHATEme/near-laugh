"""Regenerate original P07b neutral PCM effects and their provenance."""

import hashlib
import json
import math
from pathlib import Path
import random
import struct
import wave

ROOT = Path(__file__).resolve().parents[1]
RATE = 48000


def main():
    rng = random.Random(7002)
    files = []
    for name, duration, audible, frequency in (
        ("character-footstep", .12, .12, 95),
        ("character-interaction", 1., .20, 230),
    ):
        samples = []
        for i in range(round(duration * RATE)):
            t = i / RATE
            if t < audible:
                envelope = math.sin(math.pi * t / audible) ** 2 * math.exp(-t * 25)
                value = envelope * (.45 * math.sin(2 * math.pi * frequency * t)
                                    + .18 * rng.uniform(-1, 1))
            else:
                value = 0.
            samples.append(round(value * 32767))
        path = ROOT / "resources/audio" / (name + ".wav")
        with wave.open(str(path), "wb") as output:
            output.setparams((1, 2, RATE, 0, "NONE", "not compressed"))
            output.writeframes(struct.pack("<" + "h" * len(samples), *samples))
        files.append(path)
    caption = ROOT / "resources/captions/character-interaction.captions"
    caption.write_text("0\t1\tМанекен\tТихий щелчок.\n", encoding="utf-8", newline="\n")
    files.append(caption)
    manifest = {
        "generator": "scripts/prepare_scripted_character_audio.py",
        "source": "Original deterministic synthetic effects and neutral Russian text; no external recordings",
        "seed": 7002, "sample_rate": RATE, "profile": "PCM16 mono",
        "footstep_seconds": .12, "interaction_seconds": 1.,
        "interaction_audible_seconds": .20,
        "files": {str(p.relative_to(ROOT)).replace("\\", "/"):
                  hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
    }
    (ROOT / "resources/audio/scripted-characters-provenance.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
