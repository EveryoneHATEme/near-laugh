## Why

The FPS level is currently hard-coded in `PrototypeLevel`, so changing terrain, structures, lights, or the player spawn requires recompiling the engine. A bounded level asset is the necessary persistence boundary for later purpose-built authoring tools while preserving the runtime's immutable, validated world.

## What Changes

- Define one versioned, human-readable FPS level asset containing the existing heightfield, axis-aligned solids, fixed-surface assignments, static prop placement and box proxy, point lights, ambient intensity, and player spawn.
- Add deterministic level loading, serialization, validation, and actionable diagnostics without introducing a generic scene graph or asset system.
- Package and load the prototype level beneath the configured executable-relative resource root.
- Preserve one immutable validated level shared by renderer and physics after startup.
- **BREAKING** Replace the requirement that the prototype scene needs no level file with a required packaged level asset.

## Capabilities

### New Capabilities

- `level-persistence`: Bounded level-file representation, deterministic load/save behavior, validation, diagnostics, and immutable runtime handoff.

### Modified Capabilities

- `prototype-scene`: Source the existing prototype terrain, solids, prop, lights, and spawn from the packaged level asset instead of hard-coded construction.
- `runtime-composition`: Resolve the required level asset from the explicit resource root and fail startup cleanly when it cannot produce a valid level.

## Impact

- Affects `near_laugh_world`, runtime resource resolution and composition, renderer/physics startup, packaged resources, CMake resource copying, and deterministic tests.
- Introduces a narrowly scoped level codec dependency or parser implementation selected in design; it does not add runtime hot reload, scene hierarchy, arbitrary assets, or mutable runtime terrain.
- Establishes the prerequisite data boundary for `add-level-editor-foundation`.
