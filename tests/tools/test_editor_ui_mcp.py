"""Host tests use fake private-protocol peers and never launch an editor."""

from concurrent.futures import ThreadPoolExecutor
import copy
import io
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import editor_ui_mcp as host
import editor_ui_protocol as protocol
from editor_ui_process import LineReader, LineWriter, ProcessExit


def arguments(**fields):
    return {"protocol_version": 1, "request_id": "1", **fields}


class FakeChild:
    def __init__(self, command, *, cwd, mode="normal"):
        self.command, self.cwd, self.mode = command, cwd, mode
        self.messages = queue.Queue()
        self.sent = []
        self.closed = False
        self.session = None
        self.active = None
        self.completed = {}
        self.dead = False
        self.exit_at = None
        self.exit_code = None
        self.close_calls = 0
        self.outcome = None
        self.protocol_eof = False

    def base(self, request):
        return {"protocol_version": 1, "request_id": request["request_id"],
                "session_id": self.session, "build_fingerprint": "test-build", "ok": True}

    def status(self, request):
        return {**self.base(request), "state": "executing" if self.active else "ready",
                "capabilities": list(protocol.OPERATIONS), "limits": dict(protocol.LIMITS),
                "file_slots": self.configuration["file_slots"], "last_request": None,
                "excluded": list(host.EXCLUDED)}

    def send(self, message, *, priority=False):
        self.sent.append(message)
        if message["kind"] == "hello":
            hello = protocol.make_handshake("other" if self.mode == "mismatch" else "test-build")
            self.messages.put(hello)
            return
        if message["kind"] == "start":
            self.configuration = message
            self.session = message["session_id"]
            if self.mode.startswith("start_refused"):
                self.reply("ui_session", {**self.base(message["arguments"]), "ok": False,
                    "error": {"code": "environment_unavailable", "message": "injected construction failure",
                              "step_index": None, "target": None, "last_completed_index": None,
                              "expected": None, "observed": None, "snapshot": None, "effects": "none",
                              "cleanup": "released", "session_state": "faulted"}})
                self.exit_at = None if self.mode == "start_refused_stall" else time.monotonic() + .05
                self.protocol_eof = True
                return
            self.reply("ui_session", self.status(message["arguments"]))
            return
        tool, request = message["tool"], message["arguments"]
        if tool == "ui_execute":
            if self.mode == "die":
                self.dead = True
                self.exit_code = 3
            elif request["request_id"] in self.completed:
                self.reply(tool, self.completed[request["request_id"]])
            elif self.mode in ("wait", "stall", "all_wait", "close_stall", "close_missing_execution"):
                self.active = request
            elif self.mode in ("file_exit", "file_exit_error"):
                result = self.execution(request)
                result["session_state"] = "closing"
                self.reply(tool, result)
                self.exit_at = time.monotonic() + 0.05
                self.protocol_eof = True
            else:
                self.complete(request)
        elif tool == "ui_session":
            if request["op"] == "close":
                if self.mode != "close_no_ack":
                    self.reply(tool, {**self.status(request), "state": "closing"})
                if self.active and self.mode not in ("close_stall", "close_missing_execution"):
                    active, self.active = self.active, None
                    result = self.execution(active, cancelled=True)
                    result["session_state"] = result["error"]["session_state"] = "closing"
                    self.reply("ui_execute", result)
                if self.mode != "close_stall":
                    self.exit_at = time.monotonic() + (0.25 if self.mode in ("close_delayed", "close_early_eof") else 0.01)
                self.protocol_eof = self.mode == "close_early_eof"
                return
            if request["op"] == "cancel" and self.active and self.mode != "stall":
                active, self.active = self.active, None
                result = self.execution(active, cancelled=True)
                self.reply("ui_execute", result)
            self.reply(tool, self.status(request))
        elif tool == "app_inspect":
            self.reply(tool, {**self.base(request), "snapshot": self.stamp(), "values": [],
                              "next_cursor": None, "truncated": False})
        else:
            if self.mode == "all_wait":
                return
            self.reply(tool, {**self.base(request), "snapshot": self.stamp(), "active_scope": None,
                              "modal_scope": None, "items": [], "next_cursor": None,
                              "coverage": {"limitations": ["submitted_only"],
                                           "snapshot_item_count": 0, "truncated": False, "scopes": []}})

    @staticmethod
    def stamp():
        return {"snapshot_id": "snapshot-1", "frame": "1", "document_generation": "1",
                "document_revision": "1", "selection_revision": "1", "preview_revision": "1", "stale": False}

    def execution(self, request, cancelled=False):
        stamp = self.stamp()
        result = {**self.base(request), "steps": [], "before": stamp, "after": stamp,
                  "last_completed_index": None if cancelled else len(request["steps"]) - 1,
                  "failed_index": 0 if cancelled else None, "effects": "partial" if cancelled else "none",
                  "cleanup": "released", "session_state": "ready"}
        if cancelled:
            result.update(ok=False, error={"code": "cancelled", "message": "cancelled",
                "step_index": 0, "target": None, "last_completed_index": None,
                "expected": None, "observed": None, "snapshot": stamp, "effects": "partial",
                "cleanup": "released", "session_state": "ready"})
        for index in range(len(request["steps"])):
            if cancelled:
                step = {"index": index, "status": "failed" if index == 0 else "not_run",
                        "before": stamp if index == 0 else None, "after": stamp if index == 0 else None}
                if index == 0:
                    step["error"] = result["error"]
            else:
                step = {"index": index, "status": "passed", "before": stamp, "after": stamp,
                        "observed": {"availability": "not_applicable"}}
            result["steps"].append(step)
        return result

    def complete(self, request):
        result = self.execution(request)
        self.completed[request["request_id"]] = result
        self.reply("ui_execute", result)

    def reply(self, tool, result):
        self.messages.put({"kind": "result", "tool": tool, "result": result})

    def receive(self, timeout=0.01):
        if (self.dead or self.protocol_eof) and self.messages.empty():
            raise protocol.ProtocolError("disconnected", "fake child exited")
        try:
            return self.messages.get(timeout=timeout)
        except queue.Empty:
            return None

    def close(self, *, grace=2, deadline=None):
        self.close_calls += 1
        if self.outcome is not None:
            return self.outcome
        self.closed = True
        until = min(time.monotonic() + grace, (deadline or time.monotonic() + 2) - 0.2)
        while not self.dead and time.monotonic() < until:
            if self.exit_at is not None and time.monotonic() >= self.exit_at:
                self.dead = True
                self.exit_code = 2 if self.mode.startswith("start_refused") else (
                    3 if self.mode in ("close_error", "close_early_eof", "file_exit_error") else 0)
                break
            time.sleep(0.001)
        forced = not self.dead
        if forced:
            self.dead, self.exit_code = True, -9
        diagnostics = "validation teardown failure" if self.mode in ("close_error", "close_early_eof", "file_exit_error") else ""
        if self.mode.startswith("start_refused"):
            diagnostics = "partial construction teardown diagnostic"
        self.outcome = ProcessExit(self.exit_code, forced, diagnostics)
        return self.outcome


class ControllerTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        binary = self.root / "build/debug/bin"
        (binary / "resources/levels").mkdir(parents=True)
        (binary / "level_editor_automation.exe").write_bytes(b"not executable: fake peer only")
        (binary / "editor-ui-build.json").write_text('{"build_fingerprint":"test-build"}', encoding="utf-8")
        self.fixture = binary / "resources/levels/apartment-stairs.level.json"
        self.fixture.write_bytes(b'{"source":"unchanged"}')
        self.children = []
        self.mode = "normal"
        self.exits = []
        self.exit_report = mock.patch.object(host, "_report_exit", side_effect=self.exits.append)
        self.exit_report.start()
        self.addCleanup(self.exit_report.stop)

        def factory(command, *, cwd):
            child = FakeChild(command, cwd=cwd, mode=self.mode)
            self.children.append(child)
            return child

        self.controller = host.SessionController(environment="test-profile", repository_root=self.root,
                                                 process_factory=factory)

    def tearDown(self):
        self.controller.disconnect()
        self.directory.cleanup()

    def start(self):
        result = self.controller.call("ui_session", arguments(op="start", fixture="apartment-stairs",
                                                              environment="test-profile"))
        protocol.validate_result("ui_session", result)
        self.assertTrue(result["ok"], result)
        return result

    def call(self, tool, **fields):
        request = arguments(session_id=self.controller.session_id, **fields)
        result = self.controller.call(tool, request)
        protocol.validate_result(tool, result, request=request)
        return result

    def test_no_start_without_operator_authorization(self):
        self.controller.environment = None
        result = self.controller.call("ui_session", arguments(op="start", fixture="apartment-stairs",
                                                              environment="test-profile"))
        self.assertEqual(result["error"]["code"], "environment_not_authorized")
        self.assertEqual(self.children, [])
        self.assertEqual(self.controller.state, "closed")

    def test_fixture_manifest_and_fixed_command(self):
        refused = self.controller.call("ui_session", arguments(op="start", fixture="../outside",
                                                               environment="test-profile"))
        self.assertEqual(refused["error"]["code"], "invalid_request")
        result = self.start()
        child = self.children[0]
        self.assertEqual(child.command, [str(self.root / "build/debug/bin/level_editor_automation.exe"), "--automation-pipe"])
        self.assertEqual(len(result["file_slots"]), 9)
        self.assertEqual(Path(child.configuration["input_path"]).read_bytes(), self.fixture.read_bytes())
        for slot in result["file_slots"][1:]:
            self.assertFalse(Path(slot["path"]).exists())
        Path(child.configuration["input_path"]).write_bytes(b"temporary edit")
        self.assertEqual(self.fixture.read_bytes(), b'{"source":"unchanged"}')
        root = Path(child.configuration["root"])
        self.call("ui_session", op="close")
        self.assertFalse(root.exists())
        self.assertTrue(child.closed)

    def test_handshake_mismatch_terminates_owned_child(self):
        self.mode = "mismatch"
        result = self.controller.call("ui_session", arguments(op="start", fixture="apartment-stairs",
                                                              environment="test-profile"))
        self.assertEqual(result["error"]["code"], "version_mismatch")
        self.assertTrue(self.children[0].closed)
        self.assertFalse(any(message["kind"] == "start" for message in self.children[0].sent))

    def test_startup_exception_cannot_replace_unverified_owned_child(self):
        self.mode = "mismatch"
        with mock.patch.object(FakeChild, "close", return_value=ProcessExit(None, True, "unreaped child")):
            result = self.controller.call("ui_session", arguments(op="start", fixture="apartment-stairs",
                                                                  environment="test-profile"))
        self.assertEqual(result["error"]["code"], "version_mismatch")
        self.assertEqual(result["error"]["cleanup"], "unverified")
        self.assertEqual(self.controller.state, "faulted")
        child, temporary = self.controller.process, self.controller.temp
        self.assertIs(child, self.children[0])
        self.assertTrue(Path(temporary.name).exists())
        self.controller.state = "closed"  # Ownership guard is independent of the state label.
        refused = self.controller.call("ui_session", arguments(request_id="second", op="start",
                                        fixture="apartment-stairs", environment="test-profile"))
        self.assertEqual(refused["error"]["code"], "busy")
        self.assertEqual(len(self.children), 1)
        self.assertIs(self.controller.process, child)
        self.controller.state = "faulted"
        child.dead, child.exit_code = True, -9
        self.controller._terminate("cancelled", "test cleanup")

    def test_close_during_process_creation_retains_unverified_child(self):
        entered, release = threading.Event(), threading.Event()
        factory = self.controller.process_factory

        def held_factory(*args, **kwargs):
            child = factory(*args, **kwargs)
            entered.set()
            release.wait(3)
            return child

        self.controller.process_factory = held_factory
        with mock.patch.object(FakeChild, "close", return_value=ProcessExit(None, True, "unreaped child")):
            with ThreadPoolExecutor(max_workers=2) as executor:
                starting = executor.submit(self.controller.call, "ui_session",
                    arguments(op="start", fixture="apartment-stairs", environment="test-profile"))
                try:
                    self.assertTrue(entered.wait(1))
                    root = Path(self.controller.temp.name)
                    closing = executor.submit(self.call, "ui_session", request_id="close", op="close")
                    until = time.monotonic() + 1
                    while self.controller._closing is None and time.monotonic() < until:
                        time.sleep(.001)
                    self.assertIsNotNone(self.controller._closing)
                    self.assertTrue(root.exists())
                finally:
                    release.set()
                self.assertEqual(starting.result(timeout=2.1)["error"]["code"], "cancelled")
                self.assertEqual(closing.result(timeout=2.1)["error"]["cleanup"], "unverified")
        self.assertIs(self.controller.process, self.children[0])
        self.assertEqual(self.controller.state, "faulted")
        self.assertTrue(root.exists())
        self.assertFalse(any(message["kind"] == "hello" for message in self.children[0].sent))
        self.children[0].dead, self.children[0].exit_code = True, -9
        self.controller._terminate("cancelled", "test cleanup")

    def test_creation_past_close_deadline_keeps_root_until_late_child_is_reaped(self):
        entered, release = threading.Event(), threading.Event()
        factory = self.controller.process_factory

        def held_factory(*args, **kwargs):
            child = factory(*args, **kwargs)
            entered.set()
            release.wait(4)
            return child

        self.controller.process_factory = held_factory
        with ThreadPoolExecutor(max_workers=1) as executor:
            starting = executor.submit(self.controller.call, "ui_session",
                arguments(op="start", fixture="apartment-stairs", environment="test-profile"))
            try:
                self.assertTrue(entered.wait(1))
                root = Path(self.controller.temp.name)
                before = time.monotonic()
                closed = self.call("ui_session", request_id="close", op="close")
                self.assertLess(time.monotonic() - before, 2.1)
                self.assertEqual(closed["error"]["cleanup"], "unverified")
                self.assertEqual(self.controller.state, "faulted")
                self.assertTrue(root.exists())
                refused = self.controller.call("ui_session", arguments(request_id="again", op="start",
                                    fixture="apartment-stairs", environment="test-profile"))
                self.assertEqual(refused["error"]["code"], "busy")
            finally:
                release.set()
            self.assertEqual(starting.result(timeout=1)["error"]["code"], "cancelled")
        self.assertTrue(self.children[0].closed)
        self.assertIsNone(self.controller.process)
        self.assertIsNone(self.controller._launching)
        self.assertFalse(root.exists())

    def test_late_creation_publication_survives_concurrent_unverified_cleanup(self):
        entered, release_factory = threading.Event(), threading.Event()
        restoring, release_cleanup = threading.Event(), threading.Event()
        factory = self.controller.process_factory
        original_cleanup = ProcessExit.cleanup.fget

        def held_factory(*args, **kwargs):
            child = factory(*args, **kwargs)
            entered.set()
            release_factory.wait(3)
            return child

        def held_cleanup(outcome):
            if outcome.exit_code is None:
                restoring.set()
                release_cleanup.wait(3)
            return original_cleanup(outcome)

        self.controller.process_factory = held_factory
        with mock.patch.object(ProcessExit, "cleanup", property(held_cleanup)):
            with ThreadPoolExecutor(max_workers=2) as executor:
                starting = executor.submit(self.controller.call, "ui_session",
                    arguments(op="start", fixture="apartment-stairs", environment="test-profile"))
                try:
                    self.assertTrue(entered.wait(1))
                    root = Path(self.controller.temp.name)
                    aborting = executor.submit(self.controller.disconnect, "invalid_request", "bad transport")
                    self.assertTrue(restoring.wait(1))
                    release_factory.set()
                    self.assertEqual(starting.result(timeout=1)["error"]["cleanup"], "unverified")
                finally:
                    release_factory.set()
                    release_cleanup.set()
                aborting.result(timeout=1)
        self.assertIs(self.controller.process, self.children[0])
        self.assertEqual(self.controller.state, "faulted")
        self.assertTrue(root.exists())
        self.children[0].dead, self.children[0].exit_code = True, -9
        self.controller._terminate("cancelled", "test cleanup")

    def test_refused_startup_waits_for_final_exit_and_reports_actual_cleanup(self):
        for mode, forced, code in (("start_refused", False, 2), ("start_refused_stall", True, -9)):
            with self.subTest(mode=mode):
                self.mode = mode
                before = time.monotonic()
                result = self.controller.call("ui_session", arguments(op="start", fixture="apartment-stairs",
                                                                      environment="test-profile"))
                protocol.validate_result("ui_session", result)
                self.assertLess(time.monotonic() - before, 2.1)
                self.assertFalse(result["ok"])
                self.assertEqual(result["error"]["code"], "environment_unavailable")
                self.assertEqual(result["error"]["cleanup"], "process_terminated")
                self.assertEqual(self.controller.last_exit.exit_code, code)
                self.assertEqual(self.controller.last_exit.forced, forced)
                self.assertIn("partial construction teardown", self.controller.last_exit.diagnostics)
                self.assertEqual(self.children[-1].close_calls, 1)
                self.assertFalse(Path(self.children[-1].configuration["root"]).exists())
                self.call("ui_session", op="close")

    def test_refused_startup_retains_ownership_when_exit_cannot_be_verified(self):
        self.mode = "start_refused"
        with mock.patch.object(FakeChild, "close", return_value=ProcessExit(None, True, "unverified exit")):
            result = self.controller.call("ui_session", arguments(op="start", fixture="apartment-stairs",
                                                                  environment="test-profile"))
        protocol.validate_result("ui_session", result)
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"]["cleanup"], "unverified")
        self.assertIs(self.controller.process, self.children[-1])
        self.assertTrue(Path(self.children[-1].configuration["root"]).exists())

    def test_refused_startup_waits_for_existing_eof_cleanup_owner(self):
        self.mode = "start_refused"
        entered, release, received = threading.Event(), threading.Event(), threading.Event()
        original_close = FakeChild.close
        original_roundtrip = self.controller._roundtrip

        def held_close(child, **kwargs):
            entered.set()
            release.wait(1)
            return original_close(child, **kwargs)

        def after_eof_owner(*args, **kwargs):
            result = original_roundtrip(*args, **kwargs)
            self.assertTrue(entered.wait(1))
            received.set()
            return result

        with mock.patch.object(FakeChild, "close", held_close), \
                mock.patch.object(self.controller, "_roundtrip", side_effect=after_eof_owner):
            with ThreadPoolExecutor(max_workers=1) as executor:
                starting = executor.submit(self.controller.call, "ui_session",
                    arguments(op="start", fixture="apartment-stairs", environment="test-profile"))
                try:
                    self.assertTrue(received.wait(1))
                    self.assertFalse(starting.done())
                    self.assertTrue(Path(self.children[-1].configuration["root"]).exists())
                finally:
                    release.set()
                result = starting.result(timeout=2.1)
        self.assertEqual(result["error"]["cleanup"], "process_terminated")
        self.assertEqual(self.controller.last_exit.exit_code, 2)
        self.assertFalse(self.controller.last_exit.forced)
        self.assertEqual(self.children[-1].close_calls, 1)

    def test_close_waits_for_delayed_teardown_and_has_one_owner(self):
        self.mode = "close_delayed"
        self.start()
        before = time.monotonic()
        with ThreadPoolExecutor(max_workers=1) as executor:
            closing = executor.submit(self.call, "ui_session", request_id="close", op="close")
            until = time.monotonic() + 1
            while self.controller._closing is None and time.monotonic() < until:
                time.sleep(.001)
            self.assertEqual(self.call("ui_session", request_id="during", op="status")["state"], "closing")
            duplicate = self.call("ui_session", request_id="duplicate-close", op="close")
            self.assertEqual(duplicate["error"]["code"], "busy")
            result = closing.result(timeout=2.1)
        self.assertTrue(result["ok"], result)
        self.assertEqual(result["state"], "closed")
        self.assertGreater(time.monotonic() - before, .2)
        self.assertLess(time.monotonic() - before, 2.1)
        self.assertEqual(self.children[0].close_calls, 1)
        self.assertEqual(self.controller.last_exit.exit_code, 0)
        self.assertFalse(self.controller.last_exit.forced)

    def test_close_reports_post_ack_validation_failure_and_retains_audit(self):
        for mode in ("close_error", "close_early_eof"):
            with self.subTest(mode=mode):
                self.mode = mode
                self.start()
                result = self.call("ui_session", op="close")
                self.assertFalse(result["ok"])
                self.assertEqual(result["error"]["code"], "engine_error")
                self.assertEqual(result["error"]["cleanup"], "process_terminated")
                self.assertIn("code 3", result["error"]["message"])
                self.assertIn("validation teardown failure", result["error"]["message"])
                self.assertEqual(self.controller.last_exit.exit_code, 3)
                self.assertFalse(self.controller.last_exit.forced)
                self.assertEqual(self.exits[-1], self.controller.last_exit)

    def test_execution_crossing_close_boundary_is_never_sent(self):
        self.mode = "close_delayed"
        self.start()
        entered, release = threading.Event(), threading.Event()
        original = self.controller._roundtrip

        def delayed_roundtrip(tool, *args, **kwargs):
            if tool == "ui_execute":
                entered.set()
                release.wait(1)
            return original(tool, *args, **kwargs)

        with mock.patch.object(self.controller, "_roundtrip", side_effect=delayed_roundtrip):
            with ThreadPoolExecutor(max_workers=2) as executor:
                executing = executor.submit(self.call, "ui_execute",
                    steps=[{"op": "activate", "target": {"ref": "item"}}])
                self.assertTrue(entered.wait(1))
                closing = executor.submit(self.call, "ui_session", request_id="close", op="close")
                until = time.monotonic() + 1
                while self.controller._closing is None and time.monotonic() < until:
                    time.sleep(.001)
                release.set()
                self.assertEqual(executing.result(timeout=1)["error"]["code"], "busy")
                self.assertTrue(closing.result(timeout=1)["ok"])
        self.assertFalse(any(message.get("tool") == "ui_execute" for message in self.children[0].sent))

    def test_close_cannot_hide_an_already_observed_bad_exit(self):
        self.mode = "die"
        self.start()
        self.call("ui_execute", steps=[{"op": "activate", "target": {"ref": "item"}}])
        self.assertEqual(self.controller.state, "faulted")
        result = self.call("ui_session", op="close")
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"]["code"], "engine_error")
        self.assertIn("code 3", result["error"]["message"])
        self.assertEqual(self.controller.state, "closed")

    def test_file_exit_eof_preserves_natural_exit_and_final_diagnostics(self):
        for mode, code in (("file_exit", 0), ("file_exit_error", 3)):
            with self.subTest(mode=mode):
                self.mode = mode
                self.start()
                child = self.children[-1]
                root = Path(child.configuration["root"])
                before = time.monotonic()
                result = self.call("ui_execute", steps=[{"op": "activate", "target": {"ref": "exit"}}])
                self.assertTrue(result["ok"], result)
                self.assertEqual(result["session_state"], "closing")
                self.assertEqual(result["cleanup"], "released")
                until = before + 2.1
                while (not self.exits or self.exits[-1] is not child.outcome or root.exists()) and time.monotonic() < until:
                    time.sleep(.001)
                self.assertLess(time.monotonic() - before, 2.1)
                self.assertEqual(self.controller.last_exit.exit_code, code)
                self.assertFalse(self.controller.last_exit.forced)
                self.assertIs(self.exits[-1], child.outcome)
                self.assertFalse(root.exists())
                self.assertEqual(child.close_calls, 1)
                self.assertEqual(self.controller.state, "faulted")
                if code:
                    self.assertIn("validation teardown failure", self.controller.last_exit.diagnostics)
                self.call("ui_session", op="close")

    def test_close_diagnostics_obey_utf8_error_budget_and_preserve_tail(self):
        self.mode = "close_error"
        self.start()
        child = self.children[0]
        original = child.close

        def unicode_failure(**kwargs):
            outcome = original(**kwargs)
            return ProcessExit(outcome.exit_code, outcome.forced, "\u044f" * 5000 + " FINAL")

        with mock.patch.object(child, "close", side_effect=unicode_failure):
            result = self.call("ui_session", op="close")
        self.assertFalse(result["ok"])
        self.assertLessEqual(len(result["error"]["message"].encode("utf-8")), 512)
        self.assertTrue(result["error"]["message"].endswith(" FINAL"))
        self.assertEqual(self.controller.last_exit.diagnostics, "\u044f" * 5000 + " FINAL")

    def test_unverified_exit_retains_owned_process_and_files_for_retry(self):
        self.start()
        child = self.children[0]
        root = Path(child.configuration["root"])

        def cannot_reap(**_kwargs):
            child.dead = True
            return ProcessExit(None, True, "exit wait did not complete")

        with mock.patch.object(child, "close", side_effect=cannot_reap):
            result = self.call("ui_session", op="close")
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"]["cleanup"], "unverified")
        self.assertEqual(self.controller.state, "faulted")
        self.assertIs(self.controller.process, child)
        self.assertTrue(root.exists())
        child.exit_code = -9
        child.outcome = ProcessExit(-9, True, "exit now verified")
        result = self.call("ui_session", request_id="retry-close", op="close")
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"]["cleanup"], "process_terminated")
        self.assertEqual(self.controller.state, "closed")
        self.assertFalse(root.exists())

    def test_close_requires_acknowledgement_even_when_exit_is_zero(self):
        self.mode = "close_no_ack"
        self.start()
        result = self.call("ui_session", op="close")
        self.assertFalse(result["ok"])
        self.assertEqual(result["error"]["code"], "engine_error")
        self.assertIn("without acknowledging", result["error"]["message"])

    def test_close_drains_active_execution_before_reporting_success(self):
        self.mode = "wait"
        self.start()
        with ThreadPoolExecutor(max_workers=1) as executor:
            executing = executor.submit(self.call, "ui_execute",
                steps=[{"op": "activate", "target": {"ref": "item"}}])
            until = time.monotonic() + 1
            while self.children[0].active is None and time.monotonic() < until:
                time.sleep(.001)
            result = self.call("ui_session", request_id="close", op="close")
            execution = executing.result(timeout=1)
        self.assertTrue(result["ok"], result)
        self.assertEqual(execution["error"]["code"], "cancelled")
        self.assertEqual(execution["cleanup"], "released")
        self.assertEqual(self.controller.state, "closed")

    def test_close_stall_or_missing_terminal_execution_never_claims_success(self):
        for mode in ("close_stall", "close_missing_execution"):
            with self.subTest(mode=mode):
                self.mode = mode
                self.start()
                with ThreadPoolExecutor(max_workers=1) as executor:
                    executing = executor.submit(self.call, "ui_execute",
                        steps=[{"op": "activate", "target": {"ref": "item"}}])
                    until = time.monotonic() + 1
                    while self.children[-1].active is None and time.monotonic() < until:
                        time.sleep(.001)
                    before = time.monotonic()
                    result = self.call("ui_session", request_id="close", op="close")
                    execution = executing.result(timeout=1)
                self.assertFalse(result["ok"])
                self.assertEqual(result["error"]["code"], "timeout" if mode == "close_stall" else "engine_error")
                self.assertEqual(result["error"]["cleanup"], "process_terminated")
                self.assertEqual(result["error"]["effects"], "unknown")
                self.assertLess(time.monotonic() - before, 2.1)
                self.assertEqual(execution["effects"], "unknown")
                self.assertEqual(execution["steps"][0]["status"], "unknown")
                self.assertEqual(self.controller.state, "closed")

    def test_status_cancel_and_competing_batches_while_executing(self):
        self.mode = "wait"
        self.start()
        request = arguments(session_id=self.controller.session_id,
                            steps=[{"op": "activate", "target": {"ref": "item"}}] * 2)
        with ThreadPoolExecutor(max_workers=1) as executor:
            future = executor.submit(self.controller.call, "ui_execute", request)
            deadline = time.monotonic() + 1
            while self.children[0].active is None and time.monotonic() < deadline:
                time.sleep(0.001)
            before = time.monotonic()
            self.assertEqual(self.call("ui_session", request_id="status", op="status")["state"], "executing")
            self.assertLess(time.monotonic() - before, 0.1)
            duplicate = self.controller.call("ui_execute", request)
            self.assertEqual(duplicate["status"], "running")
            conflict = copy.deepcopy(request)
            conflict["steps"][0]["op"] = "focus"
            self.assertEqual(self.controller.call("ui_execute", conflict)["error"]["code"], "request_id_conflict")
            competing = {**request, "request_id": "2"}
            self.assertEqual(self.controller.call("ui_execute", competing)["error"]["code"], "busy")
            self.call("ui_session", request_id="cancel", op="cancel", active_request_id="1")
            result = future.result(timeout=1)
            protocol.validate_result("ui_execute", result, request=request)
            self.assertEqual(result["error"]["code"], "cancelled")
            self.assertEqual([s["status"] for s in result["steps"]], ["failed", "not_run"])

    def test_child_loss_reports_unknown_steps_without_replay(self):
        self.mode = "die"
        self.start()
        result = self.call("ui_execute", steps=[{"op": "activate", "target": {"ref": "item"}}] * 2)
        self.assertFalse(result["ok"])
        self.assertEqual(result["effects"], "unknown")
        self.assertEqual(result["cleanup"], "process_terminated")
        self.assertEqual([s["status"] for s in result["steps"]], ["unknown", "unknown"])
        self.assertEqual(len([m for m in self.children[0].sent if m.get("tool") == "ui_execute"]), 1)
        self.assertEqual(self.controller.state, "faulted")

    def test_timeout_cancels_then_terminates_unresponsive_child(self):
        self.mode = "stall"
        self.start()
        before = time.monotonic()
        result = self.call("ui_execute", timeout_ms=1, steps=[{"op": "activate", "target": {"ref": "item"}}])
        self.assertEqual(result["error"]["code"], "timeout")
        self.assertLess(time.monotonic() - before, 2.5)
        self.assertTrue(self.children[0].closed)
        self.assertTrue(any(m.get("arguments", {}).get("op") == "cancel" for m in self.children[0].sent))

    def test_controller_eof_closes_active_work_with_one_bounded_deadline(self):
        for mode, forced in (("wait", False), ("close_stall", True)):
            with self.subTest(mode=mode):
                self.mode = mode
                self.start()
                peer = WirePeer(self.controller)
                try:
                    with ThreadPoolExecutor(max_workers=1) as executor:
                        executing = executor.submit(self.call, "ui_execute",
                            steps=[{"op": "activate", "target": {"ref": "item"}}])
                        until = time.monotonic() + 1
                        while self.children[-1].active is None and time.monotonic() < until:
                            time.sleep(.001)
                        self.assertIsNotNone(self.children[-1].active)
                        before = time.monotonic()
                        peer.input.close()
                        peer.thread.join(timeout=2.2)
                        self.assertFalse(peer.thread.is_alive())
                        self.assertLess(time.monotonic() - before, 2.2)
                        execution = executing.result(timeout=.2)
                    self.assertEqual(self.controller.last_exit.forced, forced)
                    self.assertEqual(self.controller.last_exit.exit_code, -9 if forced else 0)
                    self.assertEqual(execution["cleanup"], "process_terminated" if forced else "released")
                    self.assertEqual(self.controller.state, "closed")
                    controls = [item["arguments"]["op"] for item in self.children[-1].sent
                                if item.get("tool") == "ui_session"]
                    self.assertEqual(controls, ["close"])
                    self.assertEqual(self.children[-1].close_calls, 1)
                finally:
                    peer.close()

    def test_malformed_controller_transport_aborts_owned_child_without_close_grace(self):
        self.start()
        peer = WirePeer(self.controller)
        try:
            before = time.monotonic()
            peer.input.write(b'{"duplicate":1,"duplicate":2}\n')
            # Bad framing closes the bounded SDK transport; diagnostics use stderr.
            peer.thread.join(timeout=.5)
            self.assertFalse(peer.thread.is_alive())
            self.assertLess(time.monotonic() - before, .5)
            self.assertTrue(self.controller.last_exit.forced)
            self.assertEqual(self.controller.last_exit.cleanup, "process_terminated")
            self.assertFalse(any(item.get("arguments", {}).get("op") == "close"
                                 for item in self.children[-1].sent))
        finally:
            peer.close()

    def test_controller_eof_waits_for_existing_close_or_child_eof_owner(self):
        for mode in ("file_exit", "close_delayed"):
            with self.subTest(mode=mode):
                self.mode = mode
                self.start()
                child = self.children[-1]
                entered, release = threading.Event(), threading.Event()
                original = child.close

                def held_close(**kwargs):
                    entered.set()
                    release.wait(1)
                    return original(**kwargs)

                peer = WirePeer(self.controller)
                try:
                    with mock.patch.object(child, "close", side_effect=held_close):
                        with ThreadPoolExecutor(max_workers=1) as executor:
                            if mode == "file_exit":
                                self.call("ui_execute", steps=[{"op": "activate", "target": {"ref": "exit"}}])
                                closing = None
                            else:
                                closing = executor.submit(self.call, "ui_session", request_id="close", op="close")
                            try:
                                self.assertTrue(entered.wait(1))
                                peer.input.close()
                                peer.thread.join(timeout=.03)
                                self.assertTrue(peer.thread.is_alive())
                                self.assertIsNone(self.controller.last_exit)
                            finally:
                                release.set()
                            if closing is not None:
                                self.assertTrue(closing.result(timeout=2.1)["ok"])
                            peer.thread.join(timeout=2.1)
                            self.assertFalse(peer.thread.is_alive())
                    self.assertEqual(self.controller.last_exit.exit_code, 0)
                    self.assertFalse(self.controller.last_exit.forced)
                    self.assertEqual(child.close_calls, 1)
                    self.assertIs(self.exits[-1], child.outcome)
                finally:
                    release.set()
                    peer.close()
                self.call("ui_session", request_id="reset", op="close")

    def test_read_only_calls_and_idle_expiry(self):
        self.start()
        self.assertTrue(self.call("ui_observe", scope="root")["ok"])
        self.assertTrue(self.call("app_inspect", projection="document")["ok"])
        self.controller.last_activity = time.monotonic() - 301
        self.controller.tick()
        self.assertTrue(self.children[0].closed)
        self.assertEqual(self.controller.state, "faulted")

    def test_wrong_session_error_preserves_request_identity(self):
        self.start()
        request = arguments(session_id="expired-session", scope="root")
        result = self.controller.call("ui_observe", request)
        protocol.validate_result("ui_observe", result, request=request)
        self.assertEqual(result["error"]["code"], "session_closed")

    def test_late_old_pipe_failure_cannot_terminate_a_new_session(self):
        self.start()
        old_child = self.children[0]
        entered, release = threading.Event(), threading.Event()

        def delayed_failure(timeout=0.01):
            entered.set()
            release.wait(3)
            raise protocol.ProtocolError("disconnected", "late failure from old pipe")

        old_child.receive = delayed_failure
        self.assertTrue(entered.wait(1))
        self.controller.disconnect()
        self.call("ui_session", request_id="close-old", op="close")
        new_result = self.start()
        try:
            release.set()
            time.sleep(0.03)
            self.assertFalse(self.children[1].closed)
            self.assertEqual(self.controller.session_id, new_result["session_id"])
            self.assertTrue(self.call("ui_observe", request_id="new-observe", scope="root")["ok"])
        finally:
            release.set()

    def test_mcp_status_and_cancel_bypass_saturated_regular_workers(self):
        self.mode = "all_wait"
        self.start()
        peer = WirePeer(self.controller)
        try:
            peer.initialize()
            batch = arguments(session_id=self.controller.session_id,
                              steps=[{"op": "activate", "target": {"ref": "item"}}])
            peer.send({"jsonrpc": "2.0", "id": 10, "method": "tools/call", "params": {
                "name": "ui_execute", "arguments": batch}})
            for index in range(3):
                peer.send({"jsonrpc": "2.0", "id": 20 + index, "method": "tools/call", "params": {
                    "name": "ui_observe", "arguments": arguments(request_id=f"observe-{index}",
                    session_id=self.controller.session_id, scope="root")}})
            deadline = time.monotonic() + 1
            while len(self.controller.pending) < 4 and time.monotonic() < deadline:
                time.sleep(0.001)
            self.assertEqual(len(self.controller.pending), 4)
            peer.send({"jsonrpc": "2.0", "id": 30, "method": "tools/call", "params": {
                "name": "ui_session", "arguments": arguments(request_id="status",
                session_id=self.controller.session_id, op="status")}})
            self.assertEqual(peer.receive()["id"], 30)
            peer.send({"jsonrpc": "2.0", "id": 31, "method": "tools/call", "params": {
                "name": "ui_session", "arguments": arguments(request_id="cancel",
                session_id=self.controller.session_id, op="cancel", active_request_id="1")}})
            replies = [peer.receive(), peer.receive()]
            self.assertEqual({reply["id"] for reply in replies}, {10, 31})
        finally:
            peer.close()

    def test_mcp_cancellation_notification_stops_batch_and_suppresses_cancelled_response(self):
        self.mode = "wait"
        self.start()
        peer = WirePeer(self.controller)
        try:
            peer.initialize()
            batch = arguments(session_id=self.controller.session_id,
                              steps=[{"op": "activate", "target": {"ref": "item"}}])
            peer.send({"jsonrpc": "2.0", "id": 10, "method": "tools/call", "params": {
                "name": "ui_execute", "arguments": batch}})
            deadline = time.monotonic() + 1
            while self.children[0].active is None and time.monotonic() < deadline:
                time.sleep(0.001)
            peer.send({"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": 10}})
            peer.send({"jsonrpc": "2.0", "id": 11, "method": "ping"})
            self.assertEqual(peer.receive()["id"], 11)
            deadline = time.monotonic() + 1
            while self.controller.running is not None and time.monotonic() < deadline:
                time.sleep(0.001)
            self.assertIsNone(self.controller.running)
            with self.assertRaises(queue.Empty):
                peer.reader.messages.get(timeout=0.05)
        finally:
            peer.close()

    def test_sdk_cancelled_start_closes_child_even_after_readiness(self):
        ready = threading.Event()
        release = threading.Event()
        roundtrip = self.controller._roundtrip

        def delayed_result(tool, request, *args, **kwargs):
            result = roundtrip(tool, request, *args, **kwargs)
            if tool == "ui_session" and request["op"] == "start":
                ready.set()
                release.wait(2)
            return result

        with mock.patch.object(self.controller, "_roundtrip", side_effect=delayed_result):
            peer = WirePeer(self.controller)
            try:
                peer.initialize()
                peer.send({"jsonrpc": "2.0", "id": 10, "method": "tools/call", "params": {
                    "name": "ui_session", "arguments": arguments(op="start", fixture="apartment-stairs",
                                                                  environment="test-profile")}})
                self.assertTrue(ready.wait(2))
                peer.send({"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": 10}})
                deadline = time.monotonic() + 2
                while self.controller.state != "closed" and time.monotonic() < deadline:
                    time.sleep(.005)
                release.set()
                self.assertEqual(self.controller.state, "closed")
                self.assertTrue(self.children[0].closed)
            finally:
                release.set()
                peer.close()

    def test_sdk_eof_with_pending_stalled_batch_has_one_cleanup_budget(self):
        self.mode = "close_stall"
        self.start()
        peer = WirePeer(self.controller)
        try:
            peer.initialize()
            peer.send({"jsonrpc": "2.0", "id": 10, "method": "tools/call", "params": {
                "name": "ui_execute", "arguments": arguments(session_id=self.controller.session_id,
                    steps=[{"op": "activate", "target": {"ref": "item"}}])}})
            deadline = time.monotonic() + 2
            while not self.controller.running and time.monotonic() < deadline:
                time.sleep(.005)
            self.assertIsNotNone(self.controller.running)
            begin = time.monotonic()
            peer.input.close()
            peer.thread.join(3)
            self.assertFalse(peer.thread.is_alive())
            self.assertLess(time.monotonic() - begin, 2.6)
            self.assertTrue(self.children[0].closed)
            self.assertEqual(self.controller.last_exit.cleanup, "process_terminated")
        finally:
            peer.close()

    def test_sdk_eof_prevents_start_worker_acquiring_late_ownership(self):
        entered = threading.Event()
        release = threading.Event()
        disconnected = threading.Event()
        start = self.controller._start
        disconnect = self.controller.disconnect

        def before_start(*args, **kwargs):
            entered.set()
            release.wait(2)
            return start(*args, **kwargs)

        def after_disconnect(*args, **kwargs):
            result = disconnect(*args, **kwargs)
            disconnected.set()
            return result

        with mock.patch.object(self.controller, "_start", side_effect=before_start), \
                mock.patch.object(self.controller, "disconnect", side_effect=after_disconnect):
            peer = WirePeer(self.controller)
            try:
                peer.initialize()
                peer.send({"jsonrpc": "2.0", "id": 10, "method": "tools/call", "params": {
                    "name": "ui_session", "arguments": arguments(op="start", fixture="apartment-stairs",
                                                                  environment="test-profile")}})
                self.assertTrue(entered.wait(2))
                peer.input.close()
                self.assertTrue(disconnected.wait(2))
                release.set()
                peer.thread.join(2)
                self.assertFalse(peer.thread.is_alive())
                self.assertEqual(self.controller.state, "closed")
                self.assertEqual(self.children, [])
            finally:
                release.set()
                peer.close()


