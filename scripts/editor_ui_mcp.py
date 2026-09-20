"""Local stdio MCP host for one explicitly authorized disposable editor.

Run ``python -B scripts/editor_ui_mcp.py --environment <operator-profile>``.
Omitting --environment permits protocol discovery/status but denies startup.
Only the fixed repository build, resource package and fixture manifest are used.
No socket, arbitrary executable argument, shell, PID attachment or reset setter
is exposed. All stdout bytes are newline JSON-RPC; diagnostics use stderr.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from collections import deque
import json
import os
from pathlib import Path
import queue
import shutil
import sys
import tempfile
import threading
import time
import uuid

import editor_ui_protocol as protocol
from editor_ui_process import LineReader, LineWriter, OwnedProcess, ProcessExit

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = {"apartment-stairs": "apartment-stairs.level.json",
            "household-interactions": "household-interactions.level.json"}
EXCLUDED = ["viewport_picking", "placement", "sculpting", "navigation", "gizmos",
            "docking", "os_dialogs", "game_process"]
TOOL_DESCRIPTIONS = {
    "ui_session": (
        "Start/status/cancel/close one owned disposable editor; start requires an operator-authorized "
        "environment and fixture ID. Never attaches to an existing editor. Ready means a completed UI frame; "
        "faulted requires close/new start. Status stays available during execution. Cancel targets the "
        "execution request_id (distinct from the MCP ID). Diagnostic artifact paths survive close."),
    "ui_observe": (
        "Passively observe a bounded page from a completed editor frame; use next_cursor for that snapshot. "
        "No focus, scrolling, opening or editing occurs. Coverage is submitted controls only, not a full UI tree: "
        "closed/collapsed/clipped/unregistered content and truncated values are explicit. Discover refs, "
        "exact selectors, capabilities and commit methods. Draft/input values may differ from applied state."),
    "ui_execute": (
        "Send one ordered batch of up to 64 arbitrary actions/assertions to the editor for execution through "
        "real widgets; no model round trips between steps. The whole shape is validated first, targets resolve "
        "per step. First failure stops the suffix; prior changes remain. Results contain checks, revisions, "
        "assistance and errors, not a full document/UI dump. passed/failed are verified, not_run proves no "
        "execution, unknown means evidence was lost. effects=unknown forbids assuming no change. cleanup "
        "is released/process_terminated/unverified. Use increasing decimal request IDs; an explicit identical "
        "retry can retrieve retained results, but never auto-retry after response loss or claim exactly-once. "
        "Editing alone does not prove application: assert applied state separately. Strict by default; "
        "scroll/focus assistance requires explicit policy. Timeout is bounded; session cancel is available."),
    "app_inspect": (
        "Read a bounded immutable application projection and selected fields; no arbitrary member traversal "
        "or mutation. Document/applied values differ from UI drafts and transient preview values. Availability "
        "known/unknown/unavailable/not_applicable/truncated and snapshot revisions must be respected. "
        "Use field selection and pagination; full-value comparisons cannot use truncated prefixes."),
}


@dataclass
class Pending:
    tool: str
    arguments: dict
    event: threading.Event = field(default_factory=threading.Event)
    result: dict | None = None
    control_deadline: float | None = None


@dataclass
class Closing:
    epoch: int
    deadline: float
    eof: threading.Event = field(default_factory=threading.Event)
    finished: threading.Event = field(default_factory=threading.Event)
    fault: tuple | None = None


_diagnostic_queue = queue.Queue(4)
_diagnostic_thread = None
_diagnostic_start_lock = threading.Lock()


def _report_exit(outcome):
    """Keep a blocked stderr consumer outside the session's cleanup deadline."""
    global _diagnostic_thread

    def write_diagnostics():
        while True:
            text = _diagnostic_queue.get()
            try:
                sys.stderr.write(text)
                sys.stderr.flush()
            except (OSError, ValueError):
                pass

    with _diagnostic_start_lock:
        if _diagnostic_thread is None:
            _diagnostic_thread = threading.Thread(target=write_diagnostics, daemon=True)
            _diagnostic_thread.start()
    text = (f"editor-ui child exit={outcome.exit_code} forced={outcome.forced} "
            f"cleanup={outcome.cleanup}\n" + outcome.diagnostics[-8192:] + "\n")
    try:
        _diagnostic_queue.put_nowait(text)
    except queue.Full:
        pass  # The domain result and last_exit still retain the current evidence.


