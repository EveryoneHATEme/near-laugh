"""Prepared MCP -> real editor acceptance; requires an authorized desktop run.

This is an external test client. The host/executor receive ordinary runtime
tool calls and know nothing about this test. No screenshots or desktop input.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import queue
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import editor_ui_protocol as protocol
from editor_ui_process import LineReader, LineWriter


class Client:
    def __init__(self, environment, transcript):
        self.log = transcript.open("xb", buffering=0)
        self.stderr_path = transcript.with_suffix(".stderr.log")
        self.stderr = self.stderr_path.open("xb", buffering=0)
        self.process = subprocess.Popen(
            [sys.executable, "-B", str(ROOT / "scripts/editor_ui_mcp.py"),
             "--environment", environment],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.stderr,
            shell=False, close_fds=True, bufsize=0,
            creationflags=subprocess.CREATE_NO_WINDOW)
        self.reader, self.writer = LineReader(self.process.stdout), LineWriter(self.process.stdin)
        self.sequence = self.execution = 0
        self.session = None
        self.timings = []
        self.maximum_response = 0
        self.log_bytes = 0
        self.responses = {}
        self.starts = {}
        try:
            self.rpc("initialize", {"protocolVersion": protocol.MCP_PROTOCOL_VERSION,
                     "capabilities": {}, "clientInfo": {"name": "near-laugh-acceptance", "version": "1"}})
            self.send({"jsonrpc": "2.0", "method": "notifications/initialized"})
        except BaseException:
            self.close()
            raise

    def record(self, direction, message):
        data = protocol.encode_message({"direction": direction, "monotonic_ms": time.monotonic() * 1000,
                                        "message": message})
        self.log_bytes += len(data)
        if self.log_bytes > 64 * 1024 * 1024:
            raise RuntimeError("Acceptance transcript exceeded its bound")
        self.log.write(data)

    def send(self, message):
        self.record("request", message)
        self.writer.send(message)

    def begin_rpc(self, method, params=None):
        self.sequence += 1
        request = {"jsonrpc": "2.0", "id": self.sequence, "method": method}
        if params is not None:
            request["params"] = params
        self.starts[self.sequence] = (method, time.monotonic())
        self.send(request)
        return self.sequence

    def end_rpc(self, request_id, timeout=70):
        method, begin = self.starts[request_id]
        while time.monotonic() - begin < timeout:
            fault = self.reader.poll_fault() or self.writer.poll_fault()
            if fault:
                raise fault
            response = self.responses.pop(request_id, None)
            if response is None:
                try:
                    response = self.reader.messages.get(timeout=.05)
                except queue.Empty:
                    continue
                self.record("response", response)
                self.maximum_response = max(self.maximum_response, len(protocol.encode_message(response)))
                if response.get("id") != request_id:
                    if len(self.responses) >= 16:
                        raise RuntimeError("Response queue exceeded its bound")
                    self.responses[response.get("id")] = response
                    continue
            if "error" in response:
                raise RuntimeError(json.dumps(response["error"]))
            self.timings.append((method, (time.monotonic() - begin) * 1000))
            del self.starts[request_id]
            return response["result"]
        raise TimeoutError(method)

    def rpc(self, method, params=None, timeout=70):
        return self.end_rpc(self.begin_rpc(method, params), timeout)

    def begin_tool(self, tool, arguments):
        arguments = {"protocol_version": 1, "request_id": f"read-{self.sequence + 1}", **arguments}
        if self.session:
            arguments["session_id"] = self.session
        protocol.validate_request(tool, arguments)
        return self.begin_rpc("tools/call", {"name": tool, "arguments": arguments}), tool, arguments

    def end_tool(self, pending):
        request_id, tool, arguments = pending
        result = self.end_rpc(request_id)
        structured = result["structuredContent"]
        protocol.validate_result(tool, structured, request=arguments)
        assert json.loads(result["content"][0]["text"]) == structured
        assert result.get("isError", False) == (not structured["ok"])
        return structured

    def tool(self, tool, arguments):
        return self.end_tool(self.begin_tool(tool, arguments))

    def observe(self):
        result = self.tool("ui_observe", {"scope": "root", "depth": 16, "page_size": 256})
        assert result["ok"], result
        items = result["items"]
        while result["next_cursor"]:
            result = self.tool("ui_observe", {"cursor": result["next_cursor"]})
            assert result["ok"], result
            items.extend(result["items"])
        return items

    def control(self, key, kind=None):
        matches = [item for item in self.observe() if item["key"] == key and
                   (kind is None or item["kind"] == kind)]
        assert len(matches) == 1, (key, kind, [(item["ref"], item["scope"]) for item in matches])
        return matches[0]

    def execute(self, steps, error=None):
        self.execution += 1
        result = self.tool("ui_execute", {"request_id": str(self.execution), "steps": steps,
                                         "policy": {"auto_scroll": True, "auto_focus": True}})
        if error:
            assert not result["ok"] and result["error"]["code"] == error, result
        else:
            assert result["ok"], result
        return result

    def action(self, key, op="activate", kind=None, **payload):
        item = self.control(key, kind)
        return self.execute([{"op": op, "target": {"ref": item["ref"]}, **payload}])

    def menu(self, menu, item):
        self.action(menu, "open", "menu")
        return self.action(item, kind="menu_item")

    def document(self):
        result = self.tool("app_inspect", {"projection": "document"})
        assert result["ok"], result
        return {value["field"]: value["value"] for value in result["values"]}

    def sample_frame_interval(self):
        samples = []
        for _ in range(12):
            observed = self.tool("ui_observe", {"scope": "root", "depth": 1, "page_size": 1})
            assert observed["ok"] and not observed["snapshot"]["stale"], observed
            samples.append((int(observed["snapshot"]["frame"]), time.monotonic() * 1000))
        frame_delta = samples[-1][0] - samples[0][0]
        elapsed_ms = samples[-1][1] - samples[0][1]
        assert frame_delta > 0, samples
        return {"samples": len(samples), "completed_frames": frame_delta, "elapsed_ms": elapsed_ms,
                "mean_completed_frame_interval_ms": elapsed_ms / frame_delta,
                "measurement": "fresh observation frame counters and client receipt timestamps; not GPU timing"}

    def close(self, require_ok=True):
        close_result = None
        try:
            if self.session and self.process.poll() is None:
                close_result = self.tool("ui_session", {"op": "close"})
        finally:
            self.writer.finish()
            self.process.stdin.close()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.terminate()  # This exact host owns its child Job.
                self.process.wait(timeout=5)
            self.process.stdout.close()
            self.stderr.close()
            self.log.close()
        if require_ok and close_result is not None:
            assert close_result["ok"], close_result
        return close_result

    def child_exit_audit(self):
        diagnostics = self.stderr_path.read_text(encoding="utf-8", errors="replace")
        exits = re.findall(r"editor-ui child exit=(-?\d+|None) forced=(True|False) cleanup=([a-z_]+)", diagnostics)
        errors = re.findall(r"Automation Vulkan validation errors after teardown: (\d+)", diagnostics)
        return {"exit_reports": len(exits),
                "child_exit_code": int(exits[-1][0]) if exits and exits[-1][0] != "None" else None,
                "forced": exits[-1][1] == "True" if exits else None,
                "cleanup": exits[-1][2] if exits else None,
                "final_vulkan_errors": int(errors[-1]) if errors else None}


def run(client, environment):
    manifest = client.rpc("tools/list")
    assert {tool["name"] for tool in manifest["tools"]} == set(protocol.INPUT_SCHEMAS)
    start = client.tool("ui_session", {"op": "start", "fixture": "apartment-stairs", "environment": environment})
    assert start["ok"], start
    client.session = start["session_id"]
    shallow = client.tool("ui_observe", {"scope": "root", "depth": 0, "page_size": 1})
    assert shallow["ok"] and "depth_limit" in shallow["coverage"]["limitations"], shallow
    first = client.tool("ui_observe", {"scope": "root", "depth": 16, "page_size": 1})
    assert first["ok"] and first["next_cursor"] and len(first["items"]) == 1, first
    continuation = client.tool("ui_observe", {"cursor": first["next_cursor"]})
    assert continuation["ok"] and continuation["snapshot"] == first["snapshot"], continuation
    assert continuation["items"][0]["ref"] != first["items"][0]["ref"], continuation
    output = next(slot["path"] for slot in start["file_slots"] if slot["slot"] == "output-1")
    before = client.document()
    ambient = client.control("ambient-0-to-0-20", "number")
    client.execute([{"op": "edit", "target": {"ref": ambient["ref"]}, "text": "0.073", "commit": "none"}])
    draft = client.control("ambient-0-to-0-20", "number")
    assert draft["input"]["text"] == "0.073" and draft["state"]["active"]
    assert client.document()["ambient_intensity"] == before["ambient_intensity"]
    client.execute([{"op": "commit", "target": {"ref": ambient["ref"]}, "method": "enter"}])
    assert abs(client.document()["ambient_intensity"]["value"] - .073) < 1e-6
    client.menu("edit", "undo")
    assert client.document()["ambient_intensity"] == before["ambient_intensity"]
    client.menu("edit", "redo")
    assert abs(client.document()["ambient_intensity"]["value"] - .073) < 1e-6
    client.menu("file", "save-as")
    client.action("path", "edit", "text", text=output, commit="tab")
    client.action("save", kind="button")
    saved = client.document()
    assert saved["slot"]["value"] == "output-1" and not saved["dirty"]["value"]
    client.menu("file", "open")
    client.action("path", "edit", "text", text=output, commit="tab")
    client.action("open", kind="button")
    reopened = client.document()
    assert abs(reopened["ambient_intensity"]["value"] - .073) < 1e-6
    assert reopened["generation"] != saved["generation"]
    client.execute([{"op": "activate", "target": {"ref": ambient["ref"]}}], "stale_ref")
    client.execute([{"op": "activate", "target": {"selector": {"scope": "root", "key": "missing"}}}], "not_found")
    viewport = next(item for item in client.observe() if item["kind"] == "viewport")
    client.execute([{"op": "activate", "target": {"ref": viewport["ref"]}}], "unsupported")
    client.execute([{"op": "activate", "target": {"ref": client.control("play", "button")["ref"]}}], "policy_denied")
    ambient = client.control("ambient-0-to-0-20", "number")
    client.action("file", "open", "menu")
    save = client.control("save", "menu_item")
    file_menu = client.control("file", "menu")
    client.execute([{"op": "close", "target": {"ref": file_menu["ref"]}}])
    saved_digest = hashlib.sha256(Path(output).read_bytes()).hexdigest()
    failed = client.execute([
        {"op": "edit", "target": {"ref": ambient["ref"]}, "value": .081, "commit": "enter"},
        {"op": "assert", "condition": {"source": "app", "projection": "document", "field": "dirty",
                                            "predicate": "equals", "expected": False}},
        {"op": "open", "target": {"ref": file_menu["ref"]}},
        {"op": "activate", "target": {"ref": save["ref"]}},
    ], "assertion_failed")
    assert [step["status"] for step in failed["steps"]] == ["passed", "failed", "not_run", "not_run"]
    assert abs(client.document()["ambient_intensity"]["value"] - .081) < 1e-6
    assert hashlib.sha256(Path(output).read_bytes()).hexdigest() == saved_digest, "Save suffix ran after failed assertion"
    return {"semantic": "passed", "visual": "not_run", "session": client.session,
            "maximum_response_bytes": client.maximum_response, "round_trip_ms": client.timings,
            "coverage": sorted({limit for item in client.observe() for limit in
                                (["unavailable_value"] if item["value"]["availability"] != "known" else [])})}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True, help="Previously authorized desktop/profile for this run")
    parser.add_argument("--transcript", required=True, type=Path)
    args = parser.parse_args()
    source = ROOT / "resources/levels/apartment-stairs.level.json"
    original = hashlib.sha256(source.read_bytes()).hexdigest()
    client = Client(args.environment, args.transcript)
    summary = {"semantic": "failed", "visual": "not_run"}
    status = 0
    try:
        summary = run(client, args.environment)
    except Exception as error:
        status = 1
        summary["error"] = str(error)[:2048]
    finally:
        try:
            summary["close_result"] = client.close()
        except Exception as error:
            status = 1
            summary["cleanup_error"] = str(error)[:512]
        assert hashlib.sha256(source.read_bytes()).hexdigest() == original, "Source fixture was modified"
        with args.transcript.with_suffix(".summary.json").open("x", encoding="utf-8") as output:
            json.dump(summary, output, ensure_ascii=False, indent=2)
    print(json.dumps(summary, ensure_ascii=False))
    return status


if __name__ == "__main__":
    raise SystemExit(main())
