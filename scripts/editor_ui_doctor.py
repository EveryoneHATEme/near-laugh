"""Read-only editor-ui prerequisites and official SDK discovery diagnostics.

Exit 0: requested checks passed; 1: failed/blocked check; 2: invalid invocation.
Default invocation opens no editor. A successful discovery does not establish
agent tool visibility, current-source build freshness, desktop or GPU readiness.
"""

import argparse
import asyncio
from importlib import metadata
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import struct
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parents[1]
BUILD_COMMAND = ("cmake --build --preset debug --target level_editor "
                 "level_editor_automation editor_automation_tests -j 4")
REPORT_LIMIT = 1024 * 1024


def runtime_check():
    actual = {"system": platform.system(), "machine": platform.machine(),
              "implementation": platform.python_implementation(),
              "python": platform.python_version(), "bits": struct.calcsize("P") * 8,
              "executable": sys.executable}
    if (actual["system"] != "Windows" or actual["machine"].lower() not in ("amd64", "x86_64")
            or actual["implementation"] != "CPython" or actual["bits"] != 64
            or sys.version_info[:2] != (3, 12)):
        raise ValueError(f"Requires Windows x64 / CPython 3.12: {actual}")
    return actual


def dependencies_check(root=ROOT):
    pins = {}
    for line in (root / "scripts/requirements-editor-ui.txt").read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        match = re.fullmatch(r"([A-Za-z0-9_.-]+)==([^\s;]+)", line)
        if not match or match[1].lower() in pins:
            raise ValueError("Requirements must contain unique exact version pins")
        pins[match[1].lower()] = match[2]
    if not pins:
        raise ValueError("Dependency lock is empty")
    observed, failures = {}, []
    for name, expected in pins.items():
        try:
            actual = metadata.version(name)
        except metadata.PackageNotFoundError:
            actual = "missing"
        observed[name] = {"expected": expected, "observed": actual}
        if actual != expected:
            failures.append(f"{name}: expected {expected}, observed {actual}")
    if failures:
        raise ValueError("; ".join(failures))
    return observed


def prerequisites_check(root=ROOT):
    cache_path = root / "build/debug/CMakeCache.txt"
    cache = {}
    if cache_path.is_file():
        for line in cache_path.read_text(encoding="utf-8").splitlines():
            match = re.fullmatch(r"([^:#]+):[^=]+=(.*)", line)
            if match:
                cache[match[1]] = match[2]
    found = {}
    for name, variable in (("cmake", "CMAKE_COMMAND"), ("ninja", "CMAKE_MAKE_PROGRAM"),
                           ("clang++", "CMAKE_CXX_COMPILER"), ("glslc", "Vulkan_GLSLC_EXECUTABLE")):
        configured = cache.get(variable)
        path = (str(Path(configured)) if configured and Path(configured).is_file()
                else shutil.which(configured or name))
        if not path:
            raise ValueError(f"Missing prerequisite: {name} (PATH or configured {variable})")
        found[name] = path
    if cache.get("NEAR_LAUGH_UI_AUTOMATION") != "ON":
        raise ValueError("build/debug must be configured with NEAR_LAUGH_UI_AUTOMATION=ON")
    found["configured_build"] = str(cache_path)
    return found


def build_check(fixture, root=ROOT):
    # This existing host method only validates fixed build/fixture metadata.
    # It neither starts a child nor creates an owned fixture/session directory.
    from editor_ui_mcp import SessionController
    from editor_ui_fixtures import load_fixtures

    controller = SessionController(environment="doctor-read-only", repository_root=root)
    fixtures = load_fixtures(root)
    executable, resources, source, fingerprint = controller._configuration(
        {"environment": "doctor-read-only", "fixture": fixture})
    for name in fixtures:
        if name != fixture:
            controller._configuration({"environment": "doctor-read-only", "fixture": name})
    required = ("shaders/prototype_scene_vertex.spv", "shaders/prototype_scene_fragment.spv",
                "fonts/NotoSans-Regular.ttf", "characters/manifest.json", "characters/catalog.json")
    for relative in required:
        path = resources / relative
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"Missing packaged resource: {relative}")
        if path.suffix == ".json":
            if path.stat().st_size > REPORT_LIMIT:
                raise ValueError(f"Resource metadata exceeds 1 MiB: {relative}")
            if not isinstance(json.loads(path.read_text(encoding="utf-8")), dict):
                raise ValueError(f"Resource metadata must be an object: {relative}")
    return {"executable": str(executable), "build_fingerprint": fingerprint,
            "fixture": str(source), "resource_root": str(resources), "checked_resources": list(required),
            "fixture_manifest": str(root / "scripts/editor_ui_fixtures.json"),
            "checked_fixtures": sorted(fixtures),
            "scope": "host build/fixture validation and required package files; not complete resource validation"}


