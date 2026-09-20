"""Doctor checks are read-only by default and never infer desktop readiness."""

import argparse
import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import AsyncMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import editor_ui_doctor as doctor
import editor_ui_protocol as protocol


def arguments(**values):
    return argparse.Namespace(launch_editor=False, environment=None,
                              fixture="apartment-stairs", disable_implicit_layers=False,
                              **values)


class DoctorTests(unittest.TestCase):
    def test_launch_requires_explicit_environment_before_any_checks(self):
        with patch.object(doctor, "diagnose") as diagnose, contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                doctor.main(["--launch-editor"])
        self.assertEqual(error.exception.code, 2)
        diagnose.assert_not_called()

    def test_environment_is_not_a_hidden_launch_switch(self):
        with patch.object(doctor, "diagnose") as diagnose, contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                doctor.main(["--environment", "test-profile"])
        self.assertEqual(error.exception.code, 2)
        diagnose.assert_not_called()

    def test_python_version_mismatch_is_explicit(self):
        with patch.object(doctor.sys, "version_info", (3, 13, 0)), self.assertRaisesRegex(ValueError, "CPython 3.12"):
            doctor.runtime_check()

    def test_all_pins_checked_and_mismatch_names_expected_observed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            (root / "scripts/requirements-editor-ui.txt").write_text("mcp==2.2.0\nanyio==4.15.1\n", encoding="utf-8")
            with patch.object(doctor.metadata, "version", side_effect=["2.2.0", "4.0.0"]) as version:
                with self.assertRaisesRegex(ValueError, "anyio: expected 4.15.1, observed 4.0.0"):
                    doctor.dependencies_check(root)
                self.assertEqual(version.call_count, 2)

    def test_missing_sdk_is_reported_without_importing_it(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            (root / "scripts/requirements-editor-ui.txt").write_text("mcp==2.2.0\n", encoding="utf-8")
            with patch.object(doctor.metadata, "version", side_effect=doctor.metadata.PackageNotFoundError):
                with self.assertRaisesRegex(ValueError, "mcp: expected 2.2.0, observed missing"):
                    doctor.dependencies_check(root)

    def test_missing_build_is_diagnostic_and_does_not_launch(self):
        with tempfile.TemporaryDirectory() as directory, patch("editor_ui_mcp.OwnedProcess") as process:
            with self.assertRaises((protocol.ProtocolError, OSError)):
                doctor.build_check("apartment-stairs", Path(directory))
            process.assert_not_called()

    def test_host_build_metadata_failure_is_preserved(self):
        with patch("editor_ui_mcp.SessionController._configuration", side_effect=
                   protocol.ProtocolError("environment_unavailable", "Invalid automation build metadata")):
            with self.assertRaisesRegex(protocol.ProtocolError, "Invalid automation build metadata"):
                doctor.build_check("apartment-stairs")

    def test_untrusted_fixture_manifest_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "scripts").mkdir()
            (root / "scripts/editor_ui_fixtures.json").write_text(json.dumps(
                {"version": 1, "fixtures": {"apartment-stairs": "../outside.level.json"}}), encoding="utf-8")
            with self.assertRaisesRegex(protocol.ProtocolError, "Invalid fixture IDs or level filenames"):
                doctor.build_check("apartment-stairs", root)

    def test_bad_resource_manifest_fails_without_launch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in ("shaders/prototype_scene_vertex.spv", "shaders/prototype_scene_fragment.spv",
                             "fonts/NotoSans-Regular.ttf", "characters/manifest.json"):
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text("invalid", encoding="utf-8")
            with patch("editor_ui_mcp.SessionController._configuration", return_value=(
                    root / "level_editor_automation.exe", root, root / "fixture.json", "a" * 64)):
                with self.assertRaises(json.JSONDecodeError):
                    doctor.build_check("apartment-stairs")

    def test_failed_preflight_still_allows_discovery_but_never_claims_ready(self):
        report = {}
        with (patch.object(doctor, "runtime_check", return_value={}),
              patch.object(doctor, "dependencies_check", return_value={}),
              patch.object(doctor, "prerequisites_check", side_effect=ValueError("missing cmake")),
              patch.object(doctor, "build_check", side_effect=ValueError("missing build")),
              patch.object(doctor, "sdk_check", new_callable=AsyncMock) as sdk):
            self.assertEqual(doctor.diagnose(arguments(), report), 1)
            sdk.assert_awaited_once()
        self.assertEqual(report["checks"]["sdk"]["status"], "passed")
        for name in ("desktop_gpu", "vulkan", "source_build_freshness", "agent_tools", "visual"):
            self.assertEqual(report[name]["status"], "not_checked")

    def test_missing_dependencies_block_sdk_with_clear_failure_exit(self):
        report = {}
        with (patch.object(doctor, "runtime_check", return_value={}),
              patch.object(doctor, "dependencies_check", side_effect=ValueError("missing SDK")),
              patch.object(doctor, "prerequisites_check", return_value={}),
              patch.object(doctor, "build_check", return_value={}),
              patch.object(doctor, "sdk_check", new_callable=AsyncMock) as sdk):
            self.assertEqual(doctor.diagnose(arguments(), report), 1)
            sdk.assert_not_awaited()
        self.assertEqual(report["checks"]["sdk"]["status"], "blocked")

    def test_report_never_overwrites_existing_file(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            path.write_text("existing", encoding="utf-8")
            with patch.object(doctor, "diagnose") as diagnose, contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(doctor.main(["--report", str(path)]), 1)
            diagnose.assert_not_called()
            self.assertEqual(path.read_text(encoding="utf-8"), "existing")

    def test_vulkan_audit_requires_real_enabled_validation_and_teardown(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "editor.log"
            path.write_text("Vulkan validation: enabled\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "zero-error teardown"):
                doctor.vulkan_check(path)
            path.write_text("Vulkan validation: enabled\nAutomation Vulkan validation errors after teardown: 0\n", encoding="utf-8")
            self.assertEqual(doctor.vulkan_check(path)["teardown_errors"], 0)


class DoctorSdkTests(unittest.IsolatedAsyncioTestCase):
    async def test_real_discovery_default_passes_only_initialize_and_list(self):
        # The actual production host is launched with no environment permission.
        # A call_tool guard additionally proves this path never sends start.
        from mcp import Client
        report = {}
        with patch.object(Client, "call_tool", new_callable=AsyncMock,
                          side_effect=AssertionError("Default doctor must not call a tool")) as call:
            await doctor.sdk_check(arguments(), report)
            call.assert_not_awaited()
        self.assertEqual(len(report["discovery"]["tools"]["tools"]), 4)
        self.assertTrue(report["discovery"]["connection"]["instructions"])
        self.assertNotIn("desktop_gpu", report)


if __name__ == "__main__":
    unittest.main()
