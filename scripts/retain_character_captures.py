"""Retain P07 Vulkan readbacks as lossless PNGs with verified stored RGB."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

from analyze_lighting_captures import read_ppm, read_png, write_png

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
manifest = {}
for path in sorted(args.source.glob("*.ppm")):
    width, height, pixels = read_ppm(path)
    target = args.output / (path.stem + ".png")
    write_png(target, width, height, pixels)
    if read_png(target) != (width, height, pixels):
        raise ValueError(f"Lossless RGB comparison failed: {path}")
    manifest[target.name] = {"width": width, "height": height,
                             "rgb_sha256": hashlib.sha256(pixels).hexdigest()}
if not manifest:
    raise ValueError("No P07 PPM readbacks found")
shutil.copyfile(args.source / "readbacks.txt", args.output / "readbacks.txt")
(args.output / "captures.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(f"Retained {len(manifest)} verified lossless readbacks")
