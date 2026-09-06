## Why

Doors control access, create a sense of safety, and open or block escape
routes in this game's authored interiors. The current switch interaction
changes a light bit but cannot move blocking geometry or represent a locked
room.

## What Changes

- Add explicitly placed hinged doors with durable identities, authored hinge
  and opening limits, initial open/closed and lock state, and concrete player
  actions to open, close, lock/unlock where applicable, and knock.
- Own current door state in gameplay. Keep door presentation, collision, and
  interaction obstruction consistent during motion; define what happens when
  a player blocks the swing instead of allowing penetration or silent crushing.
- Share bounded targeting and input-edge handling between actual door and
  switch callers. One press activates at most one eligible target, and a moving
  or closed door can block interaction with an object behind it.
- Provide minimal interaction/knock results for later sound and story callers
  without introducing a general event bus or arbitrary action registry.
- Add door creation, editing, selection, preview, validation, and undo/redo.
  Generated temporary door geometry is sufficient for this milestone.
- **BREAKING:** Extend the then-current level profile and frame/physics
  contracts for authored doors and changing poses. Keep source documents
  immutable while playing.
- No general rigid-body sandbox, destructible doors, lock-picking system,
  character AI, or audio implementation in this change.

## Capabilities

### New Capabilities

- `interactive-doors`: Authored hinged doors, locks, knocking, obstruction
  behavior, and coherent changing collision/presentation.
- `authored-interaction`: Deterministic target selection and one-press
  dispatch for the supported concrete world interactions.

### Modified Capabilities

- `light-switch`: Participate in target arbitration and respect door blockers
  while preserving light independence and held-input suppression.
- `level-persistence`: Persist door identities and initial configuration.
- `level-object-placement`: Author door placement, hinge, bounds, and state.
- `physics-simulation`: Include changing door collision and visibility blocking.
- `runtime-composition`: Own door state and supply coherent presentation data.
- `vulkan-renderer`: Present changing door poses without rebuilding the entire
  static scene or exposing gameplay policy in frame data.

## Impact

Affects gameplay interaction, world data/validation, physics queries and
collision updates, render instances, and editor commands/preview. Retain
immutable static resources and explicit per-frame resource lifetime. Update
controls and door-authoring documentation.

## Dependencies and Boundaries

P03; requires [P01](../archive/2026-09-06-add-interior-level-authoring/proposal.md).
Audio response is supplied by P04 and narrative control by P05. These later
consumers use the same authoritative door state.

## Acceptance Criteria

- Open, close, and lock Lena's room door from supported positions. A locked
  door refuses opening with an observable result; knocking does not open it.
- Collision, visible motion, and targeting agree throughout opening/closing;
  blocking a swing produces a defined result without embedding the player.
- A door blocks an otherwise reachable switch. Holding interaction, missing
  a target, or recapturing the cursor cannot produce delayed activation.
- Save/reopen and undo/redo preserve authored door properties; run-time motion
  never changes the source level.
- Run behavioral collision/input tests and Vulkan smoke with repeated door
  motion and presentation recovery, plus a manual doorway exercise.
