"""Strict v1 value contracts shared by the editor automation host and tests.

This module performs no I/O, launches no process and implements no UI action.
INPUT_SCHEMAS and OUTPUT_SCHEMAS are JSON Schema 2020-12 objects suitable for
MCP tools/list. validate_request/result enforce those schemas plus UTF-8 byte
and cross-field constraints that JSON Schema cannot express. Projection fields
are explicit wire names, not C++ member traversal. Adding a new persisted field
requires an intentional read-only projection and a corresponding schema update.

JsonLineDecoder accepts arbitrary byte fragments. Its owner must call expire()
while awaiting input, cancel its session on ProtocolError, and discard the
connection; the decoder becomes permanently closed on any framing/JSON error.
encode_message applies the byte budget to the *complete* wire object, including
MCP's duplicated text and structured content. No unbounded readline is needed.
"""

from __future__ import annotations

import json
import math
import re
import time

PROTOCOL_VERSION = 1
MCP_PROTOCOL_VERSION = "2025-11-25"
IMGUI_REVISION = "b48d1afbe8ee8b238e2961dc363a949dd7304e23"
TEST_ENGINE_REVISION = "2628e39cc0ea3a0a612d5d039543c9d4e873c720"
MAX_MESSAGE_BYTES = 1024 * 1024
MAX_JSON_DEPTH = 32
MAX_INPUT_BYTES = 16 * 1024
MAX_SAFE_INTEGER = 2**53 - 1
LIMITS = {
    "message_bytes": MAX_MESSAGE_BYTES, "json_depth": MAX_JSON_DEPTH,
    "steps": 64, "items_per_page": 256, "items_per_snapshot": 4096,
    "scope_depth": 16, "input_bytes": MAX_INPUT_BYTES,
    "partial_line_ms": 5000, "operation_default_ms": 5000,
    "operation_max_ms": 30000, "batch_default_ms": 20000,
    "batch_max_ms": 60000, "cleanup_ms": 2000, "startup_ms": 30000,
    "idle_session_ms": 300000, "session_max_ms": 3600000,
}
OPERATIONS = (
    "activate", "focus", "open", "close", "set_checked", "select", "edit",
    "commit", "key", "scroll", "drag", "assert", "wait_until",
)
CHORDS = (
    "Enter", "Escape", "Tab", "Shift+Tab", "Left", "Right", "Up", "Down",
    "Home", "End", "PageUp", "PageDown", "Ctrl+Z", "Ctrl+Y", "Ctrl+S",
    "Ctrl+Shift+Z",
)
SESSION_STATES = ("starting", "ready", "executing", "faulted", "closing", "closed")
ERROR_CODES = (
    "invalid_request", "limit_exceeded", "version_mismatch", "busy",
    "session_closed", "environment_not_authorized", "environment_unavailable",
    "not_found", "ambiguous_target", "stale_ref", "snapshot_expired",
    "state_conflict", "disabled", "blocked_by_modal", "ui_unavailable",
    "unsupported", "value_unavailable", "assertion_failed", "policy_denied",
    "timeout", "cancelled", "disconnected", "engine_error", "session_faulted",
    "request_id_conflict", "result_expired",
)
AVAILABILITY = ("known", "unknown", "unavailable", "not_applicable", "truncated")
KINDS = (
    "button", "menu_item", "checkbox", "selectable", "combo", "text",
    "multiline_text", "number", "vector", "slider", "color", "section",
    "window", "menu", "popup", "child", "diagnostic", "viewport",
)


class ProtocolError(ValueError):
    """A bounded diagnostic; code uses the domain/protocol error vocabulary."""

    def __init__(self, code: str, message: str):
        self.code = code
        self.message = message[:512]
        super().__init__(self.message)


def _object(properties, required=None, **constraints):
    return {"type": "object", "properties": properties,
            "required": list(properties) if required is None else list(required),
            "additionalProperties": False, **constraints}


def _enum(values):
    return {"type": "string", "enum": list(values)}


def _string(maximum=MAX_INPUT_BYTES, minimum=0, **constraints):
    return {"type": "string", "minLength": minimum, "maxLength": maximum,
            "x-maxUtf8Bytes": maximum, **constraints}


def _integer(minimum=0, maximum=MAX_SAFE_INTEGER):
    return {"type": "integer", "minimum": minimum, "maximum": maximum}


def _array(items, maximum=256, minimum=0, unique=False):
    return {"type": "array", "items": items, "minItems": minimum,
            "maxItems": maximum, **({"uniqueItems": True} if unique else {})}


def _nullable(schema):
    return {"oneOf": [schema, {"type": "null"}]}


