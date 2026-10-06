"""Narrative authoring through the official SDK and production editor (level B).

Requires a fresh affected-target build and an explicitly authorized environment.
Uses a disposable neutral T4 fixture and ordinary widgets, then retains the
exact editor-saved output bytes. Viewport input, game launch, visual acceptance
and listening have separate checks.
"""
import argparse
import asyncio
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from editor_ui_session import EditorUiSession, app_assert, known, target, ui_assert
from real_editor_workflow import fields, menu, modal, windows


async def owner_for(ui, kind, identity):
    records = await ui.inspect("objects", ["record_type", "id"])
    grouped = {}
    for record in records:
        grouped.setdefault(record["object_ref"], {})[record["field"]] = known(record["value"])
    matches = [owner for owner, record in grouped.items()
               if record.get("record_type") == kind and record.get("id") == identity]
    assert len(matches) == 1, (kind, identity, grouped)
    return matches[0]


async def select(ui, scopes, owner):
    row = await ui.find(scopes["Objects"], key="select", kind="selectable", owner=owner)
    await ui.execute([{"op": "select", "target": target(row)},
                      app_assert("selection", "object_ref", owner)])


async def control(ui, scopes, owner, key):
    return await ui.find(scopes["Properties"], key=key, owner=owner)


async def combo(ui, scopes, owner, key, label):
    widget = await control(ui, scopes, owner, key)
    await ui.execute([{"op": "open", "target": target(widget)}])
    # Combo choices exist in the newly opened child scope only.
    option = await ui.find(scopes["Properties"], depth=1, kind="selectable", owner=owner, label=label)
    await ui.execute([{"op": "select", "target": target(option)}])


async def applied(ui, owner, *names):
    return fields(await ui.inspect("object", list(names), object_ref=owner))


