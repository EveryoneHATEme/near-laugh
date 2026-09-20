"""Everyday SDK -> production host -> real editor batch regression (level B).

Build affected targets first. --environment names an explicitly authorized test
environment; this command supplies no Windows input or visual acceptance.
"""

import argparse
import asyncio
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from editor_ui_session import EditorUiSession, app_assert, known, selector, target, ui_assert


def fields(values):
    result = {value["field"]: known(value["value"]) for value in values}
    assert len(result) == len(values), "Expected one object, not ambiguous projection values"
    return result


async def windows(session):
    result = await session.observe("root", depth=1, kind="window")
    return {item["label"]: item["scope"] for item in result["items"]}


async def readable(session):
    records = await session.inspect("objects", ["record_type", "id"])
    grouped = {}
    for record in records:
        grouped.setdefault(record["object_ref"], {})[record["field"]] = known(record["value"])
    matches = [owner for owner, record in grouped.items()
               if record.get("record_type") == "readable_document" and record.get("id") == "letter"]
    assert len(matches) == 1, grouped
    return matches[0]


async def menu(session, scopes, name):
    item = await session.find(scopes["Menu bar"], key=name, kind="menu")
    await session.execute([{"op": "open", "target": target(item)}])
    # A first opening reveals an opaque child scope. Observe that new structure
    # once, then send all known subsequent transitions in complete batches.
    items = (await session.observe(scopes["Menu bar"], depth=1, kind="menu_item"))["items"]
    options = {option["key"]: target(option) for option in items}
    await session.execute([{"op": "close", "target": target(item)}])
    return target(item), options


async def modal(session, label):
    item = await session.find("root", depth=1, kind="popup", label=label)
    items = (await session.observe(item["scope"]))["items"]
    return {item["key"]: target(item) for item in items if item["kind"] != "popup"}


