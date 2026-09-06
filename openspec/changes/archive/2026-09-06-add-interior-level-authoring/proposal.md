## Why

The game's apartment and rear-stair blockout requires several walkable floor
heights and a quick author-to-play loop. Current spawn
validation and editor placement require terrain, while runtime startup always
selects the packaged prototype level.

## What Changes

- Support a compact interior blockout using authored floors, walls, openings,
  stairs, and named entry points. Terrain is optional for interior levels;
  validate supported, unobstructed spawn positions on the selected floor.
  Provide a new-interior starting document with the retained light/prop defaults.
- Extend placement/picking to the intended structural surface, including an
  upper floor or wall, with explicit placement context. Preserve editable
  invalid documents, shared diagnostics, undo/redo, and deterministic saves.
- Allow the runtime to launch an explicitly selected saved level and entry
  point. Add a separate-process editor playtest action with an explicit save
  decision for dirty documents; failed validation or saving prevents launch.
- Package a temporary apartment/stair acceptance level. Reclassify mandatory
  movement-test structures as prototype-fixture requirements, not constraints
  on every authored level. Keep the existing physics and editor boundaries.
- **BREAKING:** Write version 4 with optional terrain, named entries, and an
  explicit default entry. Keep exact version-2 and version-3 read compatibility;
  normalize their single spawn to the `default` entry in memory. Only explicit
  saves migrate files, and older builds cannot read the new output.
- Do not add model import, moving doors, arbitrary scene hierarchies, runtime
  editing, or world streaming in this change. Existing asset/light singletons
  remain until their owning proposals replace them.

## Capabilities

### New Capabilities

- `interior-level-authoring`: Multi-floor blockout authoring, named entry
  points, and launching a selected saved level for playtesting.

### Modified Capabilities

- `level-persistence`: Interior documents, entry identities, optional terrain,
  supported-floor spawn validation, and format compatibility.
- `prototype-scene`: Separate prototype fixture content from valid game
  interiors and permit the selected authored scene.
- `sculptable-terrain`: Require heightfield constraints only where terrain
  exists; allow an interior spawn supported by authored floors.
- `terrain-authoring`: Make terrain editing unavailable when no terrain exists.
- `level-object-placement`: Support structural-surface placement and entries.
- `level-editor`: Launch a validated saved document with dirty-state protection.
- `physics-simulation`: Construct interior collision without mandatory terrain.
- `player-controller`: Start the one player at the selected entry's pose.
- `runtime-composition`: Resolve the explicitly selected level and entry.
- `vulkan-renderer`: Present validated interior geometry without requiring a
  terrain-bearing prototype scene.

## Impact

Affects world data/codec/validation, resource selection and launcher
configuration, physics startup, generated scene geometry, and editor
placement/document/launch workflows. Update the corresponding architecture,
gameplay, rendering, and development documentation during implementation.

## Dependencies and Boundaries

P01 in [the roadmap](../../../../docs/ROADMAP.md); no prerequisite change.
Introduce durable entry identifiers for actual launch references. Other
proposals introduce identities and data for their own new object types.

## Acceptance Criteria

- Create, save, reopen, and launch the room-to-kitchen-to-rear-stairs route;
  start successfully on both apartment and lower-landing floors.
- A spawn inside a wall or without floor support is diagnosed and cannot
  launch; the document remains editable.
- Surface placement chooses the intended floor and undo restores the prior
  placement. Canceling a dirty playtest request launches nothing.
- The selected saved file runs without replacing the packaged prototype;
  resource resolution remains independent of the working directory.
- Run affected deterministic tests, game/editor Vulkan smoke, and a manual
  traversal and author-save-launch exercise.
