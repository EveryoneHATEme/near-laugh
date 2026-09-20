"""Fixture extension and confinement, without opening an editor."""

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from editor_ui_fixtures import fixture_path, load_fixtures
from editor_ui_protocol import ProtocolError


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "scripts").mkdir()
        self.manifest = self.root / "scripts/editor_ui_fixtures.json"

    def write(self, fixtures, **extra):
        self.manifest.write_text(json.dumps({"version": 1, "fixtures": fixtures, **extra}), encoding="utf-8")

    def test_new_fixture_needs_only_a_manifest_entry(self):
        self.write({"new-feature": "new_feature.level.json"})
        self.assertEqual(load_fixtures(self.root), {"new-feature": "new_feature.level.json"})
        resources = self.root / "resources"
        (resources / "levels").mkdir(parents=True)
        level = resources / "levels/new_feature.level.json"
        level.write_text("{}", encoding="utf-8")
        self.assertEqual(fixture_path(resources, load_fixtures(self.root)["new-feature"]), level)

    def test_rejects_paths_executables_invalid_types_and_aliases(self):
        for value in ("../x.level.json", "C:/x.level.json", "\\\\host\\x.level.json",
                      "x.level.json:stream", "child/x.level.json", "x.exe", "NUL",
                      "con.level.json", "com1.level.json", "lpt9.level.json", 3, None):
            with self.subTest(value=value):
                self.write({"safe": value})
                with self.assertRaises(ProtocolError):
                    load_fixtures(self.root)
        for fixtures in ({}, {"../bad": "x.level.json"},
                         {"one": "x.level.json", "two": "x.level.json"}):
            self.write(fixtures)
            with self.assertRaises(ProtocolError):
                load_fixtures(self.root)
        self.write({"safe": "x.level.json"}, executable="x.exe")
        with self.assertRaises(ProtocolError):
            load_fixtures(self.root)

    def test_rejects_duplicate_keys_size_and_version(self):
        for content in ('{"version":1,"version":1,"fixtures":{}}',
                        '{"version":true,"fixtures":{"x":"x.level.json"}}',
                        " " * 16385, "[]", '{"version":1,'):
            self.manifest.write_text(content, encoding="utf-8")
            with self.assertRaises(ProtocolError):
                load_fixtures(self.root)


if __name__ == "__main__":
    unittest.main()