ID = _string(256, 1)
SEQUENCE = _string(32, 1, pattern=r"^[1-9][0-9]*$")
REVISION = _string(32, 1)
TEXT = _string()
NUMBER = {"type": "number"}
BOOL = {"type": "boolean"}
ATOM = {"oneOf": [BOOL, NUMBER, TEXT, {"type": "null"}]}
SELECTOR = _object({"scope": ID, "key": ID, "owner": ID}, ["scope", "key"])
TARGET = {"oneOf": [_object({"ref": ID}), _object({"selector": SELECTOR})]}
VECTOR = _object({axis: NUMBER for axis in "xyz"})
COLOR = _object({axis: NUMBER for axis in "rgba"}, ["r", "g", "b"])
COLLISION_BOX = _object({"center": VECTOR, "half_extent": VECTOR})
DATA_TYPES = {
    "boolean": BOOL, "integer": _integer(-MAX_SAFE_INTEGER), "float": NUMBER,
    "string": TEXT, "optional_string": _nullable(TEXT), "ref": _nullable(ID),
    "vector3": VECTOR, "color": COLOR,
    "strings": _array(TEXT), "numbers": _array(NUMBER),
    "collision_boxes": _array(COLLISION_BOX, 8),
    "range": _object({"min": NUMBER, "max": NUMBER}, [], minProperties=1),
}

# These fields copy EditorObjectValue alternatives in level_document.hpp.
# Components are finite explicitly enumerated aliases; no path interpreter.
OBJECT_FIELDS = {
    "ref", "record_type", "id", "kind", "material", "model", "yaw_degrees",
    "uniform_scale", "collision_boxes", "light_id", "intensity", "radius",
    "initially_on", "casts_shadows", "closed_yaw_degrees", "width", "height",
    "thickness", "open_angle_degrees", "speed_degrees_per_second", "lock_side",
    "initially_open", "initially_locked", "clip", "caption", "loop", "spatial",
    "cue", "gain", "near_distance", "far_distance", "autoplay", "room_a",
    "room_b", "door", "closed_gain", "open_gain", "initial_mark",
    "initial_route", "speed", "footstep_source", "interaction_source",
    "actor", "marks", "final_clip", "title", "pages", "prop", "source",
}
for _field in ("position", "center", "half_extent", "translation",
               "hinge_position", "feet_position", "foot_position"):
    OBJECT_FIELDS.add(_field)
    OBJECT_FIELDS.update(f"{_field}.{axis}" for axis in "xyz")
OBJECT_FIELDS.update(("color", "color.r", "color.g", "color.b", "color.a"))
PROJECTION_FIELDS = {
    "document": ("generation", "revision", "object_revision", "path", "slot",
                 "dirty", "pending_action", "valid", "validation_summary",
                 "source_version", "default_entry", "ambient_intensity",
                 "terrain_material", "solid_count", "entry_count", "light_count",
                 "switch_count", "door_count", "prop_count", "audio_cue_count",
                 "audio_source_count", "audio_room_count", "audio_connection_count",
                 "actor_count", "mark_count", "route_count", "box_count",
                 "document_count", "radio_count"),
    "selection": ("object_ref", "revision", "record_type"),
    "history": ("can_undo", "can_redo", "summary"),
    "object": tuple(sorted(OBJECT_FIELDS)),
    "objects": tuple(sorted(OBJECT_FIELDS)),
    "diagnostics": ("category", "source_path", "document_path", "message",
                    "terrain_x", "terrain_z", "terrain_triangle", "edit_error"),
    "preview": ("audition.active", "audition.muted", "audition.paused",
                "audition.source", "audition.warning", "audition.listener_room",
                "audition.source_room", "audition.gain", "audition.caption",
                "character.active", "character.paused", "character.time",
                "character.duration", "character.clip", "character.actor",
                "character.mode", "character.stage", "character.segment",
                "character.target_mark", "character.final_clip", "character.error",
                "readable.page", "readable.text", "readable.title",
                "readable.layout_diagnostics", "readable.validation_diagnostics",
                "resource_revision", "resource_current", "error"),
}
UI_FIELDS = (
    "label", "kind", "state.submitted", "state.visible", "state.enabled",
    "state.active", "state.focused", "state.selected", "state.open",
    "value.draft", "value.availability", "input.text", "input.uncommitted",
    "input.availability", "commit.policy", "capabilities", "count",
    "input_validation.status",
)


