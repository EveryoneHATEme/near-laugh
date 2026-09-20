"""SDK client -> stdio server -> real editor acceptance, no screenshots/input.

Requires explicit authorization for the named test desktop. The normal run
uses the production host. Fault runs use a test-only wrapper and real child.
"""

import argparse
import asyncio
import hashlib
import json
from pathlib import Path
import sys
import traceback

from mcp import Client
from mcp.client.stdio import StdioServerParameters, stdio_client
from mcp.shared.exceptions import MCPError

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import editor_ui_protocol as protocol
from editor_ui_client import Transcript, host_parameters


def condition(value):
    return {"source": "app", "projection": "document", "field": "ambient_intensity",
            "predicate": "equals", "expected": value}


async def scenario(args, transcript, fault=None):
    parameters = host_parameters(args.environment)
    if fault:
        parameters = StdioServerParameters(command=sys.executable, args=["-B",
            str(Path(__file__).with_name("editor_ui_fault_host.py")),
            "--environment", args.environment, "--fault", fault])
    if args.disable_implicit_layers:
        # Child-local loader setting, retaining explicit Khronos validation.
        parameters.env = {"VK_LOADER_LAYERS_DISABLE": "~implicit~"}
    session = None
    sequence = 0
    diagnostics = None
    summary = {"mode": fault or "normal"}
    stderr_path = args.transcript.with_suffix(f".{fault or 'normal'}.stderr.log")
    with stderr_path.open("x", encoding="utf-8") as errors:
        async with Client(stdio_client(parameters, errlog=errors), mode="legacy",
                          read_timeout_seconds=70) as client:
            transcript.record("connection", {"mode": fault or "normal", "protocolVersion": client.protocol_version,
                              "serverInfo": client.server_info.model_dump(by_alias=True, exclude_none=True)})
            tools = (await client.list_tools()).tools
            assert {tool.name for tool in tools} == set(protocol.INPUT_SCHEMAS)
            for tool in tools:
                assert tool.input_schema == protocol.INPUT_SCHEMAS[tool.name]
                assert tool.output_schema == protocol.OUTPUT_SCHEMAS[tool.name]

            async def call(tool, arguments):
                nonlocal sequence, diagnostics
                sequence += 1
                arguments = {"protocol_version": 1, "request_id": f"read-{sequence}", **arguments}
                if session:
                    arguments["session_id"] = session
                transcript.record("request", {"method": "tools/call", "params": {"name": tool, "arguments": arguments}})
                try:
                    result = await client.call_tool(tool, arguments)
                except MCPError as error:
                    transcript.record("protocol_error", error.error.model_dump(by_alias=True, exclude_none=True))
                    raise
                transcript.record("response", result.model_dump(by_alias=True, exclude_none=True))
                data = result.structured_content
                protocol.validate_result(tool, data, request=arguments)
                assert json.loads(result.content[0].text) == data
                assert result.is_error == (not data["ok"])
                diagnostics = data.get("diagnostic_path", diagnostics)
                return data

            async def inspect():
                data = await call("app_inspect", {"projection": "document", "fields": ["ambient_intensity", "dirty"]})
                assert data["ok"], data
                return {value["field"]: value["value"]["value"] for value in data["values"]}

            async def find(key):
                data = await call("ui_observe", {"scope": "root", "depth": 16, "filter": {"key": key}})
                assert data["ok"] and len(data["items"]) == 1, data
                return {"ref": data["items"][0]["ref"]}

            async def wait_running(request_id):
                async with asyncio.timeout(10):
                    while True:
                        status = await call("ui_session", {"op": "status"})
                        if (status["state"] == "executing" and
                                status["last_request"]["request_id"] == request_id):
                            return
                        await asyncio.sleep(.01)

            started = await call("ui_session", {"op": "start", "fixture": "apartment-stairs",
                                                "environment": args.environment})
            if fault == "protocol":
                assert not started["ok"] and started["error"]["code"] == "version_mismatch", started
                summary.update(result="passed", error=started["error"]["code"], diagnostic_path=diagnostics,
                               injection="changed protocol_version on the real child's hello before host validation")
                return summary
            assert started["ok"] and started["state"] == "ready", started
            session = started["session_id"]
            summary["build_fingerprint"] = started["build_fingerprint"]
            try:
                before = await inspect()
                if fault == "crash":
                    result = await call("ui_execute", {"request_id": "1", "steps": [
                        {"op": "wait_until", "timeout_ms": 30000, "condition": condition(99)},
                        {"op": "activate", "target": await find("zero-ambient")}]})
                    assert not result["ok"] and result["error"]["code"] == "disconnected", result
                    assert result["effects"] == "unknown" and all(s["status"] == "unknown" for s in result["steps"]), result
                    assert result["cleanup"] == "process_terminated", result
                    summary.update(result="passed", error="disconnected", effects="unknown",
                                   injection="terminated only the real Popen-owned editor during a pending batch")
                else:
                    zero = await find("zero-ambient")
                    ambient = await find("ambient-0-to-0-20")
                    first = {"request_id": "1", "steps": [
                        {"op": "activate", "target": zero},
                        {"op": "assert", "condition": condition(0)}],
                        "policy": {"auto_focus": True, "auto_scroll": True}}
                    result = await call("ui_execute", first)
                    assert result["ok"] and [s["status"] for s in result["steps"]] == ["passed", "passed"], result
                    assert (await inspect())["ambient_intensity"] == 0
                    assert await call("ui_execute", first) == result, "Duplicate execution did not return retained result"
                    failed = await call("ui_execute", {"request_id": "2", "steps": [
                        {"op": "edit", "target": ambient, "value": .081, "commit": "enter"},
                        {"op": "assert", "condition": condition(.099)},
                        {"op": "activate", "target": zero}],
                        "policy": {"auto_focus": True, "auto_scroll": True}})
                    assert not failed["ok"] and failed["error"]["code"] == "assertion_failed", failed
                    assert [s["status"] for s in failed["steps"]] == ["passed", "failed", "not_run"], failed
                    assert failed["effects"] == "partial" and failed["cleanup"] == "released", failed
                    applied = await inspect()
                    assert abs(applied["ambient_intensity"] - .081) < 1e-6 and applied["dirty"], applied
                    try:
                        await call("ui_execute", {"request_id": "3", "steps": [{"op": "launch-shell"}]})
                    except MCPError as error:
                        assert error.code == -32602
                    else:
                        raise AssertionError("Invalid action was accepted")
                    assert await inspect() == applied
                    timeout = await call("ui_execute", {"request_id": "3", "steps": [
                        {"op": "wait_until", "timeout_ms": 100, "condition": condition(99)}]})
                    assert not timeout["ok"] and timeout["error"]["code"] == "timeout", timeout
                    waiting = {"request_id": "4", "steps": [
                        {"op": "wait_until", "timeout_ms": 30000, "condition": condition(99)},
                        {"op": "activate", "target": zero}]}
                    pending = asyncio.create_task(call("ui_execute", waiting))
                    await wait_running("4")
                    cancelled = await call("ui_session", {"op": "cancel", "active_request_id": "4"})
                    assert cancelled["ok"], cancelled
                    result = await pending
                    assert not result["ok"] and result["error"]["code"] == "cancelled", result
                    assert result["steps"][1]["status"] == "not_run" and result["cleanup"] == "released"
                    # The SDK sends notifications/cancelled when its caller is
                    # cancelled; no handwritten notification is used here.
                    waiting = {**waiting, "request_id": "5"}
                    pending = asyncio.create_task(call("ui_execute", waiting))
                    await wait_running("5")
                    pending.cancel()
                    try:
                        await pending
                    except asyncio.CancelledError:
                        pass
                    async with asyncio.timeout(5):
                        while (await call("ui_session", {"op": "status"}))["state"] == "executing":
                            await asyncio.sleep(.01)
                    retained = await call("ui_execute", waiting)
                    assert not retained["ok"] and retained["error"]["code"] == "cancelled", retained
                    assert (await inspect()) == applied
                    summary.update(result="passed", before=before, after=applied,
                        middle_failure=[s["status"] for s in failed["steps"]], duplicate="retained",
                        invalid_request="rejected", timeout="passed", cancel="passed", sdk_cancellation="passed")
            finally:
                closed = await call("ui_session", {"op": "close"})
                if fault == "crash":
                    assert not closed["ok"] and closed["error"]["code"] == "engine_error", closed
                    assert closed["error"]["cleanup"] == "process_terminated", closed
                    assert (await call("ui_session", {"op": "status"}))["state"] == "closed"
                else:
                    assert closed["ok"], closed
                summary["diagnostic_path"] = diagnostics
    if not fault:
        log = Path(diagnostics).read_text(encoding="utf-8", errors="replace")
        assert "Vulkan validation: enabled" in log, log[-2048:]
        assert "Automation Vulkan validation errors after teardown: 0" in log, log[-2048:]
        summary["vulkan_teardown_errors"] = 0
    return summary


