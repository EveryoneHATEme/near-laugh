"""Authorized-window lifecycle test; affects only this client's owned editor.

Uses Win32 window lifecycle calls, never keyboard/mouse injection or captures.
No arbitrary PID/window command is exposed by the automation protocol.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import time

from real_editor_acceptance import Client


def owned_editor_window(host_pid):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    user = ctypes.WinDLL("user32", use_last_error=True)

    class Entry(ctypes.Structure):
        _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD),
                    ("th32ProcessID", wintypes.DWORD), ("th32DefaultHeapID", ctypes.c_size_t),
                    ("th32ModuleID", wintypes.DWORD), ("cntThreads", wintypes.DWORD),
                    ("th32ParentProcessID", wintypes.DWORD), ("pcPriClassBase", wintypes.LONG),
                    ("dwFlags", wintypes.DWORD), ("szExeFile", wintypes.WCHAR * 260)]
    kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel.Process32FirstW.argtypes = kernel.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(Entry)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    snapshot = kernel.CreateToolhelp32Snapshot(2, 0)
    if snapshot == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    children = []
    try:
        entry = Entry(); entry.dwSize = ctypes.sizeof(entry)
        more = kernel.Process32FirstW(snapshot, ctypes.byref(entry))
        while more:
            if entry.th32ParentProcessID == host_pid and entry.szExeFile.lower() == "level_editor_automation.exe":
                children.append(entry.th32ProcessID)
            more = kernel.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel.CloseHandle(snapshot)
    assert len(children) == 1, children
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    windows = []

    @callback_type
    def visit(window, _):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(window, ctypes.byref(owner))
        if owner.value == children[0] and user.IsWindowVisible(window):
            windows.append(window)
        return True
    user.EnumWindows(visit, 0)
    assert len(windows) == 1, windows
    user.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int,
                                 ctypes.c_int, ctypes.c_int, wintypes.UINT]
    user.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
    user.IsIconic.argtypes = [wintypes.HWND]
    return user, windows[0], children[0]


def run(client, environment):
    started = client.tool("ui_session", {"op": "start", "fixture": "apartment-stairs", "environment": environment})
    assert started["ok"], started
    client.session = started["session_id"]
    user, window, pid = owned_editor_window(client.process.pid)
    initial = client.document()
    for width, height in ((1280, 720), (1600, 900), (1000, 700), (1600, 900)):
        assert user.SetWindowPos(window, None, 0, 0, width, height, 0x16)
        assert client.observe()
        assert client.document()["revision"] == initial["revision"]
    user.ShowWindow(window, 6)
    deadline = time.monotonic() + 3
    while not user.IsIconic(window) and time.monotonic() < deadline:
        time.sleep(.01)
    assert user.IsIconic(window)
    unavailable = client.tool("ui_observe", {"scope": "root"})
    assert not unavailable["ok"] and unavailable["error"]["code"] == "ui_unavailable", unavailable
    assert unavailable["error"]["snapshot"]["stale"]
    user.ShowWindow(window, 9)
    deadline = time.monotonic() + 3
    while user.IsIconic(window) and time.monotonic() < deadline:
        time.sleep(.01)
    assert not user.IsIconic(window)
    assert client.observe()
    assert client.document()["revision"] == initial["revision"]
    client.execution += 1
    active = str(client.execution)
    ambient = client.control("ambient-0-to-0-20", "number")
    pending = client.begin_tool("ui_execute", {"request_id": active, "timeout_ms": 30000,
        "policy": {"auto_scroll": True, "auto_focus": True},
        "steps": [{"op": "edit", "target": {"ref": ambient["ref"]}, "value": .081, "commit": "enter"},
                  {"op": "wait_until", "timeout_ms": 30000, "condition": {
            "source": "app", "projection": "document", "field": "dirty", "predicate": "equals", "expected": False}}]})
    deadline = time.monotonic() + 10
    while True:
        applied = client.document()
        if applied["dirty"]["value"]:
            break
        assert time.monotonic() < deadline, applied
        time.sleep(.01)
    assert abs(applied["ambient_intensity"]["value"] - .081) < 1e-6
    user.ShowWindow(window, 6)
    cancellation = client.tool("ui_session", {"op": "cancel", "active_request_id": active})
    assert cancellation["ok"], cancellation
    result = client.end_tool(pending)
    assert not result["ok"], result
    assert result["error"]["code"] in ("cancelled", "ui_unavailable", "timeout"), result
    assert result["cleanup"] in ("unverified", "process_terminated"), result
    assert result["effects"] == "unknown", result
    assert result["after"] is None or result["after"]["stale"], result
    return {"lifecycle": "passed", "visual": "not_run", "owned_pid": pid,
            "cancel_cleanup": result["cleanup"], "cancel_code": result["error"]["code"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--transcript", required=True, type=Path)
    args = parser.parse_args()
    client = Client(args.environment, args.transcript)
    summary = {"lifecycle": "failed", "visual": "not_run"}
    status = 0
    try:
        summary = run(client, args.environment)
    except Exception as error:
        status = 1; summary["error"] = str(error)[:2048]
    finally:
        try:
            summary["close_result"] = client.close(require_ok=False)
        except Exception as error:
            status = 1; summary["cleanup_error"] = str(error)[:512]
        with args.transcript.with_suffix(".summary.json").open("x", encoding="utf-8") as output:
            json.dump(summary, output, indent=2)
    print(json.dumps(summary))
    return status


if __name__ == "__main__":
    raise SystemExit(main())
