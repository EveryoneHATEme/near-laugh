## Why

P07's route fixture must become an authorable capability. The standalone editor
needs concrete character, mark and route editing so a second neutral scene can
be built and played without modifying runtime code or hand-editing JSON.

## What Changes

- Add actor/mark/route list entries, selection, properties and surface placement
  using existing document commands, durable IDs and the shared undo history.
- Show route links, facing, initial actor bounds and actionable reference and
  support/clearance diagnostics. Keep invalid safe records available for repair.
- Add explicit snapshot clip playback/scrubbing and schematic route preview,
  using P07a sampling and authored marks without constructing game physics.
  Clearly identify route preview as an inspection of motion and links;
  collision/door/audio acceptance uses saved-file Play in the game.
- Stop previews on document edits/replacement, undo/redo, minimize or Play.
  Preview controls never dirty the document or mutate runtime initial values.
- Complete save/preflight/launch and recovery checks; author and play a second
  neutral scene with the supported tools to close T2.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `level-object-placement`: Concrete actor/mark/route editing, reference
  integrity, placement and undo/redo.
- `level-editor`: Explicit animation/route preview, diagnostics and
  end-to-end character authoring acceptance.

## Impact

Affects editor document commands, picking, overlays, ImGui UI and its existing
transactional renderer integration. Reuses P07b's v9 data and validation,
P07a's animated resources, and the saved-file game process workflow. No
additional format, general timeline editor, in-editor player simulation,
retargeting UI or new physics dependency is needed.

## Dependencies and Boundaries

P07c; implement and archive
[P07b](../add-scripted-characters/proposal.md), which requires
[P07a](../archive/2026-09-08-add-character-animation/proposal.md), then rebase on their resulting
main requirements. No P05 events, P09 checkpoints or P11 tooling is needed.

The three P07 changes jointly own T2. This split intentionally places complete
feature authoring in the final P07 stage; it is not deferred to P11.
Later proposals that require P07 require the completed chain.

## Acceptance Criteria

- Add, place, rename, duplicate and remove supported actors/marks/routes;
  edit links/order/initial action and undo/redo coherent changes.
- Inspect each supported clip, time and transition plus the route and scene
  facing without changing saved data or requiring game physics in the editor.
- Broken links remain selectable and repairable; asset failure retains a
  labeled stale preview and blocks Play until a successful fresh preflight.
- Author a second scene from New Interior, Save As, reopen, and Play from a
  working directory outside the repository with literal Unicode paths.
- Verify real player/door obstruction, action sound and animated shadows in
  that game run; stop stale previews on every documented transition.
- Pass affected editor/UI/process tests and Vulkan smoke with teardown
  validation; retain manual workflow evidence and state unavailable checks.
