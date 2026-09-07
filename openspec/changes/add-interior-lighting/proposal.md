## Why

Two unshadowed point lights and a fixed prototype ambient floor prevent
authoring controllable room-by-room lighting. Establish and validate the
supported lighting profile in a furnished control interior before story work.

## What Changes

- Author the finite set of local lights needed by the control interior,
  their initial state, and ambient contribution. Choose practical limits from
  that scene rather than retaining the prototype's two fixed slots.
- Support multiple authored switches linked by stable light identity, with
  validated references and coherent runtime enable/state changes.
- Add light blocking and shadows for the selected key sources, opaque static
  geometry, and moving doors. Define supported cases and limitations during
  design. P07 integrates animated-character occlusion when characters exist.
- Evaluate the implementation against representative apartment materials and
  geometry. Select a concrete lighting/shadow method after that evaluation.
  HDR, PBR, volumetrics, or a render graph need a demonstrated scene or
  technical requirement.
- Add light/switch creation, editing, deletion, selection, initial-state
  preview, diagnostics, and undo/redo. Keep preview valid through unrelated
  edits and presentation recovery.
- Own explicit run-local light state with coherent switch changes and restart
  behavior, separate from the optional player light. P05 later connects event
  actions and P09 adds checkpoint reconstruction using this concrete state.
- **BREAKING:** Replace fixed light-slot/single-switch content and presentation
  assumptions with the supported authored set; specify format migration.
  No unrelated post-processing or speculative rendering architecture.

## Capabilities

### New Capabilities

- `interior-lighting`: Authored room lighting, supported occlusion/shadows,
  scene-based visual acceptance, and light-budget constraints.

### Modified Capabilities

- `scene-lighting`: Replace two-light/fixed-ambient requirements with the
  selected interior lighting and light-blocking behavior.
- `light-switch`: Support multiple switches and stable light references while
  preserving deterministic interaction and independent runtime state.
- `authored-interaction`: Select among multiple switch plates while preserving
  nearest-target obstruction, deterministic ordering, and input-edge behavior.
- `level-persistence`: Persist the supported light/switch definitions and links.
- `level-object-placement`: Create, duplicate, remove, and edit lights/switches.
- `level-editor`: Diagnose linked-light edits and preview authored light state.
- `runtime-composition`: Supply the supported light set and preserve its
  run-local state through presentation outcomes.
- `vulkan-renderer`: Own lighting/shadow resources and present changing lights
  and required occluders with explicit frame/resource lifetime.

## Impact

Affects level data, switch links, lighting/shader resources, frame data,
editor preview, and run-local light ownership. Document the supported lighting
profile and its limitations. Profile the furnished control scene on explicitly
identified target hardware/resolution before substantial optimization.

## Dependencies and Boundaries

P10 is the next technical change; requires
[P02](../archive/2026-09-06-add-authored-scene-assets/proposal.md) and
[P03](../archive/2026-09-06-add-interactive-doors/proposal.md).
Base format planning starts from current v7. T1 acceptance covers the control
interior, lights, switches, static occluders, and moving doors without P05 or P09.
P07 owns animated-character/shadow integration, P05 owns event lighting control,
and P09 owns save reconstruction. Those later checks do not keep P10 open after
its own supported scope is accepted. No plot, actor scene, or final art is needed.

## Acceptance Criteria

- Author distinct room/corridor light states and operate several linked
  switches; deleting a referenced light produces a repairable diagnostic.
- Key light does not visibly cross a supported opaque wall/closed door; opening
  or obstructing the door changes the expected lit/shadowed region using its
  actual accepted pose.
- Dark areas retain the intended navigational readability without relying on
  a mandatory flashlight or unintended illumination from another room.
- Switch changes survive presentation recovery; a fresh run restores authored
  initial values. Editor preview and runtime agree on those initial values.
- Regenerate/validate changed shaders, run light/reference/state tests
  and Vulkan smoke, and record manual visual assessment plus measured frame
  times and unresolved limitations.