def _condition_schema():
    predicates = [
        {"predicate": {"const": "exists"}, "expected": BOOL},
        {"predicate": _enum(("equals", "not_equals")), "expected":
            {"oneOf": [ATOM, VECTOR, COLOR, _array(ATOM)]}},
        {"predicate": {"const": "range"}, "expected":
            _object({"min": NUMBER, "max": NUMBER}, [], minProperties=1)},
        {"predicate": {"const": "approx"}, "expected": NUMBER, "tolerance":
            _object({"absolute": {"type": "number", "minimum": 0},
                     "relative": {"type": "number", "minimum": 0}},
                    [], minProperties=1)},
        {"predicate": {"const": "contains"}, "expected": TEXT},
        {"predicate": {"const": "count"}, "expected": _integer(0, 4096)},
    ]
    sources = [{"source": {"const": "ui"}, "target": TARGET,
                "field": _enum(UI_FIELDS)}]
    for projection, fields in PROJECTION_FIELDS.items():
        source = {"source": {"const": "app"}, "projection": {"const": projection},
                  "field": _enum(fields)}
        if projection == "object":
            source["object_ref"] = ID
        sources.append(source)
    return {"oneOf": [_object({**source, **predicate})
                      for source in sources for predicate in predicates]}


CONDITION = _condition_schema()
_steps = []
for _operation in OPERATIONS:
    _base = {"op": {"const": _operation}, "timeout_ms": _integer(1, 30000)}
    _required = ["op"]
    if _operation in ("assert", "wait_until"):
        _base["condition"] = CONDITION
        _required.append("condition")
    else:
        _base["target"] = TARGET
        _required.append("target")
    _payloads = [{}]
    if _operation == "set_checked":
        _payloads = [{"value": BOOL}]
    elif _operation == "commit":
        _payloads = [{"method": _enum(("enter", "tab"))}]
    elif _operation == "key":
        _payloads = [{"chord": _enum(CHORDS)}]
    elif _operation == "edit":
        _base["commit"] = _enum(("none", "enter", "tab"))
        _payloads = [{"value": NUMBER}, {"text": TEXT}]
    elif _operation == "scroll":
        _payloads = [{"direction": _enum(("up", "down")), "pages": _integer(1, 16)},
                     {"to": TARGET}]
    elif _operation == "drag":
        _base["frames"] = _integer(2, 120)
        _payloads = [{"direction": _enum(("increase", "decrease")),
                      "fraction": {"type": "number", "exclusiveMinimum": 0,
                                   "maximum": 1}}]
    _steps.extend(_object({**_base, **payload}, _required + list(payload))
                  for payload in _payloads)
STEP = {"oneOf": _steps}
COMMON_REQUEST = {"protocol_version": {"type": "integer", "const": 1},
                  "request_id": ID}
SESSION_REQUEST = {**COMMON_REQUEST, "session_id": ID}
_session = []
for _operation in ("start", "status", "cancel", "close"):
    _properties = dict(COMMON_REQUEST if _operation == "start" else SESSION_REQUEST)
    _properties["op"] = {"const": _operation}
    if _operation == "start":
        # Authorization/fixture manifest checks are runtime policy, not syntax.
        _properties.update(fixture=ID, environment=ID)
    if _operation == "cancel":
        _properties["active_request_id"] = SEQUENCE
    _session.append(_object(_properties,
                            [key for key in _properties
                             if not (_operation == "status" and key == "session_id")]))
_inspect = []
for _projection, _fields in PROJECTION_FIELDS.items():
    _properties = {**SESSION_REQUEST, "projection": {"const": _projection},
                   "fields": _array(_enum(_fields), 128, 1, True),
                   "snapshot_id": ID, "page_size": _integer(1, 256)}
    _required = [*SESSION_REQUEST, "projection"]
    if _projection == "object":
        _properties["object_ref"] = ID
        _required.append("object_ref")
    _inspect.append(_object(_properties, _required))
_continuation = _object({**SESSION_REQUEST, "cursor": ID})
INPUT_SCHEMAS = {
    "ui_session": {"type": "object", "oneOf": _session},
    "ui_observe": {"type": "object", "oneOf": [
        _object({**SESSION_REQUEST, "scope": ID,
                 "filter": _object({"key": ID, "kind": _enum(KINDS), "owner": ID},
                                   [], minProperties=1),
                 "depth": _integer(0, 16), "page_size": _integer(1, 256)},
                [*SESSION_REQUEST, "scope"]), _continuation]},
    "app_inspect": {"type": "object", "oneOf": [*_inspect, _continuation]},
    "ui_execute": _object({**SESSION_REQUEST, "request_id": SEQUENCE,
                           "steps": _array(STEP, 64, 1),
                           "timeout_ms": _integer(1, 60000),
                           "policy": _object({"auto_scroll": BOOL, "auto_focus": BOOL}, [], minProperties=1),
                           "if_state": _object({"document_generation": REVISION,
                                                "document_revision": REVISION},
                                               [], minProperties=1)},
                          [*SESSION_REQUEST, "steps"]),
}


