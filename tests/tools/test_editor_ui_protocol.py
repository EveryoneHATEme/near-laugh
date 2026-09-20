"""Run without a window/GPU: python -B -m unittest discover -s tests/tools -v."""

import copy
import json
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))
import editor_ui_protocol as protocol


TARGET = {"selector": {"scope": "properties", "key": "position.x", "owner": "selection"}}
STAMP = {"snapshot_id": "snapshot-1", "frame": "1", "document_generation": "1",
         "document_revision": "5", "selection_revision": "2", "preview_revision": "0",
         "stale": False}
UNKNOWN = {"availability": "unknown", "reason": "not sampled"}
KNOWN = {"availability": "known", "type": "float", "value": 1.5}


def request(**fields):
    return {"protocol_version": 1, "request_id": "1", "session_id": "session-1", **fields}


def error(code="assertion_failed", **fields):
    return {"code": code, "message": "bounded diagnostic", "step_index": None,
            "target": None, "last_completed_index": None, "expected": KNOWN,
            "observed": UNKNOWN, "snapshot": STAMP, "effects": "none",
            "cleanup": "released", "session_state": "ready", **fields}


def result(**fields):
    return {"protocol_version": 1, "request_id": "1", "session_id": "session-1",
            "build_fingerprint": "test-build", "ok": True, **fields}


def item():
    return {"ref": "item-1", "scope": "properties", "key": "position.x",
            "owner": "object-1", "label": "Position X", "kind": "number",
            "capabilities": ["focus", "edit", "commit", "assert"],
            "state": {"submitted": True, "visible": True, "enabled": True,
                      "active": True, "focused": True,
                      "selected": {"availability": "not_applicable"},
                      "open": {"availability": "not_applicable"}},
            "value": {"availability": "known", "type": "float", "draft": 1.5, "unit": "m"},
            "input": {"availability": "known", "text": "1.5", "uncommitted": True},
            "commit": {"policy": "deactivate", "methods": ["tab"]},
            "applied_binding": {"projection": "object", "field": "position.x"},
            "provenance": {"identity": "ui_metadata", "state": "imgui", "value": "ui_draft"}}


def observation():
    return result(snapshot=copy.deepcopy(STAMP), active_scope="properties", modal_scope=None,
                  items=[item()], coverage={"limitations": ["submitted_only"],
                  "snapshot_item_count": 1, "truncated": False, "scopes": []},
                  next_cursor=None)


def batch_result(statuses=("passed",)):
    steps = []
    last = failed = None
    for index, status in enumerate(statuses):
        step = {"index": index, "status": status,
                "before": None if status == "not_run" else STAMP,
                "after": None if status == "not_run" else STAMP}
        if status == "passed":
            last = index
            step["observed"] = KNOWN
        if status == "failed":
            failed = index
            step["error"] = error(step_index=index, last_completed_index=last, effects="partial")
        steps.append(step)
    payload = result(steps=steps, before=STAMP, after=STAMP, last_completed_index=last,
                     failed_index=failed, effects="partial", cleanup="released", session_state="ready")
    if failed is not None:
        payload.update(ok=False, error=error(step_index=failed, last_completed_index=last, effects="partial"))
    return payload