def vulkan_check(path):
    if not path:
        raise ValueError("Editor returned no diagnostic path for Vulkan teardown audit")
    diagnostic = Path(path)
    if diagnostic.stat().st_size > 16 * 1024 * 1024:
        raise ValueError("Editor diagnostic exceeds its 16 MiB bound")
    log = diagnostic.read_text(encoding="utf-8", errors="replace")
    if ("Vulkan validation: enabled" not in log
            or "Automation Vulkan validation errors after teardown: 0" not in log):
        raise ValueError(f"Missing enabled-validation / zero-error teardown evidence: {diagnostic}")
    return {"diagnostic_path": str(diagnostic), "teardown_errors": 0,
            "scope": "startup and close only; not full lifecycle or visual acceptance"}


async def sdk_check(args, report, transcript=None):
    from mcp import Client
    from mcp.client.stdio import stdio_client
    import editor_ui_protocol as protocol
    from editor_ui_client import host_parameters

    def record(direction, message):
        if transcript:
            transcript.record(direction, message)

    parameters = host_parameters(args.environment if args.launch_editor else None)
    if args.disable_implicit_layers:
        parameters.env = {"VK_LOADER_LAYERS_DISABLE": "~implicit~"}
    with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as errors:
        try:
            async with Client(stdio_client(parameters, errlog=errors), mode="legacy",
                              read_timeout_seconds=40 if args.launch_editor else 10) as client:
                connection = {"protocolVersion": client.protocol_version,
                              "serverInfo": client.server_info.model_dump(by_alias=True, exclude_none=True),
                              "instructions": client.instructions}
                record("connection", connection)
                tools = await client.list_tools()
                discovery = tools.model_dump(by_alias=True, exclude_none=True)
                record("response", {"method": "tools/list", "result": discovery})
                if (client.protocol_version != protocol.MCP_PROTOCOL_VERSION
                        or client.server_info.name != "near-laugh-editor-ui"):
                    raise ValueError("Unexpected initialized protocol/server identity")
                if len(tools.tools) != 4 or {tool.name for tool in tools.tools} != set(protocol.INPUT_SCHEMAS):
                    raise ValueError("Expected exactly the four editor-ui tools")
                for tool in tools.tools:
                    if (tool.input_schema != protocol.INPUT_SCHEMAS[tool.name]
                            or tool.output_schema != protocol.OUTPUT_SCHEMAS[tool.name]):
                        raise ValueError(f"Mismatching tool schemas: {tool.name}")
                if not client.instructions:
                    raise ValueError("MCP initialize returned no server instructions")
                report["discovery"] = {"connection": connection, "tools": {"tools": [
                    {"name": tool.name, "schemas": "matched", "schema_sha256": hashlib.sha256(
                        json.dumps({"input": tool.input_schema, "output": tool.output_schema},
                                   sort_keys=True, separators=(",", ":")).encode()).hexdigest()}
                    for tool in tools.tools]},
                                       "response_bytes": len(json.dumps(discovery, separators=(",", ":")).encode()),
                                       "scope": "separate official SDK process, not agent tool visibility"}
                report["mcp_requests"] = 2  # SDK initialize and tools/list.
                if not args.launch_editor:
                    return
                session = None
                diagnostic = None

                async def call(op):
                    nonlocal session, diagnostic
                    arguments = {"protocol_version": 1, "request_id": f"doctor-{op}", "op": op}
                    if op == "start":
                        arguments.update(fixture=args.fixture, environment=args.environment)
                    elif session:
                        arguments["session_id"] = session
                    record("request", {"method": "tools/call", "params": {"name": "ui_session", "arguments": arguments}})
                    report["mcp_requests"] += 1
                    result = await client.call_tool("ui_session", arguments)
                    record("response", result.model_dump(by_alias=True, exclude_none=True))
                    data = result.structured_content
                    protocol.validate_result("ui_session", data, request=arguments)
                    session = data.get("session_id") or session
                    diagnostic = data.get("diagnostic_path") or diagnostic
                    report[op] = data
                    if not data["ok"]:
                        raise ValueError(f"{op}: {data['error']['code']}: {data['error']['message']}")
                    return data

                try:
                    started = await call("start")
                    if started["state"] != "ready":
                        raise ValueError("Editor did not reach a completed ready frame")
                    report["desktop_gpu"] = {"status": "passed", "detail": "Dedicated editor reached ready"}
                finally:
                    # One explicit close; disconnect/job cleanup remains the SDK/host's
                    # responsibility if startup/response failed. No mutation retry.
                    if session:
                        await call("close")
                report["vulkan"] = {"status": "passed", "detail": vulkan_check(diagnostic)}
        except Exception:
            errors.seek(0, 2)
            size = errors.tell()
            errors.seek(max(0, size - 8192))
            report["host_stderr_tail"] = errors.read(8192)
            raise


