## Why

The player can explore and use a flashlight but cannot affect an authored object. One light switch creates a small, observable interaction and a concrete reason to separate authored lighting from changing gameplay state.

## What Changes

- Add at most one authored switch controlling one of the existing two point lights. The packaged prototype includes a reachable, recognizable switch with its light initially on.
- Map E to interaction: a new press toggles the linked light when the player looks at the switch within 2 metres and static collision does not obstruct the view. Held or inactive input does not cause delayed or repeated toggles.
- Keep level data, geometry, and authored light parameters immutable during play; maintain the switch state in the runtime and submit independent point-light enable values with each frame.
- Include switch creation/removal, selection, numeric placement/yaw, light selection, initial state, terrain placement, undo/redo, validation, and preview in the existing editor workflow. The level permits zero or one switch; duplication is unavailable.
- Add version-3 level documents with a required nullable switch field. Existing version-2 levels continue to load as levels without a switch. **BREAKING for older executables:** saves use version 3, which older builds cannot read; saving remains an explicit user action.
- Keep this change limited to the switch. Audio, moving doors, shadows, save-game persistence, HUD prompts, animated switch parts, general interaction registries, and scripting are outside its scope.

## Capabilities

### New Capabilities

- `light-switch`: Authored single-switch behavior, bounded unobstructed interaction, visible placement, and run-local state.

### Modified Capabilities

- `player-input`: Add the concrete interaction action and E mapping.
- `level-persistence`: Define the switch-bearing version-3 format, bounded version-2 loading, and the distinction between immutable authored data and runtime light state.
- `level-object-placement`: Extend the flat supported object set and existing editing workflow to the optional singleton switch.
- `scene-lighting`: Apply per-frame enable state to immutable authored point lights while retaining ambient and flashlight behavior.
- `scene-texturing`: Preserve textured shading with runtime point-light enable state.
- `runtime-composition`: Supply backend-neutral point-light enable state without exposing switch policy to the renderer.
- `vulkan-renderer`: Present point-light state changes safely across frames and presentation recovery.

## Impact

- Affects the level codec/validation and packaged level, physical and player input, runtime interaction policy, a narrow physics visibility query, generated switch geometry, frame/shader packing, editor commands/UI/picking/preview, and their tests.
- Reuses the existing fixed textures, two-light array, Jolt static collision, renderer ownership, and editor history. No new third-party dependency or general subsystem framework is required.
- Updates gameplay, architecture, rendering, development, and control documentation to describe the new behavior and level-format transition. Vulkan smoke and manual interaction checks are required during implementation.