async def scenario(session):
    scopes = await windows(session)
    owner = await readable(session)
    row = await session.find(scopes["Objects"], key="select", kind="selectable", owner=owner)
    await session.execute([{"op": "select", "target": target(row)},
        ui_assert(target(row), "state.selected", True),
        app_assert("selection", "object_ref", owner)])
    title = await session.find(scopes["Properties"], key="document-title", owner=owner)
    assert title["kind"] == "text" and title["commit"]["policy"] == "deactivate", title
    assert "tab" in title["commit"]["methods"] and title["applied_binding"]["field"] == "title", title
    before = fields(await session.inspect("object", ["title"], object_ref=owner))["title"]
    changed = "Semantic draft — новая записка"
    title_target = target(title)
    edit_menu, edit_options = await menu(session, scopes, "edit")
    file_menu, file_options = await menu(session, scopes, "file")

    # Seven steps run inside one editor request. No model/client round trip
    # occurs between edit and its independent UI/document/history assertions.
    await session.execute([
        {"op": "edit", "target": title_target, "text": changed, "commit": "none"},
        ui_assert(title_target, "input.text", changed),
        ui_assert(title_target, "value.draft", changed),
        ui_assert(title_target, "state.active", True),
        app_assert("object", "title", before, object_ref=owner),
        app_assert("document", "dirty", False),
        app_assert("history", "can_undo", False)])
    # Passive observation across requests must preserve the same active draft.
    observed = await session.find(scopes["Properties"], key="document-title", owner=owner)
    assert observed["input"]["availability"] == "known" and observed["input"]["text"] == changed
    assert observed["state"]["active"] is True
    assert fields(await session.inspect("object", ["title"], object_ref=owner))["title"] == before
    await session.execute([
        {"op": "commit", "target": title_target, "method": "tab"},
        app_assert("object", "title", changed, object_ref=owner),
        app_assert("document", "dirty", True),
        app_assert("history", "can_undo", True),
        {"op": "open", "target": edit_menu}, {"op": "activate", "target": edit_options["undo"]},
        app_assert("object", "title", before, object_ref=owner),
        app_assert("history", "can_redo", True),
        {"op": "open", "target": edit_menu}, {"op": "activate", "target": edit_options["redo"]},
        app_assert("object", "title", changed, object_ref=owner)])

    # Another family in a different panel, preserving its Enter-only contract.
    ambient = await session.find(scopes["Document Summary"], key="ambient-0-to-0-20", kind="number")
    await session.execute([
        {"op": "edit", "target": target(ambient), "value": .073, "commit": "enter"},
        app_assert("document", "ambient_intensity", .073, predicate="approx", tolerance={"absolute": 1e-6}),
        app_assert("object", "title", changed, object_ref=owner),
        {"op": "open", "target": file_menu}, {"op": "activate", "target": file_options["save-as"]}])
    save = await modal(session, "Save Level As")
    output = session.slot("output-1")
    await session.execute([
        {"op": "edit", "target": save["path"], "text": output, "commit": "tab"},
        {"op": "activate", "target": save["save"]},
        app_assert("document", "slot", "output-1"), app_assert("document", "dirty", False),
        {"op": "open", "target": file_menu}, {"op": "activate", "target": file_options["open"]}])
    saved = fields(await session.inspect("document", ["generation"]))
    opened = await modal(session, "Open Level")
    await session.execute([
        {"op": "edit", "target": opened["path"], "text": output, "commit": "tab"},
        {"op": "activate", "target": opened["open"]},
        app_assert("document", "dirty", False),
        app_assert("document", "generation", saved["generation"], predicate="not_equals"),
        app_assert("document", "ambient_intensity", .073, predicate="approx", tolerance={"absolute": 1e-6})])

    # Open replaces the document generation: discard every old scope/ref/owner.
    scopes = await windows(session)
    owner = await readable(session)
    assert fields(await session.inspect("object", ["title"], object_ref=owner))["title"] == changed
    row = await session.find(scopes["Objects"], key="select", kind="selectable", owner=owner)
    await session.execute([{"op": "select", "target": target(row)}, app_assert("selection", "object_ref", owner)])
    title = await session.find(scopes["Properties"], key="document-title", owner=owner)
    title_target = selector(title["scope"], title["key"], owner)
    file_menu, file_options = await menu(session, scopes, "file")
    saved_digest = hashlib.sha256(Path(output).read_bytes()).hexdigest()
    partial = "Changed before a deliberate assertion failure"
    failed = await session.execute([
        {"op": "edit", "target": title_target, "text": partial, "commit": "tab"},
        app_assert("document", "dirty", False),
        {"op": "open", "target": file_menu}, {"op": "activate", "target": file_options["save"]}],
        expected_error="assertion_failed")
    assert [step["status"] for step in failed["steps"]] == ["passed", "failed", "not_run", "not_run"], failed
    assert failed["effects"] == "partial" and failed["cleanup"] == "released", failed
    assert known(failed["error"]["expected"]) is False and known(failed["error"]["observed"]) is True, failed
    assert hashlib.sha256(Path(output).read_bytes()).hexdigest() == saved_digest, "Save suffix executed"

    # Recovery is a fresh, explicit observation and request; never a replay.
    recovered = await session.find(scopes["Properties"], key="document-title", owner=owner)
    assert recovered["value"]["availability"] == "known" and recovered["value"]["draft"] == partial
    assert fields(await session.inspect("object", ["title"], object_ref=owner))["title"] == partial
    await session.execute([
        app_assert("object", "title", partial, object_ref=owner),
        {"op": "edit", "target": target(recovered), "text": changed, "commit": "tab"},
        app_assert("object", "title", changed, object_ref=owner),
        {"op": "open", "target": file_menu}, {"op": "activate", "target": file_options["save"]},
        app_assert("document", "dirty", False)])
    session.report.update(positive="passed", negative="passed", recovery="passed",
        families=["InputText/Properties", "Selectable/Objects", "InputFloat/Document Summary", "MenuItem", "modal Path"],
        expected_failure={"failed_step": failed["failed_index"], "expected": failed["error"]["expected"],
                          "observed": failed["error"]["observed"], "effects": failed["effects"], "cleanup": failed["cleanup"]})


async def run(args):
    session = EditorUiSession(args.environment, args.transcript, disable_implicit_layers=args.disable_implicit_layers)
    fixture = ROOT / "resources/levels/household-interactions.level.json"
    original = hashlib.sha256(fixture.read_bytes()).digest()
    try:
        async with session:
            try:
                await scenario(session)
                await session.close()
                log = Path(session.report["evidence"]["editor_log"]).read_text(encoding="utf-8", errors="replace")
                assert "Vulkan validation: enabled" in log, "Validation was not enabled"
                assert "Automation Vulkan validation errors after teardown: 0" in log, log[-2048:]
                session.report["vulkan_teardown_errors"] = 0
            finally:
                preserved = hashlib.sha256(fixture.read_bytes()).digest() == original
                session.report["source_fixture_preserved"] = preserved
                assert preserved, "Source fixture was modified"
    except Exception as error:
        print(json.dumps({"status": session.report["status"], "error": str(error)[:512],
                          "evidence": session.report["evidence"]}, ensure_ascii=False))
        return 1
    print(json.dumps({key: session.report[key] for key in
                     ("status", "level", "agent_mcp", "visual", "mcp_requests", "tool_calls", "duration_ms",
                      "response_json_bytes", "cleanup", "evidence")}, ensure_ascii=False))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True, help="Explicitly authorized dedicated test environment for this run")
    parser.add_argument("--transcript", required=True, type=Path, help="New evidence path; never overwritten")
    parser.add_argument("--disable-implicit-layers", action="store_true", help="Disable external implicit Vulkan overlays only in this test child")
    return asyncio.run(run(parser.parse_args()))


if __name__ == "__main__":
    raise SystemExit(main())
