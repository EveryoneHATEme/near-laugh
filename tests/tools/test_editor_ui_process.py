"""Pipe bounds and real Windows process ownership; no GUI/editor executable."""

import ctypes
from ctypes import wintypes
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

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
from editor_ui_process import LineReader, LineWriter, OwnedProcess, WindowsJob
from editor_ui_protocol import ProtocolError


class PipeTests(unittest.TestCase):
    def test_fragmented_unicode_and_partial_deadline_while_read_is_blocked(self):
        read_fd, write_fd = os.pipe()
        reader_stream = os.fdopen(read_fd, "rb", buffering=0)
        writer_stream = os.fdopen(write_fd, "wb", buffering=0)
        try:
            reader = LineReader(reader_stream)
            wire = '{"text":"Привет"}\n'.encode()
            for byte in wire:
                writer_stream.write(bytes([byte]))
            self.assertEqual(reader.messages.get(timeout=1), {"text": "Привет"})
            writer_stream.write(b'{"unfinished":')
            until = time.monotonic() + 1
            while not reader.decoder.buffered_bytes and time.monotonic() < until:
                time.sleep(0.001)
            with reader._lock:
                reader.decoder._started = time.monotonic() - 6
            self.assertEqual(reader.poll_fault().code, "timeout")
        finally:
            writer_stream.close()
            reader_stream.close()

    def test_input_queue_is_bounded(self):
        reader = LineReader(io.BytesIO(b'{}\n' * 100), capacity=2)
        deadline = time.monotonic() + 1
        while reader.fault is None and time.monotonic() < deadline:
            time.sleep(0.001)
        self.assertEqual(reader.messages.qsize(), 2)
        self.assertEqual(reader.fault.code, "limit_exceeded")

    def test_stalled_writer_does_not_block_caller_and_has_deadline(self):
        release = threading.Event()

        class Sink:
            def write(self, data):
                release.wait(1)
                return len(data)

            def flush(self):
                pass

        writer = LineWriter(Sink(), capacity=1)
        try:
            before = time.monotonic()
            writer.send({"first": 1})
            self.assertLess(time.monotonic() - before, 0.1)
            deadline = time.monotonic() + 1
            while writer._started is None and time.monotonic() < deadline:
                time.sleep(0.001)
            writer.send({"second": 2})
            with self.assertRaises(ProtocolError):
                writer.send({"third": 3})
            writer._started = time.monotonic() - 3
            self.assertEqual(writer.poll_fault().code, "timeout")
        finally:
            release.set()
            writer.finish()

    def test_broken_writer_is_reported(self):
        class Sink:
            def write(self, _data):
                raise BrokenPipeError()

        writer = LineWriter(Sink())
        writer.send({"x": 1})
        until = time.monotonic() + 1
        while writer.fault is None and time.monotonic() < until:
            time.sleep(0.001)
        self.assertEqual(writer.poll_fault().code, "disconnected")

    def test_control_write_has_priority_over_queued_ordinary_calls(self):
        release, started = threading.Event(), threading.Event()
        writes = []

        class Sink:
            def write(self, data):
                if not writes:
                    started.set()
                    release.wait(1)
                writes.append(json.loads(bytes(data)))
                return len(data)

            def flush(self):
                pass

        writer = LineWriter(Sink())
        writer.send({"first": 1})
        self.assertTrue(started.wait(1))
        writer.send({"ordinary": 2})
        writer.send({"cancel": 3}, priority=True)
        release.set()
        self.assertTrue(writer.finish())
        self.assertEqual(writes, [{"first": 1}, {"cancel": 3}, {"ordinary": 2}])


