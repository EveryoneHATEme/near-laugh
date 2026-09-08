"""Explicit deterministic v8-to-v9 migration for packaged level preparation."""
import json
from pathlib import Path


def migrate_characters(level):
    if level["version"] == 9:
        return level
    if level["version"] != 8:
        raise ValueError("Character migration expects v8 or v9")
    level = dict(level)
    level["version"] = 9
    level["characters"] = dict(actors=[], marks=[], routes=[])
    return level


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    for path in sorted((root / "resources/levels").glob("*.level.json")):
        level = migrate_characters(json.loads(path.read_text(encoding="utf-8")))
        path.write_text(json.dumps(level, indent=2) + "\n", encoding="utf-8", newline="\n")
