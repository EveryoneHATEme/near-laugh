"""Prepared real-editor failure cases, using only ordinary runtime tool calls.

Requires explicit desktop authorization, like real_editor_acceptance.py.
The source fixture and denied destinations are checked without modifying them.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import time
import uuid

from real_editor_acceptance import Client, ROOT


def run(client, environment):
    started = client.tool("ui_session", {"op": "start", "fixture": "apartment-stairs", "environment": environment})
    assert started["ok"], started
    client.session = started["session_id"]
    initial = client.document()
    ambient = client.control("ambient-0-to-0-20", "number")
    target = {"ref": ambient["ref"]}
    suffix = {"op": "edit", "target": target, "value": .199, "commit": "enter"}
    cases = []

    def rejected(step, code, *, effects="none"):
        result = client.execute([step, suffix], code)
        assert [s["status"] for s in result["steps"]] == ["failed", "not_run"], result
        assert result["cleanup"] == "released" and result["effects"] == effects, result
        assert client.document()["ambient_intensity"] == initial["ambient_intensity"]
        cases.append(code)
        return result

    rejected({"op": "activate", "target": {"selector": {"scope": "root", "key": "missing"}}}, "not_found")
    groups = defaultdict(list)
    for item in client.observe():
        if item["kind"] == "selectable":
            groups[(item["scope"], item["key"])].append(item)
    scope, key = next(identity for identity, items in groups.items() if len(items) > 1)
    ambiguous = rejected({"op": "select", "target": {"selector": {"scope": scope, "key": key}}}, "ambiguous_target")
    assert len(ambiguous["error"]["candidates"]) >= 2
    client.action("edit", "open", "menu")
    rejected({"op": "activate", "target": {"ref": client.control("undo", "menu_item")["ref"]}}, "disabled")
    client.menu("file", "save-as")
    blocked = rejected({"op": "edit", "target": target, "value": .08}, "blocked_by_modal")
    assert blocked["error"]["blocking_scope"]
    client.action("cancel", kind="button")

    source = ROOT / "resources/levels/apartment-stairs.level.json"
    output = Path(next(slot["path"] for slot in started["file_slots"] if slot["slot"] == "output-1"))
    denied_output = output.parent.parent / ("denied-" + uuid.uuid4().hex + ".level.json")
    assert not denied_output.exists()
    for path in (str(denied_output), str(output.parent / ".." / "traversal.level.json"), str(output) + ":stream"):
        client.menu("file", "save-as")
        client.action("path", "edit", "text", text=path, commit="tab")
        rejected({"op": "activate", "target": {"ref": client.control("save", "button")["ref"]}}, "policy_denied", effects="partial")
        client.action("cancel", kind="button")
    assert not denied_output.exists()
    client.menu("file", "open")
    client.action("path", "edit", "text", text=str(source), commit="tab")
    rejected({"op": "activate", "target": {"ref": client.control("open", "button")["ref"]}}, "policy_denied", effects="partial")
    rejected({"op": "activate", "target": {"ref": client.control("play", "button")["ref"]}}, "policy_denied", effects="partial")

    condition = {"source": "app", "projection": "document", "field": "dirty", "predicate": "equals", "expected": True}
    rejected({"op": "wait_until", "condition": condition, "timeout_ms": 150}, "timeout")
    client.execution += 1
    active = str(client.execution)
    pending = client.begin_tool("ui_execute", {"request_id": active, "steps": [
        {"op": "wait_until", "condition": condition, "timeout_ms": 10000}, suffix]})
    # The separate status call demonstrates that execution does not block control.
    deadline = time.monotonic() + 2
    while True:
        status = client.tool("ui_session", {"op": "status"})
        if status["state"] == "executing":
            break
        assert time.monotonic() < deadline, status
    assert client.tool("ui_session", {"op": "cancel", "active_request_id": active})["ok"]
    cancelled = client.end_tool(pending)
    assert cancelled["error"]["code"] == "cancelled", cancelled
    assert cancelled["cleanup"] == "released" and cancelled["effects"] == "none", cancelled
    assert [s["status"] for s in cancelled["steps"]] == ["failed", "not_run"], cancelled
    cases.append("cancelled")

    steps = [{"op": "edit", "target": target, "value": .089, "commit": "enter"}]
    first = client.execute(steps)
    request_id = str(client.execution)
    applied = client.document()
    retry = client.tool("ui_execute", {"request_id": request_id, "steps": steps,
                                       "policy": {"auto_scroll": True, "auto_focus": True}})
    assert retry == first and client.document()["revision"] == applied["revision"]
    conflict = client.tool("ui_execute", {"request_id": request_id, "steps": [suffix]})
    assert conflict["error"]["code"] == "request_id_conflict", conflict
    assert client.document()["revision"] == applied["revision"]
    cases.extend(("identical_retry", "request_id_conflict"))
    return {"semantic_failures": "passed", "visual": "not_run", "cases": cases,
            "maximum_response_bytes": client.maximum_response, "round_trip_ms": client.timings}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--transcript", required=True, type=Path)
    args = parser.parse_args()
    source = ROOT / "resources/levels/apartment-stairs.level.json"
    original = hashlib.sha256(source.read_bytes()).hexdigest()
    client = Client(args.environment, args.transcript)
    summary = {"semantic_failures": "failed", "visual": "not_run"}
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