def _available(payload_key="value"):
    variants = []
    for value_type, schema in DATA_TYPES.items():
        variants.append(_object({"availability": {"const": "known"},
                                 "type": {"const": value_type}, payload_key: schema,
                                 "unit": _string(64), "encoding": _string(64),
                                 "rounding": _string(128)},
                                ["availability", "type", payload_key]))
    variants += [
        _object({"availability": _enum(AVAILABILITY[1:4]), "reason": _string(512)},
                ["availability"]),
    ]
    for value_type in ("string", "strings", "numbers"):
        variants.append(_object({"availability": {"const": "truncated"},
                                 "type": {"const": value_type},
                                 payload_key: DATA_TYPES[value_type],
                                 "reason": _string(512), "total_count": _integer()},
                                ["availability", "type", payload_key, "reason"]))
    return {"oneOf": variants}


AVAILABLE = _available()
UNAVAILABLE = _object({"availability": _enum(AVAILABILITY[1:4]),
                       "reason": _string(512)}, ["availability"])
STATE_VALUE = {"oneOf": [BOOL, UNAVAILABLE]}
INPUT_VALUE = {"oneOf": [
    _object({"availability": {"const": "known"}, "text": TEXT,
             "uncommitted": BOOL}), UNAVAILABLE,
    _object({"availability": {"const": "truncated"}, "text": TEXT,
             "uncommitted": BOOL, "reason": _string(512)})]}
STAMP = _object({"snapshot_id": ID, "frame": REVISION,
                 "document_generation": REVISION, "document_revision": REVISION,
                 "selection_revision": REVISION, "preview_revision": REVISION,
                 "stale": BOOL})
BINDING = {"oneOf": [
    _object({"projection": {"const": name}, "field": _enum(fields)})
    for name, fields in PROJECTION_FIELDS.items()]}
ITEM = _object({
    "ref": ID, "scope": ID, "key": ID, "owner": _nullable(ID), "label": TEXT,
    "label_availability": _enum(("known", "truncated")),
    "kind": _enum(KINDS), "capabilities": _array(_enum(OPERATIONS), len(OPERATIONS), unique=True),
    "state": _object({name: STATE_VALUE for name in
                       ("submitted", "visible", "enabled", "active", "focused",
                        "selected", "open")}),
    "value": _available("draft"), "input": INPUT_VALUE,
    "commit": _object({"policy": _enum(("immediate", "enter", "deactivate",
                                         "explicit", "not_applicable")),
                       "methods": _array(_enum(("enter", "tab")), 2, unique=True)}),
    "applied_binding": _nullable(BINDING),
    "provenance": _object({"identity": _enum(("ui_metadata", "test_engine")),
                           "state": _enum(("imgui", "ui_metadata", "test_engine")),
                           "value": _enum(("ui_draft", "ui_metadata", "imgui",
                                           "unavailable"))}),
    "capacity_bytes": _integer(0, MAX_INPUT_BYTES),
    "components": _array(_object({"key": ID, "ref": ID}), 4),
    "range": _object({"min": NUMBER, "max": NUMBER}),
    "chords": _array(_enum(CHORDS), len(CHORDS), unique=True),
    "unsupported_reason": _string(512),
    "ref_lifetime": _enum(("semantic", "snapshot")),
    "engine": _object({"availability": _enum(("known", "unknown")),
                        "frame": REVISION, "status_frame": REVISION,
                        "reason": _string(512)}, ["availability"]),
    "drag": _object({"axis": {"const": "horizontal"},
                      "distance_unit": {"const": "widget_width"}}),
    "input_validation": _object({"status": _enum(("valid", "invalid", "unknown")),
                                  "reason": _string(512)}, ["status"]),
}, ["ref", "scope", "key", "owner", "label", "kind", "capabilities", "state",
    "value", "input", "commit", "applied_binding", "provenance"])
COVERAGE = _object({
    "limitations": _array(_enum(("submitted_only", "collapsed", "closed_popup",
                                  "clipped_unsubmitted", "unregistered",
                                  "pagination", "truncation", "depth_limit")), 8, unique=True),
    "snapshot_item_count": _integer(0, 4096), "truncated": BOOL,
    "scopes": _array(_object({"scope": ID, "limitations":
                              _array(_enum(("collapsed", "closed_popup",
                                            "clipped_unsubmitted", "unregistered")),
                                     4, unique=True)})),
})
ERROR = _object({
    "code": _enum(ERROR_CODES), "message": _string(512),
    "step_index": _nullable(_integer(0, 63)), "target": _nullable(TARGET),
    "last_completed_index": _nullable(_integer(0, 63)),
    "expected": _nullable(AVAILABLE), "observed": _nullable(AVAILABLE),
    "snapshot": _nullable(STAMP), "effects": _enum(("none", "partial", "unknown")),
    "cleanup": _enum(("released", "process_terminated", "unverified")),
    "session_state": _enum(SESSION_STATES), "source": _enum(("ui", "app")),
    "field": ID, "blocking_scope": ID,
    "condition": CONDITION,
    "capabilities": _array(_enum(OPERATIONS), len(OPERATIONS), unique=True),
    "target_state": ITEM["properties"]["state"],
    "candidates": _array(_object({"ref": ID, "scope": ID, "key": ID,
                                  "owner": _nullable(ID)})),
}, ["code", "message", "step_index", "target", "last_completed_index", "expected",
    "observed", "snapshot", "effects", "cleanup", "session_state"])