class RequestTests(unittest.TestCase):
    def rejects(self, tool, value):
        with self.assertRaises(protocol.ProtocolError):
            protocol.validate_request(tool, value)

    def test_normative_session_variants(self):
        protocol.validate_request("ui_session", {"protocol_version": 1, "request_id": "start-1",
            "op": "start", "fixture": "prototype", "environment": "isolated-test"})
        protocol.validate_request("ui_session", {"protocol_version": 1, "request_id": "status-1", "op": "status"})
        for op in ("status", "close"):
            protocol.validate_request("ui_session", request(op=op))
        protocol.validate_request("ui_session", request(op="cancel", active_request_id="999999999999999999"))
        self.rejects("ui_session", request(op="start", fixture="x", environment="y"))
        self.rejects("ui_session", request(op="cancel"))
        self.rejects("ui_session", request(op="close", executable="editor.exe"))

    def test_observe_and_projection_variants(self):
        protocol.validate_request("ui_observe", request(scope="root", depth=16, page_size=256,
            filter={"key": "position.x", "kind": "number", "owner": "object-1"}))
        for tool in ("ui_observe", "app_inspect"):
            protocol.validate_request(tool, request(cursor="cursor-1"))
            self.rejects(tool, request(cursor="cursor-1", scope="root"))
            self.rejects(tool, request(cursor="cursor-1", page_size=1))
        for name, fields in protocol.PROJECTION_FIELDS.items():
            args = request(projection=name, fields=[fields[0]], snapshot_id="snapshot-1")
            if name == "object":
                args["object_ref"] = "object-1"
            protocol.validate_request("app_inspect", args)
            args["fields"] = ["arbitrary.pointer.member"]
            self.rejects("app_inspect", args)
        self.rejects("app_inspect", request(projection="object"))
        self.rejects("app_inspect", request(projection="document", object_ref="x"))
        self.rejects("app_inspect", request(projection="document", fields=["dirty", "dirty"]))

    def test_all_operation_forms(self):
        steps = [{"op": op, "target": TARGET} for op in ("activate", "focus", "open", "close", "select")]
        steps += [{"op": "set_checked", "target": TARGET, "value": True},
                  {"op": "edit", "target": TARGET, "text": "Привет\nworld", "commit": "none"},
                  {"op": "edit", "target": TARGET, "value": 1.5, "commit": "tab"},
                  {"op": "commit", "target": TARGET, "method": "enter"},
                  {"op": "scroll", "target": TARGET, "direction": "down", "pages": 2},
                  {"op": "drag", "target": TARGET, "direction": "increase", "fraction": .25, "frames": 8},
                  {"op": "scroll", "target": TARGET, "to": {"ref": "item-2"}}]
        for chord in protocol.CHORDS:
            steps.append({"op": "key", "target": TARGET, "chord": chord})
        protocol.validate_request("ui_execute", request(steps=steps, timeout_ms=60000,
            policy={"auto_scroll": True, "auto_focus": False},
            if_state={"document_generation": "9007199254740992", "document_revision": "18"}))

    def test_assertion_predicates(self):
        predicates = [{"predicate": "exists", "expected": True},
                      {"predicate": "equals", "expected": "-"},
                      {"predicate": "not_equals", "expected": None},
                      {"predicate": "range", "expected": {"min": 1, "max": 2}},
                      {"predicate": "approx", "expected": 1.5, "tolerance": {"absolute": 0.001}},
                      {"predicate": "contains", "expected": "буквы"},
                      {"predicate": "count", "expected": 3}]
        for predicate in predicates:
            for op in ("assert", "wait_until"):
                condition = {"source": "ui", "target": TARGET, "field": "input.text", **predicate}
                protocol.validate_request("ui_execute", request(steps=[{"op": op, "condition": condition}]))
        for projection, fields in protocol.PROJECTION_FIELDS.items():
            condition = {"source": "app", "projection": projection, "field": fields[0],
                         "predicate": "exists", "expected": True}
            if projection == "object":
                condition["object_ref"] = "object-1"
            protocol.validate_request("ui_execute", request(steps=[{"op": "assert", "condition": condition}]))

    def test_invalid_static_batch_rejected_whole(self):
        invalid_steps = [
            {"op": "callback", "name": "save"}, {"op": "activate", "target": TARGET, "x": 1},
            {"op": "edit", "target": TARGET, "value": 2, "text": "2"},
            {"op": "edit", "target": TARGET, "value": True},
            {"op": "edit", "target": TARGET, "text": "x", "commit": "escape"},
            {"op": "commit", "target": TARGET}, {"op": "key", "target": TARGET, "chord": "Alt+F4"},
            {"op": "scroll", "target": TARGET, "direction": "down", "pages": 0},
            {"op": "drag", "target": TARGET, "direction": "increase", "fraction": 0},
            {"op": "drag", "target": TARGET, "direction": "increase", "fraction": 1.1},
            {"op": "drag", "target": TARGET, "direction": "increase", "fraction": True},
            {"op": "drag", "target": TARGET, "direction": "increase", "fraction": .5, "frames": 121},
            {"op": "drag", "target": TARGET, "x": 100, "y": 200},
            {"op": "scroll", "target": TARGET, "direction": "down", "pages": 1, "to": TARGET},
            {"op": "activate", "target": {"ref": "x", "selector": TARGET["selector"]}},
            {"op": "activate", "target": {"selector": {"scope": "root"}}},
            {"op": "activate", "target": TARGET, "timeout_ms": True},
            {"op": "activate", "target": TARGET, "timeout_ms": 30001},
        ]
        for step in invalid_steps:
            with self.subTest(step=step):
                self.rejects("ui_execute", request(steps=[{"op": "activate", "target": TARGET}, step]))
        for request_id in ("0", "01", "-1", "1\n", "x", 1, True):
            self.rejects("ui_execute", request(request_id=request_id, steps=[{"op": "activate", "target": TARGET}]))
        self.rejects("ui_execute", request(steps=[]))
        self.rejects("ui_execute", request(steps=[{"op": "activate", "target": TARGET}] * 65))

    def test_numeric_and_utf8_bounds(self):
        for value in (float("nan"), float("inf"), -float("inf"), 2**53):
            self.rejects("ui_execute", request(steps=[{"op": "edit", "target": TARGET, "value": value}]))
        valid = request(steps=[{"op": "edit", "target": TARGET, "text": "я" * 8192}])
        protocol.validate_request("ui_execute", valid)
        valid["steps"][0]["text"] += "я"
        self.rejects("ui_execute", valid)
        self.rejects("ui_observe", request(scope="root", depth=True))
        self.rejects("ui_observe", request(scope="root", depth=17))
        self.rejects("ui_observe", request(scope="root", page_size=257))

    def test_condition_constraints(self):
        base = {"source": "ui", "target": TARGET, "field": "input.text"}
        for predicate in ({"predicate": "regex", "expected": ".*"},
                          {"predicate": "approx", "expected": 1},
                          {"predicate": "approx", "expected": 1, "tolerance": {}},
                          {"predicate": "approx", "expected": 1, "tolerance": {"absolute": -1}},
                          {"predicate": "range", "expected": {"min": 2, "max": 1}},
                          {"predicate": "range", "expected": {}},
                          {"predicate": "contains", "expected": 2},
                          {"predicate": "count", "expected": True}):
            self.rejects("ui_execute", request(steps=[{"op": "assert", "condition": {**base, **predicate}}]))


