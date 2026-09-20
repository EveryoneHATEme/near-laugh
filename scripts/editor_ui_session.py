"""Small official-SDK session helper for project UI regressions and new batches.

This manages evidence and discovery only. All actions and assertions execute in
the existing editor. It never retries an action or interprets a scenario language.
"""

from contextlib import AsyncExitStack
import json
from pathlib import Path
import time

from mcp import Client
from mcp.client.stdio import stdio_client

import editor_ui_protocol as protocol
from editor_ui_client import Transcript, host_parameters

MAX_SCENARIO_CALLS = 256  # Includes one reserved close call; no unbounded report lists.
MAX_SUMMARY_BYTES = 8 * 1024 * 1024


def uncertain(result):
    return (result.get("effects") == "unknown" or result.get("error", {}).get("effects") == "unknown"
            or any(step["status"] == "unknown" for step in result.get("steps", [])))


def compact_value(value):
    """Full evidence stays in the bounded transcript; never label a prefix known."""
    size = len(json.dumps(value, ensure_ascii=False).encode("utf-8"))
    if size <= 4096:
        return value
    return {"detail": "retained in transcript", "serialized_bytes": size,
            "availability": value.get("availability", "unknown")}


class UiFailure(RuntimeError):
    def __init__(self, result):
        self.result = result
        error = result.get("error", {})
        super().__init__(f"{error.get('code', 'failed')}: {error.get('message', 'UI check failed')}")


def known(value):
    """Require a complete value; never flatten unavailable/truncated into None."""
    if value.get("availability") != "known":
        raise AssertionError(f"Expected a known value, observed {value}")
    return value["value"]


def target(item):
    return {"ref": item["ref"]}


def selector(scope, key, owner=None):
    value = {"scope": scope, "key": key}
    if owner is not None:
        value["owner"] = owner
    return {"selector": value}


def app_assert(projection, field, expected, *, object_ref=None, predicate="equals", tolerance=None):
    condition = {"source": "app", "projection": projection, "field": field,
                 "predicate": predicate, "expected": expected}
    if object_ref is not None:
        condition["object_ref"] = object_ref
    if tolerance is not None:
        condition["tolerance"] = tolerance
    return {"op": "assert", "condition": condition}


def ui_assert(item_target, field, expected):
    return {"op": "assert", "condition": {"source": "ui", "target": item_target,
            "field": field, "predicate": "equals", "expected": expected}}