COMMON_RESULT = {**COMMON_REQUEST, "session_id": _nullable(ID),
                 "build_fingerprint": ID}
ASSISTANCE = _array(_object({"action": _enum(("scroll", "focus")),
                             "target": TARGET, "before": STAMP, "after": STAMP}), 128)
_step_results = []
for _status in ("passed", "failed", "not_run", "unknown"):
    _properties = {"index": _integer(0, 63), "status": {"const": _status},
                   "before": _nullable(STAMP), "after": _nullable(STAMP)}
    if _status == "passed":
        _properties["observed"] = AVAILABLE
    elif _status == "failed":
        _properties["error"] = ERROR
    _required_result = list(_properties)
    _properties["assistance"] = ASSISTANCE
    _step_results.append(_object(_properties, _required_result))
STEP_RESULT = {"oneOf": _step_results}
_execution = {
    "steps": _array(STEP_RESULT, 64), "before": _nullable(STAMP),
    "after": _nullable(STAMP), "last_completed_index": _nullable(_integer(0, 63)),
    "failed_index": _nullable(_integer(0, 63)),
    "effects": _enum(("none", "partial", "unknown")),
    "cleanup": _enum(("released", "process_terminated", "unverified")),
    "session_state": _enum(SESSION_STATES),
}
_inspect_values = []
for _projection, _fields in PROJECTION_FIELDS.items():
    _inspect_values.append(_object({
        "projection": {"const": _projection}, "field": _enum(_fields),
        "object_ref": _nullable(ID), "value": AVAILABLE,
        "source": {"const": "document" if _projection in ("object", "objects") else _projection},
    }))
_success = {
    "ui_session": {"state": _enum(SESSION_STATES),
        "capabilities": _array(_enum(OPERATIONS), len(OPERATIONS), unique=True),
        "limits": _object({key: {"type": "integer", "const": value}
                            for key, value in LIMITS.items()}),
        "file_slots": _array(_object({"slot": ID, "path": _string(4096),
                                       "writable": BOOL}), 32),
        "last_request": _nullable(_object({"request_id": ID,
            "status": _enum(("running", "passed", "failed")),
            "error": _nullable(ERROR)})),
        "excluded": _array(_enum(("viewport_picking", "placement", "sculpting",
            "navigation", "gizmos", "docking", "os_dialogs", "game_process")),
            8, unique=True)},
    "ui_observe": {"snapshot": STAMP, "active_scope": _nullable(ID),
        "modal_scope": _nullable(ID), "items": _array(ITEM),
        "coverage": COVERAGE, "next_cursor": _nullable(ID)},
    "app_inspect": {"snapshot": STAMP, "values": _array({"oneOf": _inspect_values}),
        "next_cursor": _nullable(ID), "truncated": BOOL},
    "ui_execute": _execution,
}
OUTPUT_SCHEMAS = {}
for _tool, _payload in _success.items():
    _failure = {**COMMON_RESULT, "ok": {"const": False}, "error": ERROR}
    if _tool == "ui_execute":
        _failure["request_id"] = SEQUENCE
        _failure.update(_execution)
    _variants = [_object({**COMMON_RESULT, "ok": {"const": True}, **_payload}),
                 _object(_failure)]
    if _tool == "ui_execute":
        # Identical retry while its original request is still executing.
        _variants.append(_object({**COMMON_RESULT, "ok": {"const": True},
                                  "status": {"const": "running"},
                                  "session_state": {"const": "executing"}}))
    OUTPUT_SCHEMAS[_tool] = {"type": "object", "oneOf": _variants}
    for _variant in _variants:
        # Host-only artifact location, optional on private editor replies.
        _variant["properties"]["diagnostic_path"] = _string(4096)
    if _tool == "ui_execute":
        for _variant in _variants:
            _variant["properties"]["request_id"] = SEQUENCE

for _schema in (*INPUT_SCHEMAS.values(), *OUTPUT_SCHEMAS.values()):
    _schema["$schema"] = "https://json-schema.org/draft/2020-12/schema"

HANDSHAKE_SCHEMA = _object({
    "kind": {"const": "hello"}, "protocol_version": _integer(1),
    "build_fingerprint": ID, "imgui_revision": ID, "test_engine_revision": ID,
    "limits": _object({key: _integer(1) for key in LIMITS}),
})