async def scenario(ui):
    scopes = await windows(ui)
    edit, history = await menu(ui, scopes, "edit")
    file, files = await menu(ui, scopes, "file")
    # Remove the source scene event through its ordinary list/history controls.
    original_event = await owner_for(ui, "narrative_event", "neutral-sequence")
    await select(ui, scopes, original_event)
    delete_original = await ui.find(scopes["Objects"], key="delete", kind="button")
    await ui.execute([{"op": "activate", "target": target(delete_original)},
                      app_assert("document", "event_count", 0)])
    owners = {}
    for kind, identity in (("fact", "semantic-done"), ("region", "semantic-region"), ("event", "semantic-sequence")):
        add = await ui.find(scopes["Objects"], key="add-" + kind, kind="button")
        await ui.execute([{"op": "activate", "target": target(add)}, app_assert("document", "dirty", True)])
        owner = (await ui.inspect("selection", ["object_ref"]))[0]["value"]["value"]
        identifier = await control(ui, scopes, owner, "narrative-id")
        await ui.execute([{"op": "edit", "target": target(identifier), "text": identity, "commit": "tab"},
                          app_assert("object", "id", identity, object_ref=owner)])
        owners[kind] = owner
    await select(ui, scopes, owners["region"])
    center = await control(ui, scopes, owners["region"], "center")
    await ui.execute([{"op": "edit", "target": {"ref": center["components"][0]["ref"]}, "value": -1.5, "commit": "tab"}])
    center = await control(ui, scopes, owners["region"], "center")
    await ui.execute([{"op": "edit", "target": {"ref": center["components"][2]["ref"]}, "value": 1.2, "commit": "tab"}])
    event = owners["event"]
    await select(ui, scopes, event)
    seconds = await control(ui, scopes, event, "steps[0]/seconds")
    before = (await applied(ui, event, "steps"))["steps"]
    revision = fields(await ui.inspect("document", ["revision"]))["revision"]
    await ui.execute([
        {"op": "edit", "target": target(seconds), "text": "-", "commit": "none"},
        ui_assert(target(seconds), "input.text", "-"),
        ui_assert(target(seconds), "input_validation.status", "invalid"),
        app_assert("document", "revision", revision),
        {"op": "key", "target": target(seconds), "chord": "Escape"},
        app_assert("document", "revision", revision)])
    assert (await applied(ui, event, "steps"))["steps"] == before
    seconds = await control(ui, scopes, event, "steps[0]/seconds")
    await ui.execute([{"op": "edit", "target": target(seconds), "value": 1.25, "commit": "tab"}])
    assert (await applied(ui, event, "steps"))["steps"] == [{"kind": "delay", "seconds": 1.25}]
    await combo(ui, scopes, event, "trigger-kind", "Region entry")
    await combo(ui, scopes, event, "region", "semantic-region")
    add_step = await control(ui, scopes, event, "add-step")
    await ui.execute([{"op": "activate", "target": target(add_step)}])
    await combo(ui, scopes, event, "steps[1]/step-kind", "Set fact")
    await combo(ui, scopes, event, "steps[1]/fact", "semantic-done")
    value = await control(ui, scopes, event, "steps[1]/value")
    await ui.execute([{"op": "set_checked", "target": target(value), "value": True}])
    expected = [{"kind": "delay", "seconds": 1.25}, {"kind": "set_fact", "fact": "semantic-done", "value": True}]
    assert (await applied(ui, event, "steps", "trigger")) == {
        "steps": expected, "trigger": {"kind": "region_entry", "region": "semantic-region"}}

    # Add the remaining concrete actions through ordered insertion. This is the
    # second saved neutral variation, authored entirely in ordinary widgets.
    for index, kind, references in (
        (0, "Set light", (("light", "table-light"),)),
        (1, "Play cue", (("source", "sequence-source"),)),
        (3, "Run route", (("actor", "walker"), ("route", "walk"))),
    ):
        insert = await control(ui, scopes, event, f"steps[{index}]/insert-step-before")
        await ui.execute([{"op": "activate", "target": target(insert)}])
        await combo(ui, scopes, event, f"steps[{index}]/step-kind", kind)
        for field, value in references:
            await combo(ui, scopes, event, f"steps[{index}]/{field}", value)
    enabled = await control(ui, scopes, event, "steps[0]/enabled")
    await ui.execute([{"op": "set_checked", "target": target(enabled), "value": True}])
    expected = [
        {"kind": "set_light", "light": "table-light", "enabled": True},
        {"kind": "play_cue", "source": "sequence-source"},
        {"kind": "delay", "seconds": 1.25},
        {"kind": "run_route", "actor": "walker", "route": "walk"},
        {"kind": "set_fact", "fact": "semantic-done", "value": True},
    ]
    assert (await applied(ui, event, "steps"))["steps"] == expected
    reordered = [expected[1], expected[0], *expected[2:]]

    # Cancel conditions use the existing radio identity and explicit desired state.
    radio_records = await ui.inspect("objects", ["record_type", "id"])
    grouped = {}
    for record in radio_records:
        grouped.setdefault(record["object_ref"], {})[record["field"]] = known(record["value"])
    radio = next(v["id"] for v in grouped.values() if v.get("record_type") == "radio")
    cancel = await control(ui, scopes, event, "cancellation-enabled")
    await ui.execute([{"op": "set_checked", "target": target(cancel), "value": True}])
    add_condition = await control(ui, scopes, event, "cancel-add-condition")
    await ui.execute([{"op": "activate", "target": target(add_condition)}])
    await combo(ui, scopes, event, "cancel[0]/condition-kind", "Radio enabled")
    await combo(ui, scopes, event, "cancel[0]/radio", radio)
    assert (await applied(ui, event, "cancel"))["cancel"] == [{"kind": "radio", "radio": radio, "enabled": False}]
    await ui.execute([app_assert("document", "valid", True),
                      {"op": "open", "target": file}, {"op": "activate", "target": files["save-as"]}])
    save = await modal(ui, "Save Level As")
    output = ui.slot("output-1")
    await ui.execute([{"op": "edit", "target": save["path"], "text": output, "commit": "tab"},
                      {"op": "activate", "target": save["save"]},
                      app_assert("document", "dirty", False), app_assert("document", "slot", "output-1")])
    baseline_bytes = Path(output).read_bytes()
    down = await control(ui, scopes, event, "steps[0]/move-step-down")
    await ui.execute([{"op": "activate", "target": target(down)}, app_assert("document", "dirty", True)])
    assert (await applied(ui, event, "steps"))["steps"] == reordered
    await ui.execute([{"op": "open", "target": edit}, {"op": "activate", "target": history["undo"]},
                      app_assert("document", "dirty", False)])
    assert (await applied(ui, event, "steps"))["steps"] == expected
    await ui.execute([{"op": "open", "target": edit}, {"op": "activate", "target": history["redo"]},
                      app_assert("document", "dirty", True)])
    assert (await applied(ui, event, "steps"))["steps"] == reordered
    await ui.execute([{"op": "open", "target": edit}, {"op": "activate", "target": history["undo"]},
                      app_assert("document", "dirty", False)])

    await select(ui, scopes, owners["fact"])
    identifier = await control(ui, scopes, owners["fact"], "narrative-id")
    await ui.execute([{"op": "edit", "target": target(identifier), "text": "semantic-renamed", "commit": "tab"}])
    assert (await applied(ui, event, "steps"))["steps"][4]["fact"] == "semantic-renamed"
    await ui.execute([{"op": "open", "target": edit}, {"op": "activate", "target": history["undo"]},
                      app_assert("document", "dirty", False)])
    delete = await ui.find(scopes["Objects"], key="delete", kind="button")
    await ui.execute([{"op": "activate", "target": target(delete)}, app_assert("document", "valid", False),
                      app_assert("document", "dirty", True)])
    assert (await applied(ui, event, "steps"))["steps"] == expected
    await ui.execute([{"op": "open", "target": edit}, {"op": "activate", "target": history["undo"]},
                      app_assert("document", "valid", True), app_assert("document", "dirty", False)])
    assert Path(output).read_bytes() == baseline_bytes, "Unsaved edits modified the saved baseline"

    await ui.execute([{"op": "open", "target": file}, {"op": "activate", "target": files["open"]}])
    opened = await modal(ui, "Open Level")
    await ui.execute([{"op": "edit", "target": opened["path"], "text": output, "commit": "tab"},
                      {"op": "activate", "target": opened["open"]},
                      app_assert("document", "dirty", False), app_assert("document", "valid", True)])
    scopes = await windows(ui)  # All old references expired on document replacement.
    event = await owner_for(ui, "narrative_event", "semantic-sequence")
    await select(ui, scopes, event)
    assert (await applied(ui, event, "steps"))["steps"] == expected
    assert (await applied(ui, event, "trigger"))["trigger"]["region"] == "semantic-region"
    # The host removes output slots when the session closes. Retain exactly the
    # editor-saved bytes, without manufacturing or normalizing authored data.
    retained = ui.path.with_suffix(".level.json")
    with retained.open("xb") as artifact:
        artifact.write(baseline_bytes)
    assert retained.read_bytes() == Path(output).read_bytes()
    ui.report["evidence"]["saved_variation"] = str(retained)
    ui.report.update(narrative="passed", saved_variation=str(retained),
        saved_variation_sha256=hashlib.sha256(baseline_bytes).hexdigest(),
        coverage=["create/select", "typed references", "invalid duration draft", "Escape cancel", "step reorder", "rename", "delete/undo repair", "undo/redo clean baseline", "save/reopen"],
        unsupported=["viewport picking/placement", "game launch", "appearance", "listening"])


async def run(args):
    source = ROOT / "resources/levels/narrative-t4.level.json"
    original = hashlib.sha256(source.read_bytes()).digest()
    ui = EditorUiSession(args.environment, args.transcript, fixture="narrative-t4",
                         disable_implicit_layers=args.disable_implicit_layers)
    try:
        async with ui:
            try:
                await scenario(ui)
            finally:
                assert hashlib.sha256(source.read_bytes()).digest() == original, "Source fixture changed"
                ui.report["source_fixture_preserved"] = True
    except Exception as error:
        print(json.dumps({"status": ui.report["status"], "error": str(error)[:512], "evidence": ui.report["evidence"]}))
        return 1
    print(json.dumps({key: ui.report[key] for key in ("status", "level", "agent_mcp", "visual", "mcp_requests", "tool_calls", "duration_ms", "response_json_bytes", "cleanup", "evidence")}))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--environment", required=True)
    parser.add_argument("--transcript", required=True, type=Path)
    parser.add_argument("--disable-implicit-layers", action="store_true")
    return asyncio.run(run(parser.parse_args()))


if __name__ == "__main__":
    raise SystemExit(main())
