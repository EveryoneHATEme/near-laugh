"""Discover and exercise an added ordinary editor control through runtime JSON.

Requires an authorized desktop. No generated scenario handler, document writes,
screenshots or OS keyboard/mouse input are used.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from real_editor_acceptance import Client, ROOT


def run(client, environment):
    started = client.tool("ui_session", {"op": "start", "fixture": "apartment-stairs", "environment": environment})
    assert started["ok"], started
    client.session = started["session_id"]
    before = client.document()
    assert before["ambient_intensity"]["value"] > 0
    control = client.control("zero-ambient", "button")
    assert control["applied_binding"] == {"projection": "document", "field": "ambient_intensity"}, control
    changed = client.execute([
        {"op": "activate", "target": {"ref": control["ref"]}},
        {"op": "assert", "condition": {"source": "app", "projection": "document", "field": "ambient_intensity",
            "predicate": "equals", "expected": 0}},
        {"op": "assert", "condition": {"source": "app", "projection": "document", "field": "dirty",
            "predicate": "equals", "expected": True}},
    ])
    assert [step["status"] for step in changed["steps"]] == ["passed"] * 3, changed
    client.menu("edit", "undo")
    assert client.document()["ambient_intensity"] == before["ambient_intensity"]
    return {"extension": "passed", "visual": "not_run", "control": control,
            "execution_result": changed, "undo_restored_ambient": True,
            "maximum_response_bytes": client.maximum_response, "round_trip_ms": client.timings}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--transcript", required=True, type=Path)
    args = parser.parse_args()
    source = ROOT / "resources/levels/apartment-stairs.level.json"
    original = hashlib.sha256(source.read_bytes()).hexdigest()
    client = Client(args.environment, args.transcript)
    summary = {"extension": "failed", "visual": "not_run"}
    status = 0
    try:
        summary = run(client, args.environment)
    except Exception as error:
        status = 1
        summary["error"] = str(error)[:2048]
    finally:
        try:
            summary["close_result"] = client.close()
            summary["exit_audit"] = client.child_exit_audit()
            assert summary["exit_audit"]["child_exit_code"] == 0, summary["exit_audit"]
            assert summary["exit_audit"]["forced"] is False, summary["exit_audit"]
            assert summary["exit_audit"]["final_vulkan_errors"] == 0, summary["exit_audit"]
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
