## Why

Two unshadowed point lights and a fixed prototype ambient floor prevent
authoring controllable room-by-room lighting. Establish and validate the
supported lighting profile in a furnished control interior before story work.

## What Changes

- Author zero through eight point lights with durable IDs and per-light initial
  state, plus a level-wide ambient value from 0 through 0.20. The T1 interior
  exercises six lights across rooms, corridor and stairs.
- Support zero through sixteen switches, each referencing one light by ID.
  Several switches can toggle the same light; initial state belongs to the
  light, so links cannot introduce conflicting initial values.
- Support up to four authored shadow-casting point lights, including initially
  disabled ones, using bounded raster shadow maps. Terrain, structural solids,
  rendered OPAQUE/MASK props and accepted moving door geometry occlude them.
  Unshadowed fill lights, global ambient and the existing optional spotlight
  retain their explicit unoccluded behavior. P07 owns character occlusion.
- Validate a neutral furnished control interior with the selected apartment
  materials, recorded image comparisons and measured frame times. Keep the
  existing diffuse material profile; no HDR, PBR, volumetrics or render graph.
- Add light/switch creation, editing, deletion, selection, initial-state
  preview, diagnostics, and undo/redo. Keep preview valid through unrelated
  edits and presentation recovery.
- Own explicit run-local light state with coherent switch changes and restart
  behavior, separate from the optional player light. P05 later connects event
  actions and P09 adds checkpoint reconstruction using this concrete state.
- **BREAKING:** Write level format v8 with light/switch arrays and ID links;
  read exact v2-v7 shapes with deterministic migration and no write on open.
  Replace fixed-slot frame enables with a bounded state matching the loaded set.
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
- `interior-level-authoring`: Make the starter interior's lights editable
  records under the new profile, retaining its useful starter defaults.
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
[P02](../2026-09-06-add-authored-scene-assets/proposal.md) and
[P03](../2026-09-06-add-interactive-doors/proposal.md).
Base format planning starts from current v7. T1 acceptance covers the control
interior, lights, switches, static occluders, and moving doors without P05 or P09.
P07 owns animated-character/shadow integration, P05 owns event lighting control,
and P09 owns save reconstruction. Those later checks do not keep P10 open after
its own supported scope is accepted. No plot, actor scene, or final art is needed.
The historical P01/P04 scenes retain their converted two-light appearance;
T1 uses a separate neutral saved level, not filename-triggered gameplay.

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
- Use the current AMD Radeon(TM) Graphics machine at a 1920x1080 framebuffer
  with a 60 FPS target as the planning baseline. The design defines warm-up,
  timing scopes and percentile gates; this proposal makes no measured
  performance claim.