async def run(args):
    transcript = Transcript(args.transcript)
    fixture = ROOT / "resources/levels/apartment-stairs.level.json"
    before = hashlib.sha256(fixture.read_bytes()).digest()
    summary = {"sdk": "mcp==2.2.0", "visual": "not_run", "scenarios": [],
               "disabled_layers": "~implicit~" if args.disable_implicit_layers else None}
    status = 0
    try:
        for fault in (None, "protocol", "crash"):
            summary["scenarios"].append(await scenario(args, transcript, fault))
    except Exception as error:
        summary.update(result="failed", error=str(error)[:2048])
        error_path = args.transcript.with_suffix(".error.log")
        with error_path.open("x", encoding="utf-8") as output:
            traceback.print_exc(file=output)
        summary["diagnostic_path"] = str(error_path)
        status = 1
    finally:
        transcript.close()
        assert hashlib.sha256(fixture.read_bytes()).digest() == before, "Source fixture changed"
        with args.transcript.with_suffix(".summary.json").open("x", encoding="utf-8") as output:
            json.dump(summary, output, ensure_ascii=False, indent=2)
    print(json.dumps(summary, ensure_ascii=False))
    return status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--transcript", required=True, type=Path)
    parser.add_argument("--disable-implicit-layers", action="store_true",
                        help="Disable external implicit overlays only in the test child's environment")
    return asyncio.run(run(parser.parse_args()))


if __name__ == "__main__":
    raise SystemExit(main())
