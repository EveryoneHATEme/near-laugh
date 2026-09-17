"""Deterministic v9-to-v10 migration for current packaged level inputs."""

import json
from pathlib import Path


def migrate_household(level):
    if level["version"] == 10:
        return level
    if level["version"] != 9:
        raise ValueError("Household migration expects v9 or v10")
    migrated = dict(level)
    migrated["version"] = 10
    migrated["household"] = dict(boxes=[], documents=[], radios=[])
    return migrated


if __name__ == "__main__":
    root = Path(__file__).resolve().parents[1]
    for path in sorted((root / "resources/levels").glob("*.level.json")):
        original = json.loads(path.read_text(encoding="utf-8"))
        migrated = migrate_household(original)
        # Migration adds only the empty authored collections and format marker.
        if original["version"] == 9:
            normalized = dict(migrated)
            normalized.pop("household")
            normalized["version"] = 9
            assert normalized == original, path
        path.write_text(json.dumps(migrated, indent=2, ensure_ascii=False) + "\n",
                        encoding="utf-8", newline="\n")
        print(f"{path.name}: v{original['version']} -> v10; authored data retained")