def _envelope(controller, code, message, *, effects="none", cleanup="released",
              step_index=None, last_completed_index=None):
    snapshot = controller.last_snapshot
    if snapshot is not None:
        snapshot = {**snapshot, "stale": True}
    bounded_message = str(message).encode("utf-8", errors="replace")[:512].decode("utf-8", errors="ignore")
    return {"code": code, "message": bounded_message, "step_index": step_index,
            "target": None, "last_completed_index": last_completed_index,
            "expected": None, "observed": None, "snapshot": snapshot,
            "effects": effects, "cleanup": cleanup, "session_state": controller.state}


class SessionController:
    """Thread-safe owner; constructor seams support tests, never wire options."""

    def __init__(self, *, environment=None, repository_root=ROOT, process_factory=OwnedProcess):
        self.environment = environment
        self.root = Path(repository_root)
        self.process_factory = process_factory
        self.lock = threading.RLock()
        self.state = "closed"
        self.session_id = None
        self.build_fingerprint = "unavailable"
        self.process = None
        self.temp = None
        self.file_slots = []
        self.capabilities = []
        self.last_snapshot = None
        self.last_request = None
        self.pending = {}
        self.running = None
        self.started = self.last_activity = None
        self.ending = threading.Event()
        self._control_sequence = 0
        self._epoch = 0
        self._closing = None
        self._termination = None
        self._launching = None
        self.start_request = None
        self.last_exit = None
        self.diagnostic_path = None
        self.result_artifacts = deque()

    def retain_diagnostic(self, result):
        """At most 32 full result artifacts of at most 1 MiB each per host."""
        data = protocol.encode_message(result)
        with self.lock:
            while len(self.result_artifacts) >= 32:
                # Failed eviction stays accounted for; refuse a new artifact
                # instead of accumulating untracked files held open by a reader.
                Path(self.result_artifacts[0]).unlink(missing_ok=True)
                self.result_artifacts.popleft()
            with tempfile.NamedTemporaryFile(prefix="near-laugh-ui-", suffix=".result.json", delete=False) as output:
                path = output.name
                self.result_artifacts.append(path)
                output.write(data)
        return path

    def _base(self, arguments):
        return {"protocol_version": 1, "request_id": arguments["request_id"],
                "session_id": arguments.get("session_id", self.session_id),
                "build_fingerprint": self.build_fingerprint}

    def failure(self, tool, arguments, code, message, *, effects="none", cleanup="released"):
        with self.lock:
            error = _envelope(self, code, message, effects=effects, cleanup=cleanup)
            result = {**self._base(arguments), "ok": False, "error": error}
            if tool == "ui_execute":
                steps = [
                    {"index": index, "status": "unknown" if effects == "unknown" else "not_run",
                     "before": None, "after": None}
                    for index in range(len(arguments["steps"]))]
                result.update(steps=steps, before=error["snapshot"], after=error["snapshot"],
                              last_completed_index=None, failed_index=None,
                              effects=effects, cleanup=cleanup, session_state=self.state)
            return result

    def status(self, arguments):
        with self.lock:
            return {**self._base(arguments), "ok": True, "state": self.state,
                    "capabilities": list(self.capabilities), "limits": dict(protocol.LIMITS),
                    "file_slots": list(self.file_slots), "last_request": self.last_request,
                    "excluded": list(EXCLUDED)}

    def _check_identity(self, arguments):
        if arguments.get("session_id") != self.session_id:
            raise protocol.ProtocolError("session_closed", "No matching owned session")
        if self.state == "closed":
            raise protocol.ProtocolError("session_closed", "Session is closed")
        if self.state == "faulted":
            raise protocol.ProtocolError("session_faulted", "Close the faulted session before a new start")

    def call(self, tool, arguments, *, cancelled=None):
        protocol.validate_request(tool, arguments)
        try:
            if cancelled is not None and cancelled.is_set():
                raise protocol.ProtocolError("cancelled", "Request cancelled before dispatch")
            if tool == "ui_session" and arguments["op"] == "start":
                return self._start(arguments, cancelled=cancelled)
            with self.lock:
                if tool == "ui_session" and arguments["op"] == "status":
                    if "session_id" in arguments and arguments["session_id"] != self.session_id:
                        raise protocol.ProtocolError("session_closed", "No matching owned session")
                    self.last_activity = time.monotonic()
                    return self.status(arguments)
                if tool == "ui_session" and arguments["op"] == "close":
                    if arguments["session_id"] != self.session_id:
                        raise protocol.ProtocolError("session_closed", "No matching owned session")
                else:
                    self._check_identity(arguments)
                if self.state == "starting" and not (tool == "ui_session" and arguments["op"] == "close"):
                    raise protocol.ProtocolError("busy", "Session startup is in progress")
                if self.state == "closing" and not (tool == "ui_session" and arguments["op"] == "close"):
                    raise protocol.ProtocolError("busy", "Session close is in progress")
                self.last_activity = time.monotonic()
            if tool == "ui_session" and arguments["op"] == "close":
                return self._close(arguments)
            if tool == "ui_execute":
                with self.lock:
                    if cancelled is not None and cancelled.is_set():
                        raise protocol.ProtocolError("cancelled", "Request cancelled before execution")
                    self._check_identity(arguments)
                    if self._closing is not None or self.state == "closing":
                        raise protocol.ProtocolError("busy", "Session close is in progress")
                    if self.running:
                        if arguments["request_id"] == self.running["request_id"]:
                            if arguments == self.running:
                                return {**self._base(arguments), "ok": True, "status": "running",
                                        "session_state": "executing"}
                            raise protocol.ProtocolError("request_id_conflict", "Request ID has a different active payload")
                        raise protocol.ProtocolError("busy", "An execution batch is already running")
                    self.running = arguments
                    self.state = "executing"
                    self.last_request = {"request_id": arguments["request_id"], "status": "running", "error": None}
            try:
                deadline = arguments.get("timeout_ms", protocol.LIMITS["batch_default_ms"])
                if tool != "ui_execute":
                    deadline = protocol.LIMITS["operation_default_ms"]
                if tool == "ui_session" and arguments["op"] == "cancel":
                    deadline = protocol.LIMITS["cleanup_ms"]
                return self._roundtrip(tool, arguments, deadline / 1000)
            finally:
                if tool == "ui_execute":
                    with self.lock:
                        if self.running is arguments:
                            self.running = None
                            if self.state == "executing":
                                self.state = "ready"
        except protocol.ProtocolError as error:
            return self.failure(tool, arguments, error.code, error.message)
        except (OSError, ValueError) as error:
            return self.failure(tool, arguments, "environment_unavailable", str(error))

    def _configuration(self, arguments):
        if self.environment is None or arguments["environment"] != self.environment:
            raise protocol.ProtocolError("environment_not_authorized", "Operator did not authorize this test environment")
        if arguments["fixture"] not in FIXTURES:
            raise protocol.ProtocolError("invalid_request", "Fixture ID is not in the configured manifest")
        binary_dir = self.root / "build" / "debug" / "bin"
        executable = binary_dir / "level_editor_automation.exe"
        metadata = binary_dir / "editor-ui-build.json"
        resources = binary_dir / "resources"
        fixture = resources / "levels" / FIXTURES[arguments["fixture"]]
        if not executable.is_file() or not metadata.is_file() or not fixture.is_file():
            raise protocol.ProtocolError("environment_unavailable", "Automation build, fingerprint or packaged fixture is missing")
        if metadata.stat().st_size > 4096 or fixture.stat().st_size > 16 * 1024 * 1024:
            raise protocol.ProtocolError("limit_exceeded", "Configured metadata/fixture size exceeds its setup limit")
        decoder = protocol.JsonLineDecoder(max_bytes=4097)
        try:
            records = decoder.feed(metadata.read_bytes().rstrip(b"\r\n") + b"\n")
            data = records[0]
            if len(records) != 1 or set(data) != {"build_fingerprint"}:
                raise ValueError("Invalid build metadata")
            protocol.make_handshake(data["build_fingerprint"])
        except (ValueError, KeyError, IndexError) as error:
            raise protocol.ProtocolError("environment_unavailable", "Invalid automation build metadata") from error
        return executable, resources.resolve(), fixture, data["build_fingerprint"]

    def _start(self, arguments, *, cancelled=None):
        with self.lock:
            if cancelled is not None and cancelled.is_set():
                raise protocol.ProtocolError("cancelled", "Startup cancelled before ownership")
            if (self.state != "closed" or self.process is not None or
                    self._termination is not None or self._closing is not None or self._launching is not None):
                raise protocol.ProtocolError("busy", "Close the existing session before starting another")
            self.state = "starting"
            self.start_request = arguments
            self._epoch += 1
            epoch = self._epoch
            self.ending.clear()
            self.last_exit = None
        try:
            executable, resources, fixture, fingerprint = self._configuration(arguments)
            temporary = tempfile.TemporaryDirectory(prefix="near-laugh-ui-")
            owned_root = Path(temporary.name).resolve()
            input_path = owned_root / "input.level.json"
            shutil.copyfile(fixture, input_path)
            slots = [{"slot": "input", "path": str(input_path), "writable": True}]
            for index in range(1, 9):
                output = owned_root / f"output-{index}.level.json"
                slots.append({"slot": f"output-{index}", "path": str(output), "writable": True})
            with self.lock:
                if self._epoch != epoch or self.state != "starting":
                    temporary.cleanup()
                    raise protocol.ProtocolError("cancelled", "Startup ownership expired during fixture preparation")
                self.temp = temporary
                self.file_slots = slots
                self.session_id = uuid.uuid4().hex
                self.build_fingerprint = fingerprint
                self.started = self.last_activity = time.monotonic()
                self.last_snapshot = self.last_request = None
                self.running = None
                launching = self._launching = threading.Event()
            try:
                process = self.process_factory([str(executable), "--automation-pipe"], cwd=executable.parent)
            except BaseException:
                with self.lock:
                    self._launching = None
                    launching.set()
                raise
            with self.lock:
                # Publish ownership even if close arrived during process creation.
                # The close owner or exception cleanup must verify its exit.
                self.process = process
                self.diagnostic_path = getattr(process, "diagnostic_path", None)
                self._launching = None
                launching.set()
                if self.ending.is_set() or self._epoch != epoch:
                    raise protocol.ProtocolError("cancelled", "Startup cancelled before handshake")
            process.send(protocol.make_handshake(fingerprint))
            deadline = self.started + protocol.LIMITS["startup_ms"] / 1000
            while True:
                if time.monotonic() >= deadline:
                    raise protocol.ProtocolError("timeout", "Editor startup handshake timed out")
                if self.ending.is_set() or self._epoch != epoch:
                    raise protocol.ProtocolError("cancelled", "Startup cancelled")
                hello = process.receive()
                if hello is not None:
                    protocol.validate_handshake(hello, expected_build=fingerprint)
                    break
            threading.Thread(target=self._dispatch, args=(process, epoch), daemon=True).start()
            configuration = {"kind": "start", "arguments": arguments,
                             "session_id": self.session_id, "root": str(owned_root),
                             "resource_root": str(resources), "input_path": str(input_path),
                             "file_slots": slots}
            result = self._roundtrip("ui_session", arguments, max(0.001, deadline - time.monotonic()),
                                     message=configuration, expected_epoch=epoch)
            if result["ok"]:
                with self.lock:
                    if self._epoch == epoch and not self.ending.is_set() and self.process is process:
                        self.state = result["state"]
                        self.capabilities = result["capabilities"]
            else:
                # A refusal can be sent before partial-construction teardown
                # and stderr flushing. EOF may already own that same cleanup.
                until = time.monotonic() + protocol.LIMITS["cleanup_ms"] / 1000
                self._terminate("session_faulted", "Session startup was refused",
                                grace=max(0, until - time.monotonic()), expected_epoch=epoch)
                with self.lock:
                    termination = self._termination
                if termination is not None:
                    termination.wait(max(0, until - time.monotonic()))
                with self.lock:
                    outcome = self.last_exit if self._epoch == epoch else None
                    result = {**result, "error": {**result["error"],
                        "cleanup": outcome.cleanup if outcome is not None else "unverified",
                        "session_state": self.state}}
            return result
        except BaseException as error:
            self._terminate("session_faulted", "Session startup failed", expected_epoch=epoch)
            with self.lock:
                outcome = self.last_exit if self._epoch == epoch else None
                outstanding = (self.process is not None or self._termination is not None or
                               self._closing is not None or self._launching is not None)
                if self._epoch == epoch:
                    if not outstanding:
                        self.state = "closed"
                    elif self._closing is None:
                        self.state = "faulted"
                cleanup = "unverified" if outstanding else outcome.cleanup if outcome is not None else "released"
                if isinstance(error, protocol.ProtocolError):
                    return self.failure("ui_session", arguments, error.code, error.message, cleanup=cleanup)
                if isinstance(error, (OSError, ValueError)):
                    return self.failure("ui_session", arguments, "environment_unavailable", str(error), cleanup=cleanup)
            raise

    def _roundtrip(self, tool, arguments, timeout, *, message=None, expected_epoch=None):
        pending = Pending(tool, arguments)
        key = (tool, arguments["request_id"])
        with self.lock:
            epoch = self._epoch
            if expected_epoch is not None and epoch != expected_epoch:
                raise protocol.ProtocolError("session_closed", "Request belongs to an expired session")
            if "session_id" in arguments and arguments["session_id"] != self.session_id:
                raise protocol.ProtocolError("session_closed", "Request belongs to an expired session")
            if self.process is None:
                raise protocol.ProtocolError("session_closed", "Editor process is unavailable")
            if self._closing is not None:
                raise protocol.ProtocolError("busy", "Session close is in progress")
            if key in self.pending:
                raise protocol.ProtocolError("busy", "This tool request is already waiting for a result")
            if len(self.pending) >= 16:
                raise protocol.ProtocolError("limit_exceeded", "Too many pending editor requests")
            self.pending[key] = pending
            process = self.process
        try:
            process.send(message or {"kind": "call", "tool": tool, "arguments": arguments},
                         priority=tool == "ui_session" and arguments["op"] in ("cancel", "close"))
        except (OSError, protocol.ProtocolError):
            self._terminate("disconnected", "Editor request pipe failed", expected_epoch=epoch)
        if not pending.event.wait(timeout):
            if tool == "ui_session" and arguments["op"] in ("cancel", "close"):
                self._terminate("timeout", "Session control did not finish within its cleanup deadline", expected_epoch=epoch)
                return pending.result or self.failure(tool, arguments, "timeout",
                    "Session control expired while close is draining", cleanup="unverified")
            if tool == "ui_execute":
                self.cancel_execution(arguments["request_id"])
            if not pending.event.wait(protocol.LIMITS["cleanup_ms"] / 1000):
                self._terminate("timeout", "Editor did not complete cleanup before its deadline", expected_epoch=epoch)
            elif pending.result and pending.result["ok"]:
                completed = pending.result
                failed = self.failure(tool, arguments, "timeout", "Host request deadline expired",
                                      effects=completed.get("effects", "none"),
                                      cleanup=completed.get("cleanup", "released"))
                if tool == "ui_execute":
                    for field in ("steps", "before", "after", "last_completed_index", "failed_index",
                                  "effects", "cleanup", "session_state"):
                        failed[field] = completed[field]
                    failed["error"]["last_completed_index"] = failed["last_completed_index"]
                return failed
        return pending.result or self.failure(tool, arguments, "session_faulted", "No verified editor result",
                                              effects="unknown", cleanup="unverified")

    def _dispatch(self, process, epoch):
        while True:
            with self.lock:
                if self.process is not process:
                    return
            try:
                message = process.receive()
                if message is None:
                    continue
                with self.lock:
                    if self._epoch != epoch or self.process is not process:
                        return
                if set(message) != {"kind", "tool", "result"} or message["kind"] != "result":
                    raise protocol.ProtocolError("invalid_request", "Malformed editor response envelope")
                result = message["result"]
                if type(result) is not dict:
                    raise protocol.ProtocolError("invalid_request", "Malformed editor result")
                key = (message["tool"], result.get("request_id"))
                with self.lock:
                    pending = self.pending.get(key)
                if pending is None:
                    raise protocol.ProtocolError("invalid_request", "Unmatched editor response")
                protocol.validate_result(pending.tool, result, request=pending.arguments)
                if result["session_id"] != self.session_id or result["build_fingerprint"] != self.build_fingerprint:
                    raise protocol.ProtocolError("version_mismatch", "Editor result identity/fingerprint mismatch")
                with self.lock:
                    self.pending.pop(key, None)
                    stamp = result.get("snapshot") or result.get("after")
                    if stamp is not None:
                        self.last_snapshot = stamp
                    if pending.tool == "ui_execute" and result.get("status") != "running":
                        self.last_request = {"request_id": result["request_id"],
                                             "status": "passed" if result["ok"] else "failed",
                                             "error": result.get("error")}
                        if self._closing is None:
                            self.state = result["session_state"]
                    pending.result = result
                    pending.event.set()
            except (protocol.ProtocolError, OSError, TypeError, KeyError) as error:
                code = error.code if isinstance(error, protocol.ProtocolError) else "engine_error"
                with self.lock:
                    closing = self._closing
                    if closing is not None and closing.epoch == epoch and self.process is process:
                        if code != "disconnected":
                            closing.fault = (code, str(error))
                        closing.eof.set()
                        return  # The close owner verifies actual process exit.
                # Channel teardown can precede CRT/process exit (for example,
                # File -> Exit). Preserve natural exit and final diagnostics
                # within the same bounded cleanup budget used by explicit close.
                grace = protocol.LIMITS["cleanup_ms"] / 1000 if code == "disconnected" else 0
                self._terminate(code, str(error), grace=grace, expected_epoch=epoch)
                return

    def cancel_execution(self, request_id):
        """Priority control message; no mutation retry and no blocking wait."""
        with self.lock:
            if self.process is None or self.running is None:
                return
            self._control_sequence += 1
            arguments = {"protocol_version": 1, "request_id": f"host-cancel-{self._control_sequence}",
                         "session_id": self.session_id, "op": "cancel", "active_request_id": request_id}
            pending = Pending("ui_session", arguments)
            pending.control_deadline = time.monotonic() + protocol.LIMITS["cleanup_ms"] / 1000
            key = ("ui_session", arguments["request_id"])
            if len(self.pending) >= 16:
                self._terminate("limit_exceeded", "No capacity for cancellation cleanup")
                return
            self.pending[key] = pending
            try:
                self.process.send({"kind": "call", "tool": "ui_session", "arguments": arguments}, priority=True)
            except (OSError, protocol.ProtocolError):
                self._terminate("disconnected", "Cancellation pipe failed")

    def _close(self, arguments, *, deadline=None):
        with self.lock:
            if self.state == "closed":
                return self.status(arguments)
            if self._closing is not None:
                return self.failure("ui_session", arguments, "busy", "Session close is already in progress")
            if self._termination is not None:
                return self.failure("ui_session", arguments, "busy", "Owned child termination is still in progress")
            was_starting = self.state == "starting"
            epoch = self._epoch
            until = time.monotonic() + protocol.LIMITS["cleanup_ms"] / 1000
            closing = self._closing = Closing(epoch, min(until, deadline) if deadline is not None else until)
            self.state = "closing"
            self.ending.set()
            process = self.process
            launching = self._launching
            active = [entry for entry in self.pending.values() if entry.tool == "ui_execute"]
        pending = None
        outcome = None
        try:
            if launching is not None:
                launching.wait(max(0, closing.deadline - time.monotonic() - .2))
                with self.lock:
                    process = self.process
                    if self._launching is launching:
                        closing.fault = ("timeout", "Editor process creation did not finish before close")
                        outcome = ProcessExit(None, False, closing.fault[1])
            if process is not None and not was_starting:
                pending = Pending("ui_session", arguments)
                key = ("ui_session", arguments["request_id"])
                with self.lock:
                    if key in self.pending or len(self.pending) >= 16:
                        closing.fault = ("limit_exceeded", "No capacity for close acknowledgement")
                    else:
                        self.pending[key] = pending
                if closing.fault is None:
                    try:
                        process.send({"kind": "call", "tool": "ui_session", "arguments": arguments}, priority=True)
                    except (OSError, protocol.ProtocolError) as error:
                        closing.fault = ("disconnected", str(error))
            if process is not None:
                grace = 0 if was_starting or closing.fault else max(0, closing.deadline - time.monotonic())
                outcome = process.close(grace=grace, deadline=closing.deadline)
                if not was_starting:
                    closing.eof.wait(max(0, closing.deadline - time.monotonic()))
            with self.lock:
                if process is None and outcome is None:
                    outcome = self.last_exit
                missing_execution = any(entry.result is None or
                                        entry.result.get("cleanup") != "released" for entry in active)
                effects = "unknown" if missing_execution else (
                    "partial" if any(entry.result.get("effects") == "partial" for entry in active) else "none")
                fault = closing.fault
                if fault is None and outcome is not None:
                    if outcome.forced or outcome.exit_code is None:
                        fault = ("timeout", "Editor teardown did not finish before the close deadline")
                    elif outcome.exit_code != 0:
                        fault = ("engine_error", f"Editor exited with code {outcome.exit_code} after teardown")
                    elif pending is None or pending.result is None:
                        fault = ("engine_error", "Editor exited without acknowledging close")
                    elif not pending.result["ok"]:
                        fault = (pending.result["error"]["code"], pending.result["error"]["message"])
                    elif missing_execution:
                        fault = ("engine_error", "Editor exited without verified execution cleanup")
                if process is None and self.last_exit is not None and self.last_exit.exit_code is None:
                    fault = ("session_faulted", "Previous child exit remains unverified")
                message = fault[1] if fault else "Session closed"
                if fault and outcome is not None and outcome.diagnostics:
                    prefix = message.encode("utf-8", errors="replace")[:192].decode("utf-8", errors="ignore")
                    tail = outcome.diagnostics.encode("utf-8", errors="replace")[-300:].decode("utf-8", errors="ignore")
                    message = prefix + ": " + tail
            self._terminate(fault[0] if fault else "cancelled", message,
                            expected_epoch=epoch, outcome=outcome, closing_owner=True)
            with self.lock:
                if self._epoch == epoch and (outcome is None or outcome.exit_code is not None):
                    self.state = "closed"
                if fault:
                    cleanup = outcome.cleanup if outcome is not None else "unverified"
                    if cleanup == "released":
                        cleanup = "process_terminated"
                    return self.failure("ui_session", arguments, fault[0], message,
                                        effects=effects, cleanup=cleanup)
                return self.status(arguments)
        finally:
            with self.lock:
                if self._closing is closing:
                    self._closing = None
                closing.finished.set()

    def _terminate(self, code, message, *, grace=0, expected_epoch=None,
                   outcome=None, closing_owner=False):
        with self.lock:
            if expected_epoch is not None and self._epoch != expected_epoch:
                return
            if self._closing is not None and not closing_owner:
                return  # Its absolute deadline and terminal evidence have one owner.
            if self._termination is not None:
                return
            termination = self._termination = threading.Event()
            epoch = self._epoch
            process, self.process = self.process, None
            pending, self.pending = list(self.pending.values()), {}
            self.state = "faulted"
            self.ending.set()
            temporary, self.temp = self.temp, None
            if process is None and self._launching is not None and outcome is None:
                outcome = ProcessExit(None, False, "Editor process creation is still in progress")
            self.capabilities = []
            results = [(entry, self.failure(entry.tool, entry.arguments, code, message,
                                            effects="unknown" if entry.tool == "ui_execute" else "none",
                                            cleanup="unverified")) for entry in pending]
            for entry, result in results:
                if entry.tool == "ui_execute":
                    self.last_request = {"request_id": entry.arguments["request_id"],
                                         "status": "failed", "error": result["error"]}
        if process is not None:
            outcome = outcome if outcome is not None else process.close(grace=grace)
            with self.lock:
                if self._epoch == epoch:
                    self.last_exit = outcome
            _report_exit(outcome)
        cleanup = outcome.cleanup if outcome is not None else "released"
        if process is not None and cleanup == "released":
            cleanup = "process_terminated"  # exit verified, UI cleanup itself was not acknowledged
        if cleanup == "unverified":
            with self.lock:
                if self._epoch == epoch:
                    # Retain ownership for an explicit close retry. A failed
                    # reap is not permission to discard the process or files.
                    # Creation may have published its child after this cleanup
                    # captured an empty process slot. Preserve that ownership.
                    if process is not None:
                        self.process = process
                    self.temp = temporary
        if temporary is not None and cleanup != "unverified":
            try:
                temporary.cleanup()
            except OSError:
                pass  # bounded process cleanup does not claim filesystem cleanup
        for entry, result in results:
            result["error"]["cleanup"] = cleanup
            if entry.tool == "ui_execute":
                result["cleanup"] = cleanup
            entry.result = result
            entry.event.set()
        with self.lock:
            if self._termination is termination:
                self._termination = None
            termination.set()

    def tick(self):
        with self.lock:
            if self.state in ("closed", "faulted", "closing") or self.started is None:
                return
            now = time.monotonic()
            control_expired = any(item.control_deadline is not None and now >= item.control_deadline
                                  for item in self.pending.values())
            expired = now - self.started >= protocol.LIMITS["session_max_ms"] / 1000
            idle = self.running is None and now - self.last_activity >= protocol.LIMITS["idle_session_ms"] / 1000
        if control_expired:
            self._terminate("timeout", "Cancellation acknowledgement deadline expired")
        elif expired or idle:
            self.disconnect("timeout", "Maximum session lifetime expired" if expired else "Idle session expired")

    def disconnect(self, code="disconnected", message="Controller connection closed", *, deadline=None):
        if code == "disconnected":
            deadline = min(deadline if deadline is not None else float("inf"),
                           time.monotonic() + protocol.LIMITS["cleanup_ms"] / 1000)
            with self.lock:
                self._control_sequence += 1
                arguments = {"protocol_version": 1, "request_id": f"host-close-{self._control_sequence}",
                             "session_id": self.session_id, "op": "close"}
            # Close already requests active input release and owns one absolute
            # two-second deadline. Do not stack a separate cancellation wait.
            self._close(arguments, deadline=deadline)
            with self.lock:
                finished = self._closing.finished if self._closing is not None else self._termination
            if finished is not None:
                # EOF may race a close request or the child's own pipe EOF.
                # Keep the host alive for that owner's exit/audit evidence.
                finished.wait(max(0, deadline - time.monotonic()))
            return
        with self.lock:
            epoch = self._epoch
        self._terminate(code, message, expected_epoch=epoch)