class DiagnosticTests(unittest.TestCase):
    def test_failed_result_artifact_eviction_does_not_exceed_capacity(self):
        controller = host.SessionController()
        controller.result_artifacts.extend(f"locked-{index}.json" for index in range(32))
        with mock.patch.object(Path, "unlink", side_effect=PermissionError("held by reader")), \
                self.assertRaises(PermissionError):
            controller.retain_diagnostic({"evidence": "bounded"})
        self.assertEqual(len(controller.result_artifacts), 32)
        self.assertEqual(controller.result_artifacts[0], "locked-0.json")

    def test_blocked_stderr_is_bounded_and_does_not_block_close_reporting(self):
        entered, release, flushed = threading.Event(), threading.Event(), threading.Event()
        written = []

        class BlockedStderr:
            def write(self, text):
                entered.set()
                release.wait(2)
                written.append(text)
                return len(text)

            def flush(self):
                flushed.set()

        with mock.patch.object(host.sys, "stderr", BlockedStderr()):
            try:
                host._report_exit(ProcessExit(3, False, "validation failure"))
                self.assertTrue(entered.wait(1))
                before = time.monotonic()
                for _ in range(20):
                    host._report_exit(ProcessExit(3, False, "x" * 20000))
                self.assertLess(time.monotonic() - before, .1)
                self.assertLessEqual(host._diagnostic_queue.qsize(), 4)
                while True:
                    try:
                        self.assertLess(len(host._diagnostic_queue.get_nowait()), 8400)
                    except queue.Empty:
                        break
            finally:
                release.set()
                self.assertTrue(flushed.wait(1))
        self.assertIn("exit=3", written[0])
        self.assertIn("validation failure", written[0])