def _validate(value, schema, path="$", depth=0):
    """Evaluate only the JSON Schema vocabulary emitted above, fail closed."""
    if depth > MAX_JSON_DEPTH:
        raise ProtocolError("limit_exceeded", f"{path}: excessive nesting")
    if "oneOf" in schema:
        matches = 0
        for alternative in schema["oneOf"]:
            try:
                _validate(value, alternative, path, depth)
                matches += 1
            except ProtocolError:
                pass
        if matches != 1:
            raise ProtocolError("invalid_request", f"{path}: no unique schema variant")
    expected = schema.get("type")
    checks = {"object": lambda: type(value) is dict,
              "array": lambda: type(value) is list,
              "string": lambda: type(value) is str,
              "integer": lambda: type(value) is int,
              "number": lambda: type(value) in (int, float),
              "boolean": lambda: type(value) is bool,
              "null": lambda: value is None}
    if expected and not checks[expected]():
        raise ProtocolError("invalid_request", f"{path}: expected {expected}")
    if "const" in schema and (type(value) is not type(schema["const"]) or
                               value != schema["const"]):
        raise ProtocolError("invalid_request", f"{path}: invalid constant")
    if "enum" in schema and value not in schema["enum"]:
        raise ProtocolError("invalid_request", f"{path}: unsupported value")
    if expected == "object":
        properties = schema.get("properties")
        if properties is None:  # top-level oneOf wrapper only
            return
        if set(value) - properties.keys() or set(schema["required"]) - value.keys():
            raise ProtocolError("invalid_request", f"{path}: unknown or missing field")
        if len(value) < schema.get("minProperties", 0):
            raise ProtocolError("invalid_request", f"{path}: empty object")
        for key, item in value.items():
            _validate(item, properties[key], f"{path}.{key}", depth + 1)
    elif expected == "array":
        if not schema["minItems"] <= len(value) <= schema["maxItems"]:
            raise ProtocolError("limit_exceeded", f"{path}: array size out of bounds")
        if schema.get("uniqueItems") and len({json.dumps(v, sort_keys=True)
                                             for v in value}) != len(value):
            raise ProtocolError("invalid_request", f"{path}: duplicate array value")
        for index, item in enumerate(value):
            _validate(item, schema["items"], f"{path}[{index}]", depth + 1)
    elif expected == "string":
        try:
            size = len(value.encode("utf-8"))
        except UnicodeError as error:
            raise ProtocolError("invalid_request", f"{path}: invalid Unicode") from error
        if not schema.get("minLength", 0) <= len(value) <= schema.get("maxLength", MAX_INPUT_BYTES):
            raise ProtocolError("limit_exceeded", f"{path}: string length out of bounds")
        if size > schema.get("x-maxUtf8Bytes", MAX_INPUT_BYTES):
            raise ProtocolError("limit_exceeded", f"{path}: UTF-8 byte limit exceeded")
        if "pattern" in schema and re.fullmatch(schema["pattern"], value) is None:
            raise ProtocolError("invalid_request", f"{path}: invalid identifier")
    elif expected in ("number", "integer"):
        if type(value) is float and not math.isfinite(value):
            raise ProtocolError("invalid_request", f"{path}: number must be finite")
        if type(value) is int and abs(value) > MAX_SAFE_INTEGER:
            raise ProtocolError("invalid_request", f"{path}: encode large integers as strings")
        if value < schema.get("minimum", -math.inf) or value > schema.get("maximum", math.inf):
            raise ProtocolError("limit_exceeded", f"{path}: number out of bounds")
        if "exclusiveMinimum" in schema and value <= schema["exclusiveMinimum"]:
            raise ProtocolError("limit_exceeded", f"{path}: number outside exclusive bounds")


def validate_request(tool: str, request: dict) -> None:
    """Validate a complete batch before dispatch; no target lookup or mutation."""
    if tool not in INPUT_SCHEMAS:
        raise ProtocolError("invalid_request", "Unknown automation tool")
    encode_message(request)
    _validate(request, INPUT_SCHEMAS[tool])
    for step in request.get("steps", []):
        condition = step.get("condition", {})
        if condition.get("predicate") == "range":
            bounds = condition["expected"]
            if bounds.get("min", -math.inf) > bounds.get("max", math.inf):
                raise ProtocolError("invalid_request", "Range minimum exceeds maximum")