class ResultTests(unittest.TestCase):
    def rejects(self, tool, value, **kwargs):
        with self.assertRaises(protocol.ProtocolError):
            protocol.validate_result(tool, value, **kwargs)

    def test_normative_observed_item(self):
        protocol.validate_result("ui_observe", observation())
        value = observation()
        value["items"][0]["input"]["text"] = "-"
        protocol.validate_result("ui_observe", value)
        self.assertEqual(value["items"][0]["value"]["draft"], 1.5)

    def test_all_availability_values(self):
        for availability in protocol.AVAILABILITY:
            value = observation()
            sample = {"availability": availability}
            if availability == "known":
                sample.update(type="string", draft="value")
            elif availability == "truncated":
                sample.update(type="string", draft="prefix", reason="byte budget")
            value["items"][0]["value"] = sample
            protocol.validate_result("ui_observe", value)
        value = observation()
        value["items"][0]["value"] = {"availability": "known", "type": "float", "draft": None}
        self.rejects("ui_observe", value)
        value["items"][0]["value"] = {"availability": "unknown", "draft": 0}
        self.rejects("ui_observe", value)
        value["items"][0]["value"] = {"availability": "truncated", "type": "string", "draft": 1, "reason": "budget"}
        self.rejects("ui_observe", value)
        value = observation()
        value["items"][0]["state"]["open"] = None
        self.rejects("ui_observe", value)

    def test_all_projection_values(self):
        for projection, fields in protocol.PROJECTION_FIELDS.items():
            source = "document" if projection in ("object", "objects") else projection
            value = result(snapshot=STAMP, next_cursor=None, truncated=False,
                values=[{"projection": projection, "field": fields[0], "object_ref": None,
                         "value": UNKNOWN, "source": source}])
            protocol.validate_result("app_inspect", value)
            value["values"][0]["field"] = "setter"
            self.rejects("app_inspect", value)

    def test_truncated_label_and_inactive_input_are_explicit(self):
        value = observation()
        value["items"][0]["label_availability"] = "truncated"
        value["items"][0]["input"] = {"availability": "truncated", "text": "prefix",
            "uncommitted": False, "reason": "byte budget"}
        protocol.validate_result("ui_observe", value)
        value["items"][0]["label_availability"] = "inferred"
        self.rejects("ui_observe", value)

    def test_snapshot_and_projection_provenance(self):
        value = observation()
        value["snapshot"] = {**STAMP, "stale": True}
        self.rejects("ui_observe", value)
        value = observation()
        value["coverage"]["snapshot_item_count"] = 0
        self.rejects("ui_observe", value)
        value = result(snapshot=STAMP, next_cursor=None, truncated=False,
            values=[{"projection": "preview", "field": "character.time", "object_ref": None,
                     "value": KNOWN, "source": "document"}])
        self.rejects("app_inspect", value)
        value["values"][0]["source"] = "preview"
        protocol.validate_result("app_inspect", value)
        self.rejects("app_inspect", value, request=request(projection="document"))

    def test_session_result(self):
        value = result(state="ready", capabilities=list(protocol.OPERATIONS),
                       limits=dict(protocol.LIMITS), file_slots=[{"slot": "save-1",
                       "path": "D:/temporary/session/save-1.level.json", "writable": True}],
                       last_request=None, excluded=["viewport_picking", "os_dialogs"])
        protocol.validate_result("ui_session", value)
        value["limits"]["steps"] = 128
        self.rejects("ui_session", value)

    def test_complete_error_envelopes(self):
        for code in protocol.ERROR_CODES:
            value = result(ok=False, error=error(code))
            for tool in ("ui_session", "ui_observe", "app_inspect"):
                protocol.validate_result(tool, value)
        for field in ("cleanup", "effects", "snapshot", "observed", "session_state"):
            value = result(ok=False, error=error())
            del value["error"][field]
            self.rejects("ui_observe", value)
        value = result(ok=False, error=error("unexpected"))
        self.rejects("ui_observe", value)

    def test_passed_and_fail_fast_results(self):
        protocol.validate_result("ui_execute", batch_result())
        value = batch_result(("passed", "failed", "not_run"))
        req = request(steps=[{"op": "activate", "target": TARGET}] * 3)
        protocol.validate_result("ui_execute", value, request=req)
        value["steps"][2] = {"index": 2, "status": "passed", "before": STAMP,
                             "after": STAMP, "observed": KNOWN}
        self.rejects("ui_execute", value)
        value = batch_result(("passed", "failed", "not_run"))
        value["steps"].pop()
        self.rejects("ui_execute", value, request=req)
        value = batch_result()
        value["cleanup"] = "unverified"
        self.rejects("ui_execute", value)
        value = batch_result(("failed", "not_run"))
        value["steps"][1]["after"] = STAMP
        self.rejects("ui_execute", value)
        value = batch_result()
        value["last_completed_index"] = None
        self.rejects("ui_execute", value)

    def test_request_identity_and_running_retry(self):
        value = batch_result()
        req = request(steps=[{"op": "activate", "target": TARGET}])
        protocol.validate_result("ui_execute", value, request=req)
        req["request_id"] = "2"
        self.rejects("ui_execute", value, request=req)
        protocol.validate_result("ui_execute", result(status="running", session_state="executing"))

    def test_unverified_crash_steps_preserve_count_without_fabricating_results(self):
        value = batch_result(("passed", "not_run", "not_run"))
        value.update(ok=False, effects="unknown", cleanup="process_terminated", session_state="faulted")
        value["error"] = error("disconnected", last_completed_index=0, effects="unknown",
                               cleanup="process_terminated", session_state="faulted")
        for step in value["steps"][1:]:
            step["status"] = "unknown"
        req = request(steps=[{"op": "activate", "target": TARGET}] * 3)
        protocol.validate_result("ui_execute", value, request=req)
        value["steps"][1]["observed"] = KNOWN
        self.rejects("ui_execute", value)
        del value["steps"][1]["observed"]
        value["cleanup"] = value["error"]["cleanup"] = "released"
        self.rejects("ui_execute", value)

    def test_unknown_result_fields_are_rejected(self):
        for location in ((), ("items", 0), ("items", 0, "state"),
                         ("items", 0, "value"), ("items", 0, "commit"),
                         ("items", 0, "provenance"), ("snapshot",), ("coverage",)):
            value = observation()
            node = value
            for key in location:
                node = node[key]
            node["unknown"] = 1
            self.rejects("ui_observe", value)