class WirePeer:
    def __init__(self, controller=None):
        incoming_read, incoming_write = os.pipe()
        outgoing_read, outgoing_write = os.pipe()
        self.input = os.fdopen(incoming_write, "wb", buffering=0)
        self.output = os.fdopen(outgoing_read, "rb", buffering=0)
        self.server_input = os.fdopen(incoming_read, "rb", buffering=0)
        self.server_output = os.fdopen(outgoing_write, "wb", buffering=0)
        self.controller = controller or host.SessionController()
        self.server = host.McpServer(self.controller, self.server_input, self.server_output,
                                     descriptions=host.TOOL_DESCRIPTIONS, format_result=host.tool_result)
        self.reader = LineReader(self.output)
        self.thread = threading.Thread(target=self.server.run, daemon=True)
        self.thread.start()

    def send(self, message):
        self.input.write(protocol.encode_message(message))

    def receive(self):
        return self.reader.messages.get(timeout=3)

    def initialize(self):
        self.send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "protocolVersion": "2025-11-25", "capabilities": {}, "clientInfo": {"name": "test", "version": "1"}}})
        result = self.receive()["result"]
        self.send({"jsonrpc": "2.0", "method": "notifications/initialized"})
        return result

    def close(self):
        self.input.close()
        self.thread.join(timeout=3)
        for stream in (self.server_output, self.server_input, self.output):
            stream.close()


