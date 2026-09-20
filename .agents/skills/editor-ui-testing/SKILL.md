---
name: editor-ui-testing
description: Prepare and run functional level-editor checks through semantic editor-ui MCP or its official SDK client, including new runtime scenarios without an existing regression. Keep visual acceptance separate.
---

Use the real editor's four generic tools. Read the optional semantic automation
section in `docs/DEVELOPMENT.md` for setup/commands; inspect the specific UI's
metadata and assertions when preparing a new scenario. No feature-specific MCP
handler, document JSON editing, or Windows input is needed for supported widgets.

The main agent prepares the fixture, expected transitions and incremental build
of affected targets **outside MCP**. Configure for initial setup/changed options,
not every action. A matching old executable/manifest is not source freshness.
Delegate the whole bounded scenario to `ui_test_runner`, distinguishing existing
regressions, a new semantic scenario and separate visual checks. Respect the
assigned environment authorization and one GUI owner; the profile name grants
no permission. The runner cannot change application code, assertions or baselines.

## Discover, batch, inspect, close

1. Run the no-window `scripts/editor_ui_doctor.py`. If the four tools are absent
   from the runner, report agent connection **blocked**. An authorized terminal
   SDK run is still possible, but is different evidence. Never use Windows input
   because a tool is absent, a batch fails or an operation is unsupported.
2. `ui_session(start)` chooses an ID in `scripts/editor_ui_fixtures.json`. Keep
   its session ID and returned output slots only for this run. Use increasing
   decimal execution request IDs; never replay IDs/refs from an old transcript.
3. Discover scopes once with `ui_observe(scope="root", depth=1,
   filter={"kind":"window"})`. Use the returned opaque scope IDs for shallow
   filtered observations. Resolve objects with `app_inspect(projection="objects",
   fields=["record_type","id"])` and exact owner filters. Follow `next_cursor`
   on the same snapshot. Closed/conditional content requires explicit opening;
   it is not an absent feature. Do not search root at maximum depth for each field.
4. Read target capabilities, `commit.policy/methods` and `applied_binding`.
   Send all **known** actions and checks together in `ui_execute` (at most 64
   steps). Additional observation is useful for newly revealed structure,
   error recovery or an independent state check, not after every action.
5. Check UI draft/input separately from applied `app_inspect` fields or app
   assertions. Select fields and respect availability/revision stamps. Do not
   compare unavailable or truncated full values as if they were known.
6. Always `ui_session(close)` in cleanup. After Open/document replacement,
   rediscover scopes, object owners and refs; none of the old UI handles survive
   the current implementation's generation reset. No cross-session caches.

## Small Python example for a newly prepared scenario

Use the existing helper, not a new scenario language. Start scripts from the
repository root with the pinned venv and `scripts` on `sys.path`. The helper
uses the same official SDK/production host, assigns IDs, paginates, records
bounded exchanges and closes on exceptions. It explicitly allows focus/scroll
assistance in batches; the wire API itself defaults to strict behavior.

```python
from editor_ui_session import EditorUiSession, app_assert, target, ui_assert

async with EditorUiSession(environment, evidence_path) as ui:
    # Discover owner and scope from this session before constructing targets.
    field = await ui.find(properties_scope, key="document-title", owner=owner)
    old = (await ui.inspect("object", ["title"], object_ref=owner))[0]["value"]
    assert old["availability"] == "known"
    await ui.execute([
        {"op": "edit", "target": target(field), "text": "New title", "commit": "none"},
        ui_assert(target(field), "input.text", "New title"),
        ui_assert(target(field), "value.draft", "New title"),
        app_assert("object", "title", old["value"], object_ref=owner),
        {"op": "commit", "target": target(field), "method": "tab"},
        app_assert("object", "title", "New title", object_ref=owner),
        app_assert("document", "dirty", True),
    ])
```

`tests/automation/real_editor_workflow.py` is the runnable complete example:
fresh discovery, deferred title edit retained across requests, independent
applied assertions, Undo/Redo, another panel/family, Save As/reopen and recovery.
Adapt its ordinary Python steps for a new feature; do not add a host tool.

Other useful checks, always using current observed targets:

- **Invalid input:** edit a deferred numeric input with raw `text:"-"` and
  `commit:"none"`; assert `input.text` and `input_validation.status:"invalid"`,
  then verify the old applied field. Syntax validity differs from application
  validation. For a rejected object ID, commit the known invalid value, verify
  the old applied ID and diagnostics. Use `real_editor_widgets.py` as reference.
- **Selection:** inspect object IDs, find the Objects `select` item with its
  exact owner, then batch `select`, UI `state.selected` and application
  `selection.object_ref` assertions. Never choose the first duplicate label.
- **Undo/Redo:** explicitly open Edit, discover menu items, then batch opening
  Edit/activating Undo/asserting the old applied value, opening Edit/Redo and
  asserting the new value. Check `history.can_undo/can_redo` as appropriate.
- **Save As/Open:** open File and discover its submenu once, activate Save As,
  observe the new ImGui modal, edit Path to a returned `output-1` slot, click
  Save and assert clean/slot. Open that slot through the real Open dialog;
  rediscover after generation change and assert the persisted value. Never use
  arbitrary paths or write level JSON. OS dialogs remain unsupported.

## Failure and evidence

For `failed`, retain the failing index, expected/observed, verified prefix and
`not_run` suffix. Prior effects remain; `cleanup:released` is not rollback.
After verified cleanup explicitly observe, then submit a new corrected batch.
`unknown` means execution evidence was lost: never equate it with `not_run`,
assume no effects, or automatically retry mutations after timeout/disconnect.
If needed explicitly inspect status/read-only state; a faulted session requires
close/new start. Retained duplicate-result retrieval is not a fresh execution.

Return a compact status, failed step, expected/observed, effects, cleanup, MCP
request count, elapsed time, response sizes and evidence paths. Retain the full
bounded transcript, including expected negative cases. Report evidence levels:
A no-window tests; B official SDK/production host/real editor; C actual runner
visibility and use of all four MCP tools. B does not establish C; restart Codex
after local MCP configuration and repeat C in that session.

New standard UI features are ready when metadata, independent applied-state
assertions and a reproducible regression exist. Viewport picking/placement,
sculpting/navigation, gizmos, docking, OS dialogs, game launch and new screenshot
infrastructure are separate future coverage. Functional tests and zero Vulkan
errors do not prove correct appearance; route authorized visual checks separately.