def diagnose(args, report, transcript=None):
    checks = report["checks"] = {}
    for name, action in (("runtime", runtime_check), ("dependencies", dependencies_check),
                         ("prerequisites", prerequisites_check), ("build_resources", lambda: build_check(args.fixture))):
        try:
            checks[name] = {"status": "passed", "detail": action()}
        except Exception as error:
            checks[name] = {"status": "failed", "detail": str(error)[:4096]}
    report["source_build_freshness"] = {"status": "not_checked", "required_before_functional_run": BUILD_COMMAND}
    report["agent_tools"] = {"status": "not_checked", "detail": "SDK discovery does not check Codex connection; restart after config changes"}
    report["desktop_gpu"] = {"status": "not_checked"}
    report["vulkan"] = {"status": "not_checked"}
    report["visual"] = {"status": "not_checked"}
    if checks["dependencies"]["status"] != "passed":
        checks["sdk"] = {"status": "blocked", "detail": "Install exact requirements with the intended Python first"}
    elif args.launch_editor and any(check["status"] != "passed" for check in checks.values()):
        checks["sdk"] = {"status": "blocked", "detail": "Launch refused because prerequisites failed; run ordinary doctor for discovery"}
    else:
        try:
            asyncio.run(sdk_check(args, report, transcript))
            checks["sdk"] = {"status": "passed", "detail": "Official initialize and tools/list; four exact schemas and instructions"}
        except Exception as error:
            checks["sdk"] = {"status": "failed", "detail": str(error)[:4096]}
            if "start" in report:
                report["vulkan"] = {"status": "failed", "detail": "Requested live check did not establish clean startup/teardown; inspect evidence"}
    return int(any(check["status"] != "passed" for check in checks.values()))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path, help="New JSON report; also writes a bounded .jsonl SDK transcript")
    parser.add_argument("--fixture", default="apartment-stairs", help="Fixture ID in the host's configured manifest")
    parser.add_argument("--launch-editor", action="store_true", help="Explicitly opt into dedicated editor startup/close")
    parser.add_argument("--environment", help="Operator-authorized desktop/profile for this run; does not grant authorization")
    parser.add_argument("--disable-implicit-layers", action="store_true", help="Live check only: child-local Vulkan loader filter; explicit validation stays enabled")
    args = parser.parse_args(argv)
    if args.launch_editor and not args.environment:
        parser.error("--launch-editor requires an explicitly authorized --environment")
    if not args.launch_editor and (args.environment or args.disable_implicit_layers):
        parser.error("--environment and --disable-implicit-layers require --launch-editor")
    if args.report and args.report.suffix.lower() != ".json":
        parser.error("--report must be a new .json path with an existing parent directory")
    report, output, transcript = {}, None, None
    started = time.monotonic()
    try:
        if args.report:
            output = args.report.open("x", encoding="utf-8")
            # Import lazily: a doctor must still diagnose a missing SDK.
            try:
                from editor_ui_client import Transcript
            except ImportError:
                pass
            else:
                transcript = Transcript(args.report.with_suffix(".jsonl"))
        exit_code = diagnose(args, report, transcript)
        report["exit_code"] = exit_code
        report["duration_seconds"] = round(time.monotonic() - started, 3)
        if output:
            serialized = json.dumps(report, ensure_ascii=True, indent=2)
            if len(serialized.encode("utf-8")) > REPORT_LIMIT:
                raise ValueError("Doctor report exceeded 1 MiB; bounded transcript retains SDK details")
            output.write(serialized + "\n")
        print(f"editor-ui doctor: {'passed' if exit_code == 0 else 'blocked'} ({report['duration_seconds']}s)")
        for name, check in report["checks"].items():
            suffix = "" if check["status"] == "passed" else f": {check['detail']}"
            print(f"  {name}: {check['status']}{suffix}")
        print("  " + "; ".join(f"{name}: {report[name]['status']}" for name in
              ("desktop_gpu", "vulkan", "source_build_freshness", "agent_tools", "visual")))
        print(f"  Before each functional scenario: {BUILD_COMMAND}")
        if args.report:
            print(f"  Evidence: {args.report}" + (f", {args.report.with_suffix('.jsonl')}" if transcript else ""))
        return exit_code
    except Exception as error:
        print(f"editor-ui doctor: failed: {str(error)[:1024]}", file=sys.stderr)
        return 1
    finally:
        if transcript:
            transcript.close()
        if output:
            output.close()


if __name__ == "__main__":
    raise SystemExit(main())