@unittest.skipUnless(os.name == "nt", "Windows Job Object ownership is Windows-only")
class ProcessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.script = self.root / "child.py"
        self.script.write_text(
            "import json,sys,time\n"
            "print(json.dumps({'ready':True}),flush=True)\n"
            "if len(sys.argv)>1 and sys.argv[1]=='unresponsive':\n"
            "    time.sleep(30)\n"
            "else:\n"
            "    for line in sys.stdin.buffer:\n"
            "        sys.stdout.buffer.write(line); sys.stdout.buffer.flush()\n", encoding="utf-8")

    def tearDown(self):
        self.directory.cleanup()

    def test_job_is_noninherited_and_child_termination_is_bounded(self):
        process = OwnedProcess([sys.executable, "-B", str(self.script), "unresponsive"], cwd=self.root)
        try:
            self.assertEqual(process.receive(timeout=1), {"ready": True})
            flags = wintypes.DWORD()
            api = ctypes.WinDLL("kernel32", use_last_error=True)
            api.GetHandleInformation.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
            api.GetHandleInformation.restype = wintypes.BOOL
            self.assertTrue(api.GetHandleInformation(process.job.handle, ctypes.byref(flags)))
            self.assertEqual(flags.value & 1, 0)
            before = time.monotonic()
            outcome = process.close(grace=0.05)
            self.assertEqual(outcome.cleanup, "process_terminated")
            self.assertTrue(outcome.forced)
            self.assertIsNotNone(outcome.exit_code)
            self.assertLess(time.monotonic() - before, 0.5)
            self.assertIsNotNone(process.child.poll())
        finally:
            process.close(grace=0)

    def test_delayed_natural_exit_retains_code_and_final_diagnostics(self):
        for code in (0, 3):
            with self.subTest(code=code):
                self.script.write_text(
                    "import json,sys,time\n"
                    "print(json.dumps({'ready':True}),flush=True)\n"
                    "time.sleep(.25)\n"
                    "sys.stderr.write('FINAL TEARDOWN DIAGNOSTIC');sys.stderr.flush()\n"
                    f"sys.exit({code})\n", encoding="utf-8")
                process = OwnedProcess([sys.executable, "-B", str(self.script)], cwd=self.root)
                try:
                    self.assertEqual(process.receive(timeout=1), {"ready": True})
                    before = time.monotonic()
                    outcome = process.close(grace=2)
                    self.assertGreater(time.monotonic() - before, .1)
                    self.assertLess(time.monotonic() - before, 2.1)
                    self.assertEqual(outcome.exit_code, code)
                    self.assertFalse(outcome.forced)
                    self.assertIn("FINAL TEARDOWN DIAGNOSTIC", outcome.diagnostics)
                    self.assertEqual(process.close(grace=0), outcome)
                finally:
                    process.close(grace=0)

    def test_absolute_close_deadline_includes_kill_and_reap(self):
        process = OwnedProcess([sys.executable, "-B", str(self.script), "unresponsive"], cwd=self.root)
        try:
            self.assertEqual(process.receive(timeout=1), {"ready": True})
            before = time.monotonic()
            outcome = process.close(grace=2, deadline=before + .35)
            self.assertLess(time.monotonic() - before, .5)
            self.assertTrue(outcome.forced)
            self.assertIsNotNone(outcome.exit_code)
        finally:
            process.close(grace=0)

    def test_protocol_eof_before_process_exit_preserves_final_exit_evidence(self):
        for code in (0, 3):
            with self.subTest(code=code):
                self.script.write_text(
                    "import json,os,sys,time\n"
                    "print(json.dumps({'ready':True}),flush=True)\n"
                    "os.close(1)\n"
                    "time.sleep(.1)\n"
                    "sys.stderr.write('AFTER PROTOCOL EOF');sys.stderr.flush()\n"
                    f"os._exit({code})\n", encoding="utf-8")
                process = OwnedProcess([sys.executable, "-B", str(self.script)], cwd=self.root)
                try:
                    self.assertEqual(process.receive(timeout=1), {"ready": True})
                    until = time.monotonic() + 1
                    with self.assertRaises(ProtocolError) as raised:
                        while time.monotonic() < until:
                            process.receive(timeout=.005)
                    self.assertEqual(raised.exception.code, "disconnected")
                    before = time.monotonic()
                    outcome = process.close(grace=2)
                    self.assertLess(time.monotonic() - before, 2.1)
                    self.assertEqual(outcome.exit_code, code)
                    self.assertFalse(outcome.forced)
                    self.assertIn("AFTER PROTOCOL EOF", outcome.diagnostics)
                finally:
                    process.close(grace=0)

    def test_assignment_failure_kills_only_the_new_child(self):
        created = []

        class FailingJob:
            def assign(self, _child):
                raise OSError("injected assignment failure")

            def close(self):
                pass

        def popen(*args, **kwargs):
            process = subprocess.Popen(*args, **kwargs)
            created.append(process)
            return process

        with self.assertRaisesRegex(OSError, "assignment failure"):
            OwnedProcess([sys.executable, "-B", str(self.script)], cwd=self.root,
                         job_factory=FailingJob, popen=popen)
        self.assertIsNotNone(created[0].poll())
        for stream in (created[0].stdin, created[0].stdout, created[0].stderr):
            stream.close()

    def test_child_diagnostics_are_drained_into_a_bounded_tail(self):
        self.script.write_text(
            "import json,sys,time\n"
            "sys.stderr.write('a'*20000+'TAIL');sys.stderr.flush()\n"
            "print(json.dumps({'ready':True}),flush=True)\n"
            "time.sleep(30)\n", encoding="utf-8")
        process = OwnedProcess([sys.executable, "-B", str(self.script)], cwd=self.root)
        try:
            self.assertEqual(process.receive(timeout=1), {"ready": True})
            deadline = time.monotonic() + 1
            while not process.diagnostics().endswith("TAIL") and time.monotonic() < deadline:
                time.sleep(0.001)
            self.assertLessEqual(len(process.diagnostics()), 8192)
            self.assertTrue(process.diagnostics().endswith("TAIL"))
            self.assertEqual(Path(process.diagnostic_path).read_text(encoding="utf-8"), "a" * 20000 + "TAIL")
        finally:
            process.close(grace=0)
            Path(process.diagnostic_path).unlink()

    def test_child_diagnostic_artifact_has_hard_limit_and_truncation_marker(self):
        self.script.write_text(
            "import sys,time\n"
            "sys.stderr.buffer.write(b'a'*(16*1024*1024+8192)+b'TAIL');sys.stderr.flush()\n"
            "print('{\"ready\":true}',flush=True)\n"
            "time.sleep(30)\n", encoding="utf-8")
        process = OwnedProcess([sys.executable, "-B", str(self.script)], cwd=self.root)
        try:
            self.assertEqual(process.receive(timeout=10), {"ready": True})
            deadline = time.monotonic() + 2
            while not process.diagnostics().endswith("TAIL") and time.monotonic() < deadline:
                time.sleep(.005)
            artifact = Path(process.diagnostic_path)
            self.assertEqual(artifact.stat().st_size, 16 * 1024 * 1024)
            with artifact.open("rb") as evidence:
                evidence.seek(-80, 2)
                self.assertIn(b"[diagnostic artifact truncated at 16 MiB]", evidence.read())
            self.assertTrue(process.diagnostics().endswith("TAIL"))
            self.assertLessEqual(len(process.diagnostics()), 8192)
        finally:
            process.close(grace=0)
            Path(process.diagnostic_path).unlink()

    def test_host_crash_kills_responsive_and_unresponsive_children_but_not_unrelated(self):
        scripts = str(Path(__file__).resolve().parents[2] / "scripts")
        launcher = self.root / "host.py"
        launcher.write_text(
            "import json,os,sys,time\n"
            f"sys.path.insert(0,{scripts!r})\n"
            "from editor_ui_process import OwnedProcess\n"
            "child=OwnedProcess([sys.executable,'-B',sys.argv[1],sys.argv[2]],cwd=os.path.dirname(sys.argv[1]))\n"
            "deadline=time.monotonic()+3\n"
            "while time.monotonic()<deadline:\n"
            "    if child.receive(): break\n"
            "print(json.dumps({'pid':child.child.pid}),flush=True)\n"
            "sys.stdin.buffer.read(1)\n"
            "os._exit(19)\n", encoding="utf-8")
        unrelated = subprocess.Popen([sys.executable, "-B", "-c", "import time;time.sleep(30)"],
                                     creationflags=subprocess.CREATE_NO_WINDOW)
        api = ctypes.WinDLL("kernel32", use_last_error=True)
        api.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        api.OpenProcess.restype = wintypes.HANDLE
        api.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        api.WaitForSingleObject.restype = wintypes.DWORD
        api.CloseHandle.argtypes = [wintypes.HANDLE]
        api.CloseHandle.restype = wintypes.BOOL
        try:
            for mode in ("responsive", "unresponsive"):
                with self.subTest(mode=mode):
                    owner = subprocess.Popen([sys.executable, "-B", str(launcher), str(self.script), mode],
                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                        bufsize=0, creationflags=subprocess.CREATE_NO_WINDOW)
                    handle = None
                    try:
                        reader = LineReader(owner.stdout)
                        record = reader.messages.get(timeout=4)
                        handle = api.OpenProcess(0x00100000, False, record["pid"])
                        self.assertTrue(handle)
                        owner.stdin.write(b"x")
                        self.assertEqual(owner.wait(timeout=2), 19)
                        self.assertEqual(api.WaitForSingleObject(handle, 2000), 0)
                        self.assertIsNone(unrelated.poll())
                    finally:
                        if handle:
                            api.CloseHandle(handle)
                        if owner.poll() is None:
                            owner.kill()
                            owner.wait(timeout=2)
                        for stream in (owner.stdin, owner.stdout, owner.stderr):
                            stream.close()
        finally:
            unrelated.kill()
            unrelated.wait(timeout=2)


if __name__ == "__main__":
    unittest.main()