def validate_result(tool: str, result: dict, *, request: dict | None = None) -> None:
    """Validate result shape, fail-fast ordering, and optional request pairing."""
    if tool not in OUTPUT_SCHEMAS:
        raise ProtocolError("invalid_request", "Unknown automation tool")
    encode_message(result)
    _validate(result, OUTPUT_SCHEMAS[tool])
    if request is not None:
        validate_request(tool, request)
        for key in ("protocol_version", "request_id", "session_id"):
            if key in request and request[key] != result[key]:
                raise ProtocolError("invalid_request", "Response identity does not match request")
    if result["ok"] and tool == "ui_observe":
        if result["snapshot"]["stale"]:
            raise ProtocolError("invalid_request", "Fresh observation cannot claim stale success")
        if len(result["items"]) > result["coverage"]["snapshot_item_count"]:
            raise ProtocolError("invalid_request", "Page exceeds declared snapshot item count")
    if result["ok"] and tool == "app_inspect" and request and "cursor" not in request:
        for value in result["values"]:
            if value["projection"] != request["projection"]:
                raise ProtocolError("invalid_request", "Projection does not match request")
            if "fields" in request and value["field"] not in request["fields"]:
                raise ProtocolError("invalid_request", "Unrequested projection field")
            if "object_ref" in request and value["object_ref"] != request["object_ref"]:
                raise ProtocolError("invalid_request", "Projection object does not match request")
    if tool != "ui_execute" or result.get("status") == "running":
        return
    steps = result["steps"]
    if request is not None and len(steps) != len(request["steps"]):
        raise ProtocolError("invalid_request", "Result must include every requested step")
    stopped = False
    uncertain = False
    has_uncertain = False
    last_completed = failed = None
    for index, step in enumerate(steps):
        if step["index"] != index:
            raise ProtocolError("invalid_request", "Nonsequential step result index")
        status = step["status"]
        if stopped and status != "not_run" and not (uncertain and status == "unknown"):
            raise ProtocolError("invalid_request", "Executed step after stopped prefix")
        if status == "passed":
            last_completed = index
        else:
            stopped = True
            if status == "unknown":
                uncertain = True
                has_uncertain = True
                if any(step[stamp] is not None and not step[stamp]["stale"] for stamp in ("before", "after")):
                    raise ProtocolError("invalid_request", "Unverified step cannot claim fresh evidence")
            if status == "not_run":
                uncertain = False
            if status == "failed":
                failed = index
                if step["error"]["step_index"] != index:
                    raise ProtocolError("invalid_request", "Failed step/error index mismatch")
        if status == "not_run" and (step["before"] is not None or step["after"] is not None):
            raise ProtocolError("invalid_request", "Unexecuted step cannot claim frame evidence")
    if result["last_completed_index"] != last_completed or result["failed_index"] != failed:
        raise ProtocolError("invalid_request", "Incorrect completed/failed step index")
    if has_uncertain and (result["ok"] or result["effects"] != "unknown" or
                      result["cleanup"] not in ("process_terminated", "unverified")):
        raise ProtocolError("invalid_request", "Unverified steps require explicit fatal uncertainty")
    if result["ok"] and (not steps or stopped or result["cleanup"] != "released"):
        raise ProtocolError("invalid_request", "Successful batch requires all steps and cleanup")
    if not result["ok"]:
        for key in ("last_completed_index", "effects", "cleanup", "session_state"):
            if result["error"][key] != result[key]:
                raise ProtocolError("invalid_request", "Inconsistent batch/error evidence")
        if result["error"]["step_index"] != failed:
            raise ProtocolError("invalid_request", "Inconsistent failure index")


def make_handshake(build_fingerprint: str) -> dict:
    """Create the private hello; readiness follows checked handshake/policy."""
    result = {"kind": "hello", "protocol_version": PROTOCOL_VERSION,
              "build_fingerprint": build_fingerprint, "imgui_revision": IMGUI_REVISION,
              "test_engine_revision": TEST_ENGINE_REVISION, "limits": dict(LIMITS)}
    _validate(result, HANDSHAKE_SCHEMA)
    return result


def validate_handshake(message: dict, *, expected_build: str) -> None:
    encode_message(message)
    _validate(message, HANDSHAKE_SCHEMA)
    if message != make_handshake(expected_build):
        raise ProtocolError("version_mismatch", "Private protocol/build/dependency/limits mismatch")


def _check_tree(value, depth=0):
    if depth > MAX_JSON_DEPTH or (type(value) in (dict, list) and depth >= MAX_JSON_DEPTH):
        raise ProtocolError("limit_exceeded", "JSON nesting limit exceeded")
    if type(value) is dict:
        for key, child in value.items():
            if type(key) is not str:
                raise ProtocolError("invalid_request", "JSON object keys must be strings")
            _check_tree(key, depth)
            _check_tree(child, depth + 1)
    elif type(value) is list:
        for child in value:
            _check_tree(child, depth + 1)
    elif type(value) is int:
        if abs(value) > MAX_SAFE_INTEGER:
            raise ProtocolError("invalid_request", "Encode large integers as strings")
    elif type(value) is float:
        if not math.isfinite(value):
            raise ProtocolError("invalid_request", "JSON number must be finite")
    elif type(value) is str:
        try:
            value.encode("utf-8")
        except UnicodeError as error:
            raise ProtocolError("invalid_request", "Invalid Unicode scalar value") from error
    elif type(value) not in (str, bool, type(None)):
        raise ProtocolError("invalid_request", "Non-JSON value")


