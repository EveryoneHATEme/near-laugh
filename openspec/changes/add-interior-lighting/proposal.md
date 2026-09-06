## Why

Two unshadowed point lights and a fixed prototype ambient floor cannot
establish the room-by-room atmosphere needed by the furnished apartment.
The closed room, corridor, and visible visitor need controllable local light
and the light blocking required for those scenes to read correctly.

## What Changes

- Author the finite set of local lights needed by the reference apartment,
  their initial state, and ambient contribution. Choose practical limits from
  that scene rather than retaining the prototype's two fixed slots.
- Support multiple authored switches linked by stable light identity, with
  validated references and coherent runtime enable/state changes.
- Add the light blocking and shadows needed for the selected key sources and
  occluders, including moving doors and characters where visible scene cues
  require them. Define the supported cases and limitations during design.
- Evaluate the implementation against representative apartment materials and
  geometry. Select a concrete lighting/shadow method after that evaluation.
  HDR, PBR, volumetrics, or a render graph need a demonstrated scene or
  technical requirement.
- Add light/switch creation, editing, deletion, selection, initial-state
  preview, diagnostics, and undo/redo. Keep preview valid through unrelated
  edits and presentation recovery.
- Extend story lighting control and checkpoint reconstruction for the actual
  authored light states. Preserve separation from the optional player light.
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
- `level-persistence`: Persist the supported light/switch definitions and links.
- `level-object-placement`: Create, duplicate, remove, and edit lights/switches.
- `level-editor`: Diagnose linked-light edits and preview authored light state.
- `runtime-composition`: Supply the supported light set and preserve its
  narrative/checkpoint state through presentation outcomes.
- `vulkan-renderer`: Own lighting/shadow resources and present changing lights
  and required occluders with explicit frame/resource lifetime.

## Impact

Affects level data, switch links, lighting/shader resources, frame data,
editor preview, and state reconstruction. Document the supported lighting
profile and its limitations. Profile the furnished control scene on explicitly
identified target hardware/resolution before substantial optimization.

## Dependencies and Boundaries

P10; requires [P02](../add-authored-scene-assets/proposal.md) and
[P05](../add-narrative-state-and-sequences/proposal.md).
Investigation may start after P01. Final M6 acceptance uses P07 actors and P09
checkpoints when integrated; those are integration checks, not prerequisites
for beginning the light implementation.

## Acceptance Criteria

- Author distinct room/corridor light states and operate several linked
  switches; deleting a referenced light produces a repairable diagnostic.
- Key light does not visibly cross a required opaque wall/closed door; opening
  the door and moving a relevant actor changes the expected lit/shadowed region.
- Dark areas retain the intended navigational readability without relying on
  a mandatory flashlight or unintended illumination from another room.
- Narrative changes and checkpoint resume preserve the intended lighting.
  Editor preview and runtime agree on authored initial state.
- Regenerate/validate changed shaders, run light/reference/reconstruction tests
  and Vulkan smoke, and record manual visual assessment plus measured frame
  times and unresolved limitations.