class McpTests(unittest.TestCase):
    def setUp(self):
        self.peer = WirePeer()

    def tearDown(self):
        self.peer.close()

    def test_initialization_tools_and_no_unsupported_capabilities(self):
        self.peer.send({"jsonrpc": "2.0", "id": 0, "method": "tools/list"})
        self.assertEqual(self.peer.receive()["error"]["code"], -32602)
        result = self.peer.initialize()
        self.assertFalse(result["capabilities"]["tools"].get("listChanged", False))
        self.assertFalse(set(result["capabilities"]) - {"tools", "experimental"})
        self.assertEqual(result["protocolVersion"], protocol.MCP_PROTOCOL_VERSION)
        self.peer.send({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
        tools = self.peer.receive()["result"]["tools"]
        self.assertEqual({t["name"] for t in tools}, set(protocol.INPUT_SCHEMAS))
        self.assertTrue(all("outputSchema" in t for t in tools))
        self.peer.send({"jsonrpc": "2.0", "id": 3, "method": "ping"})
        self.assertEqual(self.peer.receive()["result"], {})

    def test_protocol_and_domain_errors_are_distinct(self):
        self.peer.initialize()
        self.peer.send({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {
            "name": "ui_session", "arguments": arguments(op="start", fixture="apartment-stairs", environment="x")}})
        response = self.peer.receive()["result"]
        self.assertTrue(response["isError"])
        self.assertEqual(response["structuredContent"]["error"]["code"], "environment_not_authorized")
        self.assertEqual(json.loads(response["content"][0]["text"]), response["structuredContent"])
        self.peer.send({"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {
            "name": "ui_session", "arguments": arguments(op="status", unknown=True)}})
        self.assertEqual(self.peer.receive()["error"]["code"], -32602)
        self.peer.send({"jsonrpc": "2.0", "id": 4, "method": "arbitrary/launch"})
        self.assertEqual(self.peer.receive()["error"]["code"], -32601)

    def test_malformed_json_closes_channel_with_protocol_diagnostic(self):
        self.peer.input.write(b'{"jsonrpc":"2.0","id":1,"id":2}\n')
        self.peer.thread.join(timeout=3)
        self.assertFalse(self.peer.thread.is_alive())
        self.assertTrue(self.peer.reader.messages.empty())

    def test_invalid_cancellation_and_tool_name_cannot_crash_server(self):
        self.peer.initialize()
        self.peer.send({"jsonrpc": "2.0", "method": "notifications/cancelled", "params": {"requestId": {}}})
        self.peer.send({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": [], "arguments": {}}})
        self.assertEqual(self.peer.receive()["error"]["code"], -32602)
        self.peer.send({"jsonrpc": "2.0", "id": 3, "method": "ping"})
        self.assertEqual(self.peer.receive()["result"], {})

    def test_metadata_and_version_negotiation(self):
        self.peer.send({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
            "protocolVersion": "older-version", "capabilities": {}, "_meta": {},
            "clientInfo": {"name": "test", "version": "1"}}})
        self.assertEqual(self.peer.receive()["result"]["protocolVersion"], protocol.MCP_PROTOCOL_VERSION)
        self.peer.send({"jsonrpc": "2.0", "method": "notifications/initialized", "params": {"_meta": {}}})
        self.peer.send({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {
            "name": "ui_session", "arguments": arguments(op="status"), "_meta": {"progressToken": "optional"}}})
        self.assertFalse(self.peer.receive()["result"]["isError"])

    def test_full_response_budget_becomes_bounded_domain_failure(self):
        request = arguments(session_id="s", projection="diagnostics")
        result = {"protocol_version": 1, "request_id": "1", "session_id": "s",
                  "build_fingerprint": "test-build", "ok": True,
                  "snapshot": FakeChild.stamp(), "next_cursor": None, "truncated": False,
                  "values": [{"projection": "diagnostics", "field": "message", "object_ref": None,
                              "source": "diagnostics", "value": {"availability": "known",
                              "type": "string", "value": "x" * 14000}}] * 40}
        response = host.tool_result(self.peer.controller, "app_inspect", request, result)
        self.assertTrue(response["isError"])
        self.assertEqual(response["structuredContent"]["error"]["code"], "limit_exceeded")
        self.assertLess(len(protocol.encode_message({"jsonrpc": "2.0", "id": 1, "result": response})), protocol.MAX_MESSAGE_BYTES)
        artifact = Path(response["structuredContent"]["diagnostic_path"])
        try:
            self.assertEqual(json.loads(artifact.read_bytes()), result)
            self.assertLessEqual(artifact.stat().st_size, protocol.MAX_MESSAGE_BYTES)
        finally:
            artifact.unlink()


class ClientTests(unittest.TestCase):
    def test_real_stdio_client_keeps_runtime_request_transcript_without_editor(self):
        root = Path(__file__).resolve().parents[2]
        with tempfile.TemporaryDirectory() as directory:
            transcript = Path(directory) / "transcript.jsonl"
            completed = subprocess.run([sys.executable, "-B", str(root / "scripts/editor_ui_client.py"),
                                        "--transcript", str(transcript)],
                input=protocol.encode_message({"tool": "ui_session", "arguments": arguments(op="status")}),
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=10,
                creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            self.assertEqual(completed.returncode, 0, completed.stderr.decode())
            response = json.loads(completed.stdout)
            self.assertEqual(response["result"]["structuredContent"]["state"], "closed")
            records = [json.loads(line) for line in transcript.read_bytes().splitlines()]
            self.assertEqual(records[0]["message"]["method"], "initialize")
            self.assertEqual(records[-1]["direction"], "response")


if __name__ == "__main__":
    unittest.main()