def encode_message(message: dict, *, max_bytes: int = MAX_MESSAGE_BYTES) -> bytes:
    """Serialize one complete wire object including its terminating newline."""
    if type(message) is not dict:
        raise ProtocolError("invalid_request", "Protocol message must be an object")
    _check_tree(message)
    data = bytearray()
    try:
        for chunk in json.JSONEncoder(ensure_ascii=False, allow_nan=False,
                                      separators=(",", ":")).iterencode(message):
            encoded = chunk.encode("utf-8")
            if len(data) + len(encoded) + 1 > max_bytes:
                raise ProtocolError("limit_exceeded", "Serialized message byte limit exceeded")
            data.extend(encoded)
    except (ValueError, UnicodeError) as error:
        if isinstance(error, ProtocolError):
            raise
        raise ProtocolError("invalid_request", "Message cannot be encoded as UTF-8 JSON") from error
    data.append(10)
    return bytes(data)


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ProtocolError("invalid_request", "Duplicate JSON object key")
        result[key] = value
    return result


def _reject_constant(_value):
    raise ProtocolError("invalid_request", "JSON number must be finite")


class JsonLineDecoder:
    """Bounded byte framing for both channels; feed returns complete objects.

    max_bytes includes the newline. A partial line's timer starts at its first
    byte and never extends when another fragment arrives. feed/expire accept an
    optional monotonic timestamp for deterministic tests; production omits it.
    Memory retained by the decoder is at most one bounded partial message.
    """

    def __init__(self, *, max_bytes=MAX_MESSAGE_BYTES, max_depth=MAX_JSON_DEPTH,
                 partial_timeout_ms=5000):
        if not 2 <= max_bytes <= MAX_MESSAGE_BYTES or not 1 <= max_depth <= MAX_JSON_DEPTH:
            raise ValueError("Invalid framing limits")
        if not 0 < partial_timeout_ms <= 5000:
            raise ValueError("Invalid partial-line timeout")
        self.max_bytes = max_bytes
        self.max_depth = max_depth
        self.partial_timeout = partial_timeout_ms / 1000
        self.closed = False
        self._buffer = bytearray()
        self._started = None
        self._depth = 0
        self._quoted = False
        self._escaped = False

    @property
    def buffered_bytes(self):
        return len(self._buffer)

    def _fail(self, code, message):
        self.closed = True
        self._buffer.clear()
        raise ProtocolError(code, message)

    def expire(self, now=None):
        """Check partial-line deadline even when a peer sends no more bytes."""
        if self.closed:
            raise ProtocolError("disconnected", "Decoder connection is closed")
        now = time.monotonic() if now is None else now
        if self._started is not None and now - self._started >= self.partial_timeout:
            self._fail("timeout", "Partial JSON line exceeded its deadline")

    def feed(self, data: bytes, *, now=None) -> list[dict]:
        now = time.monotonic() if now is None else now
        self.expire(now)
        if len(data) > MAX_MESSAGE_BYTES:
            self._fail("limit_exceeded", "Read fragment exceeds channel byte budget")
        messages = []
        for byte in data:
            if self._started is None:
                self._started = now
            if len(self._buffer) + 1 > self.max_bytes or (byte != 10 and
                    len(self._buffer) + 2 > self.max_bytes):
                self._fail("limit_exceeded", "JSON line byte limit exceeded")
            if byte == 10:
                try:
                    message = json.loads(self._buffer.decode("utf-8"),
                                         object_pairs_hook=_pairs,
                                         parse_constant=_reject_constant)
                    if type(message) is not dict:
                        raise ProtocolError("invalid_request", "Protocol message must be an object")
                    _check_tree(message)
                except (ValueError, UnicodeError, RecursionError) as error:
                    code = error.code if isinstance(error, ProtocolError) else "invalid_request"
                    self._fail(code, "Invalid bounded UTF-8 JSON message")
                messages.append(message)
                self._buffer.clear()
                self._started = None
                self._depth = 0
                self._quoted = self._escaped = False
                continue
            self._buffer.append(byte)
            if self._quoted:
                if self._escaped:
                    self._escaped = False
                elif byte == 92:
                    self._escaped = True
                elif byte == 34:
                    self._quoted = False
            elif byte == 34:
                self._quoted = True
            elif byte in (123, 91):
                self._depth += 1
                if self._depth > self.max_depth:
                    self._fail("limit_exceeded", "JSON nesting limit exceeded before parsing")
            elif byte in (125, 93):
                self._depth -= 1
                if self._depth < 0:
                    self._fail("invalid_request", "Unbalanced JSON delimiters")
        return messages

    def finish(self):
        """Mark EOF; an unterminated message is never executed."""
        if self.closed:
            return
        if self._buffer:
            self._fail("disconnected", "EOF in partial JSON message")
        self.closed = True
