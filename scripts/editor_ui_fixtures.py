"""Reviewed fixture IDs only; never caller-selected paths or executables."""

from pathlib import Path
import json
import re
import stat

import editor_ui_protocol as protocol

ROOT = Path(__file__).resolve().parents[1]


def reject_links(path, root):
    """Check every existing component below the trusted resolved package root."""
    root = Path(root).resolve(strict=True)
    path = Path(path).absolute()
    try:
        relative = path.relative_to(root)
    except ValueError as error:
        raise protocol.ProtocolError("environment_unavailable", "Fixture escapes trusted root") from error
    current = root
    for part in relative.parts:
        current = current / part
        info = current.lstat()
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & 0x400:
            raise protocol.ProtocolError("environment_unavailable", "Fixture path contains a link/reparse point")
    return path


def load_fixtures(root=ROOT):
    root = Path(root).resolve(strict=True)
    path = reject_links(root / "scripts/editor_ui_fixtures.json", root)
    with path.open("rb") as source:
        data = source.read(16385)
    if len(data) > 16384:
        raise protocol.ProtocolError("limit_exceeded", "Fixture manifest exceeds 16 KiB")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate fixture manifest key")
            result[key] = value
        return result

    try:
        manifest = json.loads(data.decode("utf-8"), object_pairs_hook=unique)
        if type(manifest) is not dict:
            raise ValueError("Expected manifest object")
    except (ValueError, UnicodeError, RecursionError) as error:
        raise protocol.ProtocolError("environment_unavailable", "Invalid fixture manifest JSON") from error
    fixtures = manifest.get("fixtures")
    valid = (set(manifest) == {"version", "fixtures"} and type(manifest["version"]) is int
             and manifest["version"] == 1 and type(fixtures) is dict and 1 <= len(fixtures) <= 64)
    if valid:
        valid = all(re.fullmatch(r"[a-z0-9][a-z0-9-]{0,63}", name)
                    and type(filename) is str
                    and re.fullmatch(r"[a-z0-9][a-z0-9_-]{0,95}\.level\.json", filename)
                    and not re.fullmatch(r"(?:con|prn|aux|nul|com[1-9]|lpt[1-9])", filename.split(".")[0])
                    for name, filename in fixtures.items())
        valid = valid and len(set(fixtures.values())) == len(fixtures)
    if not valid:
        raise protocol.ProtocolError("environment_unavailable", "Invalid fixture IDs or level filenames")
    return fixtures


def fixture_path(resources, filename):
    # filename must come from load_fixtures, never from a tool argument.
    resources = Path(resources).absolute()
    return reject_links(resources / "levels" / filename, resources.parent.resolve(strict=True))
