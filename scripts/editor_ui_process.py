"""Bounded pipe workers and ownership of one disposable Windows child.

Only the MCP host supplies the executable/arguments. The injectable command in
OwnedProcess is an internal testing seam, never an MCP argument or attach API.
The Job Object is created first and assigned before callers can send a hello.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
import ctypes
from ctypes import wintypes
import os
import queue
import subprocess
import tempfile
import threading
import time

from editor_ui_protocol import JsonLineDecoder, ProtocolError, encode_message


class WindowsJob:
    def __init__(self):
        if os.name != "nt":
            raise ProtocolError("environment_unavailable", "Automation sessions require Windows")

        class IO_COUNTERS(ctypes.Structure):
            _fields_ = [(name, ctypes.c_ulonglong) for name in (
                "ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

        class BASIC_LIMIT(ctypes.Structure):
            _fields_ = [("PerProcessUserTimeLimit", ctypes.c_longlong),
                        ("PerJobUserTimeLimit", ctypes.c_longlong),
                        ("LimitFlags", wintypes.DWORD),
                        ("MinimumWorkingSetSize", ctypes.c_size_t),
                        ("MaximumWorkingSetSize", ctypes.c_size_t),
                        ("ActiveProcessLimit", wintypes.DWORD),
                        ("Affinity", ctypes.c_size_t),
                        ("PriorityClass", wintypes.DWORD),
                        ("SchedulingClass", wintypes.DWORD)]

        class EXTENDED_LIMIT(ctypes.Structure):
            _fields_ = [("BasicLimitInformation", BASIC_LIMIT),
                        ("IoInfo", IO_COUNTERS),
                        ("ProcessMemoryLimit", ctypes.c_size_t),
                        ("JobMemoryLimit", ctypes.c_size_t),
                        ("PeakProcessMemoryUsed", ctypes.c_size_t),
                        ("PeakJobMemoryUsed", ctypes.c_size_t)]

        self._api = ctypes.WinDLL("kernel32", use_last_error=True)
        signatures = {
            "CreateJobObjectW": ([ctypes.c_void_p, wintypes.LPCWSTR], wintypes.HANDLE),
            "SetInformationJobObject": ([wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p,
                                          wintypes.DWORD], wintypes.BOOL),
            "SetHandleInformation": ([wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD], wintypes.BOOL),
            "AssignProcessToJobObject": ([wintypes.HANDLE, wintypes.HANDLE], wintypes.BOOL),
            "CloseHandle": ([wintypes.HANDLE], wintypes.BOOL),
        }
        for name, (arguments, returns) in signatures.items():
            function = getattr(self._api, name)
            function.argtypes, function.restype = arguments, returns
        self.handle = self._api.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            if not self._api.SetHandleInformation(self.handle, 1, 0):
                raise ctypes.WinError(ctypes.get_last_error())
            limits = EXTENDED_LIMIT()
            limits.BasicLimitInformation.LimitFlags = 0x2000  # KILL_ON_JOB_CLOSE
            if not self._api.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
                raise ctypes.WinError(ctypes.get_last_error())
        except BaseException:
            self.close()
            raise

    def assign(self, child):
        if not self._api.AssignProcessToJobObject(self.handle, wintypes.HANDLE(int(child._handle))):
            raise ctypes.WinError(ctypes.get_last_error())

    def close(self):
        if self.handle:
            self._api.CloseHandle(self.handle)
            self.handle = None


class LineReader:
    """Daemon reader; owner calls poll_fault to enforce silent partial timeout."""

    def __init__(self, stream, *, capacity=32):
        self.stream = stream
        self.messages = queue.Queue(capacity)
        self.decoder = JsonLineDecoder()
        self._lock = threading.Lock()
        self.fault = None
        self.eof = False
        self._thread = threading.Thread(target=self._read, daemon=True)
        self._thread.start()

    def _read(self):
        try:
            read = getattr(self.stream, "read1", self.stream.read)
            while True:
                chunk = read(4096)
                with self._lock:
                    if not chunk:
                        self.decoder.finish()
                        self.eof = True
                        return
                    messages = self.decoder.feed(chunk)
                for message in messages:
                    try:
                        self.messages.put(message, timeout=0.1)
                    except queue.Full as error:
                        raise ProtocolError("limit_exceeded", "Input message queue is full") from error
        except (OSError, ValueError) as error:
            self.fault = error if isinstance(error, ProtocolError) else ProtocolError("disconnected", "Input pipe failed")

    def poll_fault(self):
        with self._lock:
            if not self.fault and not self.eof:
                try:
                    self.decoder.expire()
                except ProtocolError as error:
                    self.fault = error
        return self.fault


class LineWriter:
    """Bounded write queue; slow/broken pipes never block the controller loop."""

    def __init__(self, stream, *, capacity=16):
        self.stream = stream
        self.messages = queue.Queue(capacity)
        self.priority = queue.Queue(4)
        self.fault = None
        self._started = None
        self._closed = False
        self._thread = threading.Thread(target=self._write, daemon=True)
        self._thread.start()

    def send(self, message, *, priority=False):
        self.send_bytes(encode_message(message), priority=priority)

    def send_bytes(self, data, *, priority=False):
        if self._closed or self.fault:
            raise ProtocolError("disconnected", "Output pipe is closed")
        try:
            (self.priority if priority else self.messages).put_nowait((time.monotonic(), data))
        except queue.Full as error:
            raise ProtocolError("limit_exceeded", "Output message queue is full") from error

    def _write(self):
        try:
            while True:
                source = self.priority
                try:
                    entry = source.get_nowait()
                except queue.Empty:
                    source = self.messages
                    try:
                        entry = source.get(timeout=0.01)
                    except queue.Empty:
                        continue
                if entry is None:
                    source.task_done()
                    return
                queued, data = entry
                self._started = queued
                view = memoryview(data)
                while view:
                    written = self.stream.write(view)
                    if written is None or written <= 0:
                        raise OSError("No pipe write progress")
                    view = view[written:]
                self.stream.flush()
                self._started = None
                source.task_done()
        except (OSError, ValueError):
            self.fault = ProtocolError("disconnected", "Output pipe failed")

    def poll_fault(self, deadline=2.0):
        if self._started is not None and time.monotonic() - self._started >= deadline:
            self.fault = ProtocolError("timeout", "Output pipe made insufficient progress")
        return self.fault

    def finish(self, timeout=2.0):
        self._closed = True
        until = time.monotonic() + timeout
        while (self.messages.unfinished_tasks + self.priority.unfinished_tasks) and not self.fault and time.monotonic() < until:
            time.sleep(0.005)
        try:
            self.messages.put_nowait(None)
        except queue.Full:
            pass
        return not self.fault and self.messages.unfinished_tasks + self.priority.unfinished_tasks <= 1


@dataclass(frozen=True)
class ProcessExit:
    """Final owned-process evidence, retained even after handles are closed."""

    exit_code: int | None
    forced: bool
    diagnostics: str

    @property
    def cleanup(self):
        if self.exit_code is None:
            return "unverified"
        return "process_terminated" if self.forced or self.exit_code != 0 else "released"


class OwnedProcess:
    """Own handles, bounded diagnostics and kill-on-close job for one child."""

    def __init__(self, command, *, cwd, job_factory=WindowsJob, popen=subprocess.Popen):
        self.job = job_factory()
        self.child = None
        self.reader = self.writer = None
        self._diagnostics = deque(maxlen=8)
        self._diagnostic_lock = threading.Lock()
        self._close_lock = threading.Lock()
        self._exit = None
        self._forced = False
        self.closed = False
        self.diagnostic_path = None
        self._diagnostic_file = None
        try:
            # A retained per-session artifact, outside the disposable document
            # root. Exclusive creation; capped at 16 MiB including the marker.
            self._diagnostic_file = tempfile.NamedTemporaryFile(
                prefix="near-laugh-ui-", suffix=".stderr.log", delete=False)
            self.diagnostic_path = self._diagnostic_file.name
            flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
            self.child = popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, cwd=str(cwd), shell=False,
                               close_fds=True, bufsize=0, creationflags=flags)
            self.job.assign(self.child)
            self.reader = LineReader(self.child.stdout)
            self.writer = LineWriter(self.child.stdin)
            self._diagnostic_thread = threading.Thread(target=self._read_diagnostics, daemon=True)
            self._diagnostic_thread.start()
        except BaseException:
            if self.child is not None:
                try:
                    self.child.kill()
                except OSError:
                    pass  # closing the already-created job is the second cleanup path
                try:
                    self.child.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    pass
                for pipe in (self.child.stdin, self.child.stdout, self.child.stderr):
                    if pipe is not None:
                        try:
                            pipe.close()
                        except OSError:
                            pass
            self.job.close()
            if self._diagnostic_file is not None:
                self._diagnostic_file.close()
            raise

    def _read_diagnostics(self):
        remaining = 16 * 1024 * 1024
        marker = b"\n[diagnostic artifact truncated at 16 MiB]\n"
        try:
            while chunk := self.child.stderr.read(1024):
                with self._diagnostic_lock:
                    self._diagnostics.append(bytes(chunk))
                if remaining > len(marker):
                    kept = chunk[:remaining - len(marker)]
                    self._diagnostic_file.write(kept)
                    remaining -= len(kept)
                    if remaining == len(marker):
                        self._diagnostic_file.write(marker)
                        remaining = 0
                    self._diagnostic_file.flush()
        except (OSError, ValueError):
            pass
        finally:
            self._diagnostic_file.close()

    def diagnostics(self):
        with self._diagnostic_lock:
            return b"".join(self._diagnostics).decode("utf-8", errors="replace")[-8192:]

    def send(self, message, *, priority=False):
        self.writer.send(message, priority=priority)

    def receive(self, timeout=0.05):
        for worker in (self.reader, self.writer):
            fault = worker.poll_fault()
            if fault:
                raise fault
        try:
            return self.reader.messages.get(timeout=timeout)
        except queue.Empty:
            if self.reader.eof:
                raise ProtocolError("disconnected", "Editor closed its protocol pipe")
            return None

    def close(self, *, grace=2.0, deadline=None):
        """Wait for teardown, then kill/reap within one absolute deadline.

        Reserve 200 ms of the (at most two-second) budget for termination and
        pipe draining. A closing protocol acknowledgement is not exit evidence.
        """
        now = time.monotonic()
        deadline = min(deadline if deadline is not None else now + max(0, grace) + 0.2,
                       now + 2.0)
        if not self._close_lock.acquire(timeout=max(0, deadline - now)):
            return ProcessExit(None, self._forced, self.diagnostics())
        try:
            if self._exit is not None and self._exit.exit_code is not None:
                return self._exit
            self.closed = True
            until = min(now + max(0, grace), deadline - 0.2)
            while self.child.poll() is None and time.monotonic() < until:
                time.sleep(0.005)
            if self.child.poll() is None:
                self._forced = True
                try:
                    self.child.kill()
                except OSError:
                    pass  # a racing exit or the job close below still releases ownership
            self.job.close()
            try:
                self.child.wait(timeout=max(0, deadline - time.monotonic()))
            except subprocess.TimeoutExpired:
                self._exit = ProcessExit(None, self._forced, self.diagnostics())
                return self._exit
            # Read stderr through EOF before taking the diagnostic snapshot.
            # stdout must also be drained so the host can consume final results.
            for thread in (self._diagnostic_thread, self.reader._thread):
                thread.join(timeout=max(0, deadline - time.monotonic()))
            self.writer.finish(timeout=0)
            for pipe in (self.child.stdin, self.child.stdout, self.child.stderr):
                try:
                    pipe.close()
                except (OSError, ValueError):
                    pass
            self._exit = ProcessExit(self.child.returncode, self._forced, self.diagnostics())
            return self._exit
        finally:
            self._close_lock.release()
