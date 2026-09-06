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
- **BREAKING:** Write level format 5 with an explicit bounded door array.
  Read exact versions 2, 3, and 4 as levels without doors; opening never
  rewrites a source document. Extend frame/physics contracts for changing
  poses while keeping authored definitions immutable during play.
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
- `player-input`: Add a concrete lock action alongside interaction and the
  existing secondary mouse action used for knocking.
- `level-persistence`: Persist door identities and initial configuration.
- `level-object-placement`: Author door placement, hinge, bounds, and state.
- `physics-simulation`: Include changing door collision and visibility blocking.
- `runtime-composition`: Own door state and supply coherent presentation data.
- `vulkan-renderer`: Present changing door poses without rebuilding the entire
  static scene or exposing gameplay policy in frame data.
- `interior-level-authoring`: Extend the packaged apartment exercise with the
  room door and an interaction obstruction case while retaining both starts.

## Impact

Affects gameplay interaction, world data/validation, physics queries and
collision updates, render instances, and editor commands/preview. Retain
immutable static resources and explicit per-frame resource lifetime. Update
controls and door-authoring documentation.

## Dependencies and Boundaries

P03; requires [P01](../archive/2026-09-06-add-interior-level-authoring/proposal.md).
Audio response is supplied by P04 and narrative control by P05. These later
consumers use the same authoritative door state.

P03 is integrated before the concurrently planned P02. P03 keeps the current
single-chair and fixed-texture profile and uses generated doors. P02 then
rebases its shared deltas on P03 and writes version 6 while preserving doors,
their identities, and runtime behavior. This is the selected integration order,
not a requirement that door behavior depend on imported assets.

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
