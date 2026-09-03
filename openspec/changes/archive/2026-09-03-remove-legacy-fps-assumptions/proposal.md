## Why

The project now targets a single-player narrative horror game, but current documentation, specifications, executable and input names, and prototype content still encode the obsolete first-person-shooter direction. Leaving those contracts in place would keep influencing new work and preserve shooter-specific test content without a game requirement.

## What Changes

- Rewrite architecture, rendering, and development documentation around the purpose-built horror runtime and clearly separate durable constraints from current prototype details.
- **BREAKING**: Rename the `fps` game executable to `near_laugh`, rename the FPS input API and capability to player-input terminology, and update user-visible application names, tests, and documentation. No compatibility aliases are retained.
- **BREAKING**: Replace the test-only level format version 1 with version 2, remove the `shooting_target` solid kind, surface role, texture asset, and the three required inert target plates from the prototype level and runtime resource contract. Version-1 test levels are intentionally rejected and will be regenerated or edited in place without a migration path.
- Remove remaining normative FPS/shooter wording from current main specs and the two unimplemented active authoring changes while preserving factual first-person camera and player-controller requirements.
- Preserve archived OpenSpec changes as historical records.

## Capabilities

### New Capabilities

- `player-input`: Concrete platform-independent actions for the one local first-person player, replacing the obsolete `fps-input` capability.

### Modified Capabilities

- `fps-input`: Retire the obsolete capability and its FPS-named requirements without compatibility aliases.
- `platform-windowing`: Describe the physical-input boundary without assigning obsolete FPS semantics.
- `player-controller`: Consume player actions rather than FPS-named actions while preserving current prototype movement behavior.
- `runtime-composition`: Coordinate player actions and the renamed game application without FPS terminology.
- `level-persistence`: Accept only the cleaned version-2 test-level format, reject version 1 and the removed shooting-target values, and provide no backward-compatibility path.
- `prototype-scene`: Remove the required three target plates from the packaged prototype environment.
- `scene-texturing`: Reduce the fixed prototype surface set to floor, boundary, and obstacle textures and remove target-specific presentation requirements.
- `level-editor`: Keep the standalone editor isolated from the renamed `near_laugh` game executable and removed prototype resources.

## Impact

- Affects CMake targets and resource-copy rules, the runtime launcher and title, input headers/implementation and consumers, boundary tests, deterministic tests, smoke tests, and developer commands.
- Affects the level version, enums and codec, validation, packaged prototype JSON, texture-array layout, shaders or layer constants where applicable, editor/runtime resource manifests, and removal of `prototype_shooting_target.png`.
- Updates `docs/ARCHITECTURE.md`, `docs/RENDERING.md`, `docs/DEVELOPMENT.md`, current OpenSpec main specs, and the planning artifacts for `add-level-object-placement` and `add-terrain-sculpting`.
- Does not add combat, a generic input framework, a generic material system, a scene hierarchy, or backward-compatibility machinery for prototype assets.
