"""Explicit deterministic v10-to-v11 migration; preserve original v10 bytes."""

import argparse
import json
from pathlib import Path


def migrate_narrative(level):
    if not isinstance(level, dict) or type(level.get("version")) is not int:
        raise ValueError("Narrative migration requires an exact integer version marker")
    if level["version"] == 11:
        return level
    if level["version"] != 10:
        raise ValueError("Narrative migration expects v10 or v11")
    if "narrative" in level:
        raise ValueError("Version 10 must not contain narrative fields")
    migrated = dict(level)
    migrated["version"] = 11
    migrated["narrative"] = dict(facts=[], regions=[], events=[])
    return migrated


def convert_file(path, backup_directory):
    original_bytes = path.read_bytes()
    original = json.loads(original_bytes)
    migrated = migrate_narrative(original)
    if original["version"] == 11:
        return False  # Idempotent even for noncanonical v11 formatting.
    normalized = dict(migrated)
    normalized.pop("narrative")
    normalized["version"] = 10
    assert normalized == original
    backup_directory.mkdir(parents=True, exist_ok=True)
    backup = backup_directory / path.name
    if backup.resolve() == path.resolve():
        raise ValueError("The backup must be separate from the converted level")
    if backup.exists():
        if backup.read_bytes() != original_bytes:
            raise ValueError(f"Refusing to replace different original: {backup}")
    else:
        backup.write_bytes(original_bytes)
    path.write_text(json.dumps(migrated, indent=2, ensure_ascii=False) + "\n",
                    encoding="utf-8", newline="\n")
    return True


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", type=Path, nargs="*")
    parser.add_argument("--backup-directory", type=Path, required=True)
    args = parser.parse_args()
    paths = args.paths or sorted((root / "resources/levels").glob("*.level.json"))
    for path in paths:
        changed = convert_file(path, args.backup_directory)
        print(f"{path.name}: {'v10 -> v11; original retained' if changed else 'v11 unchanged'}")


if __name__ == "__main__":
    main()