class CodecTests(unittest.TestCase):
    def test_bytewise_utf8_and_multiple_messages(self):
        payload = {"text": "Здравствуйте\nworld", "brackets": "[{}]\\\""}
        encoded = protocol.encode_message(payload)
        decoder = protocol.JsonLineDecoder()
        output = []
        for byte in encoded:
            output += decoder.feed(bytes([byte]), now=0)
        self.assertEqual(output, [payload])
        self.assertEqual(decoder.feed(encoded * 2, now=1), [payload, payload])
        self.assertEqual(decoder.buffered_bytes, 0)

    def test_partial_timeout_does_not_extend_with_fragments(self):
        decoder = protocol.JsonLineDecoder()
        decoder.expire(now=10000)  # idle connections have no partial deadline
        decoder.feed(b'{"a":', now=10000)
        decoder.feed(b"1", now=10004)
        with self.assertRaises(protocol.ProtocolError) as raised:
            decoder.feed(b"}\n", now=10005)
        self.assertEqual(raised.exception.code, "timeout")
        self.assertTrue(decoder.closed)
        self.assertEqual(decoder.buffered_bytes, 0)
        with self.assertRaises(protocol.ProtocolError):
            decoder.feed(b"{}\n", now=10006)

    def test_nesting_limit_is_checked_before_json_parse(self):
        decoder = protocol.JsonLineDecoder(max_depth=3)
        with patch.object(protocol.json, "loads") as parser:
            with self.assertRaises(protocol.ProtocolError) as raised:
                decoder.feed(b'{"a":[[[', now=0)
            parser.assert_not_called()
        self.assertEqual(raised.exception.code, "limit_exceeded")
        decoder = protocol.JsonLineDecoder(max_depth=2)
        self.assertEqual(decoder.feed(b'{"s":"[[[[{{{{"}\n', now=0), [{"s": "[[[[{{{{"}])

    def test_encoder_decoder_nesting_boundary_agrees(self):
        value = {}
        for _ in range(protocol.MAX_JSON_DEPTH - 1):
            value = {"nested": value}
        encoded = protocol.encode_message(value)
        self.assertEqual(protocol.JsonLineDecoder().feed(encoded), [value])
        with self.assertRaises(protocol.ProtocolError):
            protocol.encode_message({"nested": value})

    def test_byte_budget_includes_newline_and_precedes_parse(self):
        wire = protocol.encode_message({"x": "я"})
        decoder = protocol.JsonLineDecoder(max_bytes=len(wire))
        self.assertEqual(decoder.feed(wire), [{"x": "я"}])
        decoder = protocol.JsonLineDecoder(max_bytes=len(wire) - 1)
        with patch.object(protocol.json, "loads") as parser:
            with self.assertRaises(protocol.ProtocolError):
                decoder.feed(wire[:-1])
            parser.assert_not_called()
        self.assertEqual(protocol.encode_message({"x": "я"}, max_bytes=len(wire)), wire)
        with self.assertRaises(protocol.ProtocolError):
            protocol.encode_message({"x": "я"}, max_bytes=len(wire) - 1)

    def test_invalid_json_unicode_duplicates_and_numbers(self):
        lines = [b"[]\n", b"null\n", b"\n", b'{"a":NaN}\n', b'{"a":Infinity}\n',
                 b'{"a":1e999}\n', b'{"a":9007199254740992}\n',
                 b'{"a":1,"a":2}\n', b'{"a":{"b":1,"b":2}}\n',
                 b'{"a":"\xff"}\n', b'{"a":"\\ud800"}\n', b'{"a":}\n']
        for line in lines:
            with self.subTest(line=line):
                decoder = protocol.JsonLineDecoder()
                with self.assertRaises(protocol.ProtocolError):
                    decoder.feed(line)
                self.assertTrue(decoder.closed)
        for value in ({"a": float("nan")}, {"a": "\ud800"}, {1: "bad key"}):
            with self.assertRaises(protocol.ProtocolError):
                protocol.encode_message(value)

    def test_eof_never_executes_unterminated_input(self):
        decoder = protocol.JsonLineDecoder()
        self.assertEqual(decoder.feed(b'{"x":1}'), [])
        with self.assertRaises(protocol.ProtocolError):
            decoder.finish()
        decoder = protocol.JsonLineDecoder()
        decoder.finish()
        self.assertTrue(decoder.closed)

    def test_full_mcp_response_limit_includes_text_duplicate(self):
        content = {"text": "x" * (protocol.MAX_MESSAGE_BYTES // 2)}
        protocol.encode_message(content)
        combined = {"jsonrpc": "2.0", "id": 1, "result": {"structuredContent": content,
                    "content": [{"type": "text", "text": json.dumps(content)}]}}
        with self.assertRaises(protocol.ProtocolError) as raised:
            protocol.encode_message(combined)
        self.assertEqual(raised.exception.code, "limit_exceeded")

    def test_tool_schemas_fit_actual_tool_list_wire_budget(self):
        tools = [{"name": name, "inputSchema": protocol.INPUT_SCHEMAS[name],
                  "outputSchema": protocol.OUTPUT_SCHEMAS[name]}
                 for name in protocol.INPUT_SCHEMAS]
        wire = protocol.encode_message({"jsonrpc": "2.0", "id": 1, "result": {"tools": tools}})
        self.assertLess(len(wire), protocol.MAX_MESSAGE_BYTES)
        self.assertEqual(set(protocol.INPUT_SCHEMAS), {"ui_session", "ui_observe", "ui_execute", "app_inspect"})


class HandshakeTests(unittest.TestCase):
    def test_exact_private_handshake(self):
        hello = protocol.make_handshake("build-1")
        protocol.validate_handshake(hello, expected_build="build-1")
        for field, value in (("protocol_version", 2), ("build_fingerprint", "build-2"),
                             ("imgui_revision", "other"), ("test_engine_revision", "other")):
            altered = {**hello, field: value}
            with self.assertRaises(protocol.ProtocolError) as raised:
                protocol.validate_handshake(altered, expected_build="build-1")
            self.assertEqual(raised.exception.code, "version_mismatch")
        altered = copy.deepcopy(hello)
        altered["limits"]["steps"] = 128
        with self.assertRaises(protocol.ProtocolError) as raised:
            protocol.validate_handshake(altered, expected_build="build-1")
        self.assertEqual(raised.exception.code, "version_mismatch")

    def test_malformed_handshake(self):
        hello = protocol.make_handshake("build-1")
        hello["command"] = "launch"
        with self.assertRaises(protocol.ProtocolError):
            protocol.validate_handshake(hello, expected_build="build-1")


if __name__ == "__main__":
    unittest.main()