def tool_result(controller, tool, arguments, result):
    """Return MCP structured/text forms; budget the complete duplicate payload."""
    if controller.diagnostic_path is not None:
        result = {"diagnostic_path": str(controller.diagnostic_path), **result}
    protocol.validate_result(tool, result)
    content = {"structuredContent": result,
               "content": [{"type": "text", "text": json.dumps(result, ensure_ascii=False,
                                                                      allow_nan=False, separators=(",", ":"))}],
               "isError": not result["ok"]}
    try:
        # Reserve the outer JSON-RPC envelope/ID instead of budgeting only data.
        protocol.encode_message({"jsonrpc": "2.0", "id": "\x01" * 256, "result": content})
        return content
    except protocol.ProtocolError as error:
        if error.code != "limit_exceeded":
            raise
    small = controller.failure(tool, arguments, "limit_exceeded", "MCP result exceeds serialized response limit",
                               effects=result.get("effects", "none"), cleanup=result.get("cleanup", "released"))
    try:
        small["diagnostic_path"] = controller.retain_diagnostic(result)
    except (OSError, protocol.ProtocolError) as error:
        small["error"]["message"] += f"; artifact unavailable: {str(error)[:128]}"
    if tool == "ui_execute" and "steps" in result:
        for key in ("steps", "before", "after", "last_completed_index", "failed_index",
                    "effects", "cleanup", "session_state"):
            small[key] = result[key]
        small["steps"] = []
        for original in result["steps"]:
            step = dict(original)
            step.pop("assistance", None)  # Full evidence is in the bounded artifact.
            if "observed" in step:
                step["observed"] = {"availability": "unavailable", "reason": "response byte budget"}
            if "error" in step:
                step["error"] = {key: value for key, value in step["error"].items()
                                  if key in protocol.ERROR["required"]}
                step["error"].update(expected=None, observed=None)
            small["steps"].append(step)
        small["error"].update(step_index=small["failed_index"],
                               last_completed_index=small["last_completed_index"])
    return tool_result(controller, tool, arguments, small)


from editor_ui_sdk import McpServer


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", help="Operator-authorized test desktop/profile ID for this run")
    args = parser.parse_args(argv)
    if args.environment is not None:
        try:
            protocol.validate_request("ui_session", {"protocol_version": 1, "request_id": "setup",
                "op": "start", "fixture": "apartment-stairs", "environment": args.environment})
        except protocol.ProtocolError as error:
            parser.error(error.message)
    if os.name == "nt":
        import msvcrt
        msvcrt.setmode(sys.stdin.fileno(), os.O_BINARY)
        msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)
    # Unbuffered fd duplicates avoid shutdown flushing a stalled protocol pipe.
    input_stream = os.fdopen(os.dup(sys.stdin.fileno()), "rb", buffering=0)
    output_stream = os.fdopen(os.dup(sys.stdout.fileno()), "wb", buffering=0)
    return McpServer(SessionController(environment=args.environment), input_stream, output_stream,
                     descriptions=TOOL_DESCRIPTIONS, format_result=tool_result).run()


if __name__ == "__main__":
    raise SystemExit(main())
