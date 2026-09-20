"""Authorized native coverage for remaining real workspace widget families."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from real_editor_acceptance import Client, ROOT


def inspect(client, projection, **arguments):
    result = client.tool("app_inspect", {"projection": projection, **arguments})
    assert result["ok"], result
    return {value["field"]: value["value"] for value in result["values"]}


def select(client, kind):
    objects = client.tool("app_inspect", {"projection": "objects", "fields": ["record_type"]})
    assert objects["ok"], objects
    owner = next(value["object_ref"] for value in objects["values"] if value["value"].get("value") == kind)
    row = next(item for item in client.observe() if item["kind"] == "selectable" and item["owner"] == owner)
    client.execute([{"op": "select", "target": {"ref": row["ref"]}}])
    return owner


def run(client, environment):
    started = client.tool("ui_session", {"op": "start", "fixture": "household-interactions", "environment": environment})
    assert started["ok"], started
    client.session = started["session_id"]
    initial = client.document()
    household = client.control("household", "section")
    client.execute([{"op": "close", "target": {"ref": household["ref"]}}])
    assert not client.control("household", "section")["state"]["open"]
    observation = client.tool("ui_observe", {"scope": "root", "depth": 16})
    assert "collapsed" in observation["coverage"]["limitations"]
    client.execute([{"op": "open", "target": {"ref": household["ref"]}}])
    assert client.control("household", "section")["state"]["open"]

    owner = select(client, "actor")
    original = inspect(client, "object", object_ref=owner, fields=["speed"])["speed"]["value"]
    speed = client.control("speed-m-s", "number")
    client.execute([{"op": "edit", "target": {"ref": speed["ref"]}, "value": original + .125, "commit": "tab"}])
    assert abs(inspect(client, "object", object_ref=owner, fields=["speed"])["speed"]["value"] - original - .125) < 1e-5
    client.menu("edit", "undo")
    assert inspect(client, "object", object_ref=owner, fields=["speed"])["speed"]["value"] == original
    # A new runtime packet exercises the mouse gesture and normal release path,
    # independently of the earlier text-input adapter.
    client.execute([{"op": "drag", "target": {"ref": speed["ref"]},
                     "direction": "increase", "fraction": .2, "frames": 8},
        {"op": "assert", "condition": {"source": "app", "projection": "object", "object_ref": owner,
            "field": "speed", "predicate": "not_equals", "expected": original}}])
    client.menu("edit", "undo")
    assert inspect(client, "object", object_ref=owner, fields=["speed"])["speed"]["value"] == original
    revision = client.document()["revision"]

    # BeginCombo-style options are discovered only after opening the real menu.
    client.action("preview-clip", "open", "combo")
    client.action("walk", "select", "selectable")
    client.action("start-preview", kind="button")
    client.action("pause-preview", kind="button")
    preview = inspect(client, "preview")
    assert preview["character.active"]["value"] and preview["character.paused"]["value"]
    desired = min(.25, preview["character.duration"]["value"] / 2)
    slider = client.control("clip-time", "slider")
    client.execute([{"op": "edit", "target": {"ref": slider["ref"]}, "value": desired, "commit": "enter"},
        {"op": "assert", "condition": {"source": "app", "projection": "preview", "field": "character.time",
            "predicate": "approx", "expected": desired, "tolerance": {"absolute": .002}}}])
    assert client.document()["revision"] == revision
    client.execute([{"op": "drag", "target": {"ref": slider["ref"]},
                     "direction": "increase", "fraction": .1, "frames": 8}])
    assert inspect(client, "preview", fields=["character.time"])["character.time"]["value"] != desired
    assert client.document()["revision"] == revision
    client.action("stop-preview", kind="button")

    select(client, "readable_document")
    readable = inspect(client, "preview", fields=["readable.text", "readable.title", "readable.layout_diagnostics"])
    assert readable["readable.text"]["availability"] == "known" and readable["readable.text"]["value"]
    assert any(item["kind"] == "text" for item in client.observe())
    objects = next(item for item in client.observe() if item["kind"] == "window" and item["label"] == "Objects")
    ref_before = objects["ref"]
    client.execute([{"op": "scroll", "target": {"ref": ref_before}, "direction": "down", "pages": 1}])
    client.execute([{"op": "scroll", "target": {"ref": ref_before}, "direction": "up", "pages": 1}])
    assert next(item for item in client.observe() if item["kind"] == "window" and item["label"] == "Objects")["ref"] == ref_before
    assert client.document()["revision"] == revision
    assert client.document()["ambient_intensity"] == initial["ambient_intensity"]
    return {"widgets": "passed", "visual": "not_run", "maximum_response_bytes": client.maximum_response,
            "round_trip_ms": client.timings, "families": ["CollapsingHeader", "DragFloat", "BeginCombo",
                "SliderFloat", "drag_release", "preview_text", "window_scroll"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--transcript", required=True, type=Path)
    args = parser.parse_args()
    source = ROOT / "resources/levels/household-interactions.level.json"
    original = hashlib.sha256(source.read_bytes()).hexdigest()
    client = Client(args.environment, args.transcript)
    summary = {"widgets": "failed", "visual": "not_run"}
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