class EditorUiSession:
    """Own one disposable session and close it even when setup/scenario fails.

    A lost startup reply leaves no usable session ID. Closing the SDK transport
    then lets the production host's EOF/Job Object policy clean up its child;
    this client reports cleanup as unverified, never assumes that EOF proved it.
    No item/ref cache is kept. Callers must rediscover after document replacement.
    """

    def __init__(self, environment, transcript, *, fixture="household-interactions",
                 disable_implicit_layers=False):
        self.environment, self.fixture = environment, fixture
        self.path = Path(transcript)
        self.disable_implicit_layers = disable_implicit_layers
        self.session_id = None
        self.started = None
        self.client = None
        self.sequence = 0
        self.stack = AsyncExitStack()
        self.evidence_stack = AsyncExitStack()
        self.summary_file = None
        self.transcript = None
        self.closed = False
        self.begin = time.monotonic()
        self.report = {"status": "not_run", "level": "B", "agent_mcp": "not_checked",
                       "visual": "not_run", "cleanup": "unverified", "calls": [], "batches": [],
                       "evidence": {"transcript": str(self.path),
                                    "summary": str(self.path.with_suffix(".summary.json")),
                                    "stderr": str(self.path.with_suffix(".stderr.log"))}}

    async def __aenter__(self):
        try:
            self._open_evidence()
            errors = self.stack.enter_context(self.path.with_suffix(".stderr.log").open("x", encoding="utf-8"))
            parameters = host_parameters(self.environment)
            if self.disable_implicit_layers:
                parameters.env = {"VK_LOADER_LAYERS_DISABLE": "~implicit~"}
            begin = time.monotonic()
            self.client = await self.stack.enter_async_context(
                Client(stdio_client(parameters, errlog=errors), mode="legacy", read_timeout_seconds=70))
            # Initialization belongs to the SDK; retain its negotiated facts,
            # not a reconstruction of JSON-RPC IDs or a second handshake.
            connection = {"method": "initialize", "protocolVersion": self.client.protocol_version,
                          "serverInfo": self.client.server_info.model_dump(by_alias=True, exclude_none=True),
                          "instructions": self.client.instructions}
            self.transcript.record("connection", connection)
            self._metric("initialize", begin, connection, evidence="negotiated_sdk_facts")
            self.transcript.record("request", {"method": "tools/list"})
            begin = time.monotonic()
            listing = await self.client.list_tools()
            value = listing.model_dump(by_alias=True, exclude_none=True)
            self.transcript.record("response", {"method": "tools/list", "result": value})
            self._metric("tools/list", begin, value)
            if {tool.name for tool in listing.tools} != set(protocol.INPUT_SCHEMAS):
                raise AssertionError("Host did not expose the four semantic tools")
            for tool in listing.tools:
                if tool.input_schema != protocol.INPUT_SCHEMAS[tool.name] or tool.output_schema != protocol.OUTPUT_SCHEMAS[tool.name]:
                    raise AssertionError(f"Schema mismatch: {tool.name}")
            self.started = await self.call("ui_session", op="start", fixture=self.fixture, environment=self.environment)
            self.require(self.started)
            self.session_id = self.started["session_id"]
            if self.started["state"] != "ready":
                raise AssertionError("Startup did not produce a ready editor session")
            self.report.update(status="running", build_fingerprint=self.started["build_fingerprint"])
            return self
        except BaseException as error:
            if self.report["status"] == "not_run":
                self.report["status"] = "blocked"
            await self.__aexit__(type(error), error, error.__traceback__)
            raise

    def _open_evidence(self):
        # Reserve every writable evidence destination before connecting/starting.
        # This separate stack outlives the SDK stack, so final teardown facts can
        # be written before closing the exclusively created summary handle.
        self.summary_file = self.evidence_stack.enter_context(
            self.path.with_suffix(".summary.json").open("x", encoding="utf-8"))
        self.transcript = Transcript(self.path)
        self.evidence_stack.callback(self.transcript.close)

    def _metric(self, method, begin, value=None, **details):
        self.report["calls"].append({"method": method, "duration_ms": round((time.monotonic() - begin) * 1000, 3),
            "response_json_bytes": len(protocol.encode_message(value)) if value is not None else None, **details})

    @staticmethod
    def require(result):
        if not result["ok"]:
            raise UiFailure(result)
        return result

    async def call(self, tool, **arguments):
        if any(key in arguments for key in ("request_id", "session_id", "protocol_version")):
            raise ValueError("The session helper owns fresh request/session IDs")
        closing = tool == "ui_session" and arguments.get("op") == "close"
        if self.sequence >= MAX_SCENARIO_CALLS or (self.sequence == MAX_SCENARIO_CALLS - 1 and not closing):
            raise protocol.ProtocolError("limit_exceeded", "Scenario call budget reached; only owned cleanup remains")
        self.sequence += 1
        request = {"protocol_version": 1, "request_id": str(self.sequence), **arguments}
        if self.session_id:
            request["session_id"] = self.session_id
        protocol.validate_request(tool, request)
        self.transcript.record("request", {"method": "tools/call", "params": {"name": tool, "arguments": request}})
        begin = time.monotonic()
        try:
            result = await self.client.call_tool(tool, request)
            value = result.model_dump(by_alias=True, exclude_none=True)
            self.transcript.record("response", {"method": "tools/call", "tool": tool, "request_id": request["request_id"], "result": value})
            self._metric("tools/call", begin, value, tool=tool, request_id=request["request_id"])
            data = result.structured_content
            protocol.validate_result(tool, data, request=request)
            if json.loads(result.content[0].text) != data or result.is_error != (not data["ok"]):
                raise AssertionError("MCP text/structured/error result disagreement")
        except BaseException as error:
            unknown = {"tool": tool, "request_id": request["request_id"], "status": "unknown",
                       "effects": "unknown", "cleanup": "unverified", "error": str(error)[:512]}
            if tool == "ui_execute":
                unknown["steps"] = [{"index": index, "status": "unknown"} for index in range(len(arguments["steps"]))]
                self.report["batches"].append(unknown)
            self.report.update(status="unknown", failure=unknown)
            self.transcript.record("unverified_result", unknown)
            if not self.report["calls"] or self.report["calls"][-1].get("request_id") != request["request_id"]:
                self._metric("tools/call", begin, tool=tool, request_id=request["request_id"], status="unknown")
            raise
        if data.get("diagnostic_path"):
            self.report["evidence"]["editor_log"] = data["diagnostic_path"]
        if uncertain(data):
            self.report.update(status="unknown", failure=data.get("error"))
        return data

    async def execute(self, steps, *, expected_error=None, timeout_ms=20000):
        result = await self.call("ui_execute", steps=steps, timeout_ms=timeout_ms,
                                 policy={"auto_focus": True, "auto_scroll": True})
        error = result.get("error", {})
        self.report["batches"].append({"request_id": result["request_id"],
            "status": "unknown" if uncertain(result) else "passed" if result["ok"] else "failed",
            "expected_error": expected_error, "failed_step": result["failed_index"],
            "steps": [step["status"] for step in result["steps"]], "expected": compact_value(error.get("expected")),
            "observed": compact_value(error.get("observed")), "effects": result["effects"], "cleanup": result["cleanup"]})
        if expected_error is None:
            self.require(result)
        elif result["ok"] or error.get("code") != expected_error:
            raise AssertionError(f"Expected {expected_error}, observed {result}")
        return result

    async def _pages(self, tool, key, arguments):
        values, cursors, first = [], set(), None
        for _ in range(64):
            result = self.require(await self.call(tool, **arguments))
            if result["snapshot"]["stale"]:
                raise AssertionError("Discovery returned stale evidence")
            if tool == "app_inspect" and result["truncated"]:
                raise AssertionError("Application inspection returned truncated evidence")
            if first is None:
                first = result
            elif result["snapshot"] != first["snapshot"]:
                raise AssertionError("Pagination mixed snapshots")
            values.extend(result[key])
            cursor = result["next_cursor"]
            if cursor is None:
                return {**first, key: values, "next_cursor": None}
            if cursor in cursors:
                raise AssertionError("Pagination repeated a cursor")
            cursors.add(cursor)
            arguments = {"cursor": cursor}
        raise protocol.ProtocolError("limit_exceeded", "Discovery exceeded 64 pages")

    async def observe(self, scope, *, depth=0, page_size=64, **filters):
        arguments = {"scope": scope, "depth": depth, "page_size": page_size}
        if filters:
            arguments["filter"] = filters
        return await self._pages("ui_observe", "items", arguments)

    async def find(self, scope, *, label=None, depth=0, **filters):
        result = await self.observe(scope, depth=depth, **filters)
        if result["coverage"]["truncated"]:
            raise AssertionError("Truncated discovery cannot establish a unique target; narrow the observation")
        items = result["items"]
        if label is not None:
            items = [item for item in items if item.get("label_availability", "known") == "known" and item["label"] == label]
        if len(items) != 1:
            raise AssertionError(f"Expected exactly one {filters} label={label!r} in {scope}; observed {len(items)}; coverage={result['coverage']}")
        return items[0]

    async def inspect(self, projection, fields, *, object_ref=None):
        arguments = {"projection": projection, "fields": fields, "page_size": 128}
        if object_ref is not None:
            arguments["object_ref"] = object_ref
        return (await self._pages("app_inspect", "values", arguments))["values"]

    def slot(self, name):
        matches = [slot["path"] for slot in self.started["file_slots"] if slot["slot"] == name]
        if len(matches) != 1:
            raise AssertionError(f"Expected an owned file slot: {name}")
        return matches[0]

    async def close(self):
        if self.closed:
            return
        self.closed = True  # A lost close reply is not replayed either.
        if self.session_id:
            result = await self.call("ui_session", op="close")
            self.report["close_result"] = result
            self.report["cleanup"] = result.get("error", {}).get("cleanup", "released" if result["ok"] else "unverified")
            self.require(result)

    async def __aexit__(self, exc_type, error, traceback):
        cleanup_error = None
        if error is not None:
            if self.report["status"] not in ("unknown", "blocked", "unsupported"):
                self.report["status"] = "failed"
            self.report["error"] = str(error)[:2048]
            if isinstance(error, UiFailure):
                self.report["failure"] = error.result.get("error")
                code = error.result.get("error", {}).get("code")
                if uncertain(error.result):
                    self.report["status"] = "unknown"
                elif self.report["status"] == "unknown":
                    pass  # Later errors cannot disprove earlier uncertain effects.
                elif code == "unsupported":
                    self.report["status"] = "unsupported"
                elif code in ("environment_not_authorized", "environment_unavailable", "version_mismatch"):
                    self.report["status"] = "blocked"
        try:
            await self.close()
        except BaseException as failure:
            cleanup_error = failure
            self.report.update(status="unknown" if self.report["status"] == "unknown" else "failed", cleanup_error=str(failure)[:512])
        finally:
            try:
                await self.stack.aclose()
            except BaseException as failure:
                cleanup_error = cleanup_error or failure
                self.report.update(status="unknown" if self.report["status"] == "unknown" else "failed",
                                   transport_cleanup_error=str(failure)[:512])
            if self.report["status"] == "running":
                self.report["status"] = "passed"
            self.report["duration_ms"] = round((time.monotonic() - self.begin) * 1000, 3)
            self.report["mcp_requests"] = len(self.report["calls"])
            self.report["tool_calls"] = sum(call["method"] == "tools/call" for call in self.report["calls"])
            sizes = [call["response_json_bytes"] for call in self.report["calls"] if call["response_json_bytes"] is not None]
            self.report["response_json_bytes"] = {"total": sum(sizes), "maximum": max(sizes, default=0),
                "basis": "SDK result JSON, excluding JSON-RPC envelope; initialize uses negotiated facts"}
            try:
                if self.summary_file is not None:
                    text = json.dumps(self.report, ensure_ascii=False, indent=2)
                    if len(text.encode("utf-8")) > MAX_SUMMARY_BYTES:
                        raise protocol.ProtocolError("limit_exceeded", "Summary exceeds its 8 MiB bound; retain transcript")
                    self.summary_file.write(text)
                    self.summary_file.flush()
            except BaseException as failure:
                cleanup_error = cleanup_error or failure
                self.report["summary_error"] = str(failure)[:512]
                if self.report["status"] != "unknown":
                    self.report["status"] = "failed"
            finally:
                try:
                    await self.evidence_stack.aclose()
                except BaseException as failure:
                    cleanup_error = cleanup_error or failure
                    self.report["evidence_cleanup_error"] = str(failure)[:512]
                    if self.report["status"] != "unknown":
                        self.report["status"] = "failed"
        if cleanup_error is not None and error is None:
            raise cleanup_error
        return False
