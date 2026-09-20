"""Authorized construction/controller-loss/real File-Exit acceptance.

Only the child found beneath this client's own host may be watched or stopped.
No input is injected into the desktop; File Exit uses the normal semantic UI.
"""
from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import time

from real_editor_acceptance import Client
from real_editor_lifecycle import owned_editor_window


def run(client, environment, mode):
    started = client.tool("ui_session", {"op": "start", "fixture": "apartment-stairs", "environment": environment})
    if mode == "construction":
        assert not started["ok"] and started["error"]["code"] == "environment_unavailable", started
        assert started["error"]["cleanup"] in ("released", "process_terminated"), started
        return {"shutdown": "passed", "mode": mode, "visual": "not_run", "error": started["error"]}
    assert started["ok"], started
    client.session = started["session_id"]
    _, _, pid = owned_editor_window(client.process.pid)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel.OpenProcess(0x100000 | 0x1000, False, pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    result = None
    frame_interval = None
    before = time.monotonic()
    try:
        if mode == "file-exit":
            frame_interval = client.sample_frame_interval()
            client.action("file", "open", "menu")
            item = client.control("exit", "menu_item")
            client.execution += 1
            result = client.tool("ui_execute", {"request_id": str(client.execution),
                "steps": [{"op": "activate", "target": {"ref": item["ref"]}}]})
            assert result["cleanup"] in ("released", "process_terminated"), result
            assert result["ok"] or result["error"]["code"] in ("cancelled", "disconnected"), result
            client.session = None
        else:
            ambient = client.control("ambient-0-to-0-20", "number")
            client.execution += 1
            client.begin_tool("ui_execute", {"request_id": str(client.execution), "steps": [
                {"op": "edit", "target": {"ref": ambient["ref"]}, "value": .081, "commit": "enter"},
                {"op": "wait_until", "timeout_ms": 30000, "condition": {"source": "app", "projection": "document",
                    "field": "dirty", "predicate": "equals", "expected": False}}]})
            deadline = time.monotonic() + 10
            while True:
                applied = client.document()
                if applied["dirty"]["value"]:
                    break
                assert time.monotonic() < deadline, applied
                time.sleep(.01)
            assert abs(applied["ambient_intensity"]["value"] - .081) < 1e-6
            if mode == "host-death":
                client.process.kill()  # Exact Popen-owned host, whose Job owns the editor.
            else:
                client.writer.finish()
                client.process.stdin.close()
            client.process.wait(timeout=5)
            client.session = None
        assert kernel.WaitForSingleObject(handle, 5000) == 0, "Owned editor survived bounded shutdown"
        code = wintypes.DWORD()
        assert kernel.GetExitCodeProcess(handle, ctypes.byref(code))
        if mode == "file-exit":
            assert code.value == 0, code.value
        return {"shutdown": "passed", "mode": mode, "visual": "not_run", "owned_pid": pid,
                "child_exit_code": code.value, "elapsed_ms": (time.monotonic() - before) * 1000,
                "execution_result": result, "frame_interval": frame_interval,
                "maximum_response_bytes": client.maximum_response, "round_trip_ms": client.timings}
    finally:
        kernel.CloseHandle(handle)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--transcript", required=True, type=Path)
    parser.add_argument("--mode", required=True, choices=("construction", "file-exit", "disconnect", "host-death"))
    args = parser.parse_args()
    previous = os.environ.get("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE")
    if args.mode == "construction":
        os.environ["NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"] = "editor-imgui-backend"
    try:
        client = Client(args.environment, args.transcript)
    finally:
        if args.mode == "construction":
            if previous is None:
                os.environ.pop("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", None)
            else:
                os.environ["NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE"] = previous
    summary = {"shutdown": "failed", "mode": args.mode, "visual": "not_run"}
    status = 0
    try:
        summary = run(client, args.environment, args.mode)
    except Exception as error:
        status = 1
        summary["error"] = str(error)[:2048]
    finally:
        try:
            summary["close_result"] = client.close(require_ok=False)
            audit = summary["process_audit"] = client.child_exit_audit()
            if args.mode == "construction" and summary["shutdown"] == "passed":
                assert audit["exit_reports"] == 1 and audit["child_exit_code"] == 2, audit
                assert audit["forced"] is False, audit
                assert audit["final_vulkan_errors"] == 0, audit
            elif args.mode in ("file-exit", "disconnect") and summary["shutdown"] == "passed":
                assert audit["exit_reports"] == 1 and audit["child_exit_code"] == 0, audit
                assert audit["forced"] is False and audit["cleanup"] == "released", audit
                assert audit["final_vulkan_errors"] == 0, audit
        except Exception as error:
            status = 1
            summary["shutdown"] = "failed"
            summary["cleanup_error"] = str(error)[:512]
        with args.transcript.with_suffix(".summary.json").open("x", encoding="utf-8") as output:
            json.dump(summary, output, indent=2)
    print(json.dumps(summary))
    return status


if __name__ == "__main__":
    raise SystemExit(main())
