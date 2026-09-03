## 1. Remove Legacy Level Content

- [x] 1.1 Advance the bounded level codec and repository fixtures to format version 2, remove the shooting-target kind and surface values, and verify codec tests accept canonical version-2 round trips while rejecting version 1 and legacy `shooting_target` tokens without translation.
- [x] 1.2 Remove the exactly-three-target-plates validation and delete the three plate entries from the packaged prototype level, then verify level validation, scene composition, renderer input, and physics collision tests pass for the remaining terrain, structures, lights, spawn, and chair.
- [x] 1.3 Remove `prototype_shooting_target.png` from source and copied resource manifests, reduce the fixed texture array to the stable floor/boundary/obstacle layer order, and verify resource-layout, texture decoding, generated-layer, editor-resource, and Vulkan smoke assertions cover exactly those three textures.

## 2. Rename Player Input and Game Application

- [x] 2.1 Rename the FPS input files and types to the concrete player-input contract (`PlayerInputMapper` and `PlayerActionSnapshot`) without aliases, update all consumers and tests, and verify input-batch, default-control, cursor-capture, flashlight-edge, and player-controller tests retain their behavior.
- [x] 2.2 Rename the `fps` CMake target and produced executable to `near_laugh`, update the visible application title, resource-copy expressions, process probes, boundary checks, smoke invocations, and test dependencies without an alias target, and verify a fresh configure exposes only the new game target and executable name.
- [x] 2.3 Audit non-archived source, build, resource, and test files for stale FPS/shooting-target identifiers, remove each obsolete dependency rather than suppressing it, and verify remaining first-person or shooter terms describe camera perspective, explicit non-goals, rejected legacy input, or historical artifacts only.

## 3. Align Documentation and Current Planning

- [x] 3.1 Rewrite `docs/ARCHITECTURE.md` around the actual `near_laugh_*` runtime/editor modules, concrete ownership and dependency flow, and a fenced ASCII diagram; remove the generic game-over-engine layering and duplicated target-plate/milestone inventory, then verify its stated dependencies match CMake targets and public boundaries.
- [x] 3.2 Update `docs/RENDERING.md` to serve the authored horror experience, remove first-person weapon rendering and unrequired speculative scope, describe the three-surface current renderer accurately, and verify its resource, frame, descriptor, lighting, and lifetime statements match implementation and current specs.
- [x] 3.3 Update `docs/DEVELOPMENT.md` with the `near_laugh` commands and three-texture version-2 fixture, remove the shooter-oriented absence inventory, retain concise current editor/prototype behavior, and verify every documented command and resource path exists after the build.
- [x] 3.4 Update current main-spec Purpose text and non-delta terminology from FPS/engine-generic wording to game/runtime/player wording while retaining explicit first-person behavior and intentional combat non-goals, and verify the replacement `player-input` capability and removal of `fps-input` will leave no duplicate input contract after sync/archive.
- [x] 3.5 Revise every artifact in `add-level-object-placement` and `add-terrain-sculpting` to assume version 2, the three-role surface set, `near_laugh`, and player-oriented terminology; record this cleanup as a prerequisite and verify both changes remain internally coherent and pass strict OpenSpec validation before either is applied.

## 4. Integration and Validation

- [x] 4.1 Configure the standard debug preset from a fresh build tree, build all targets, run the complete deterministic test preset, and verify no test or generated command still expects the removed target, input, level-version, or texture contract.
- [x] 4.2 Run the game and editor smoke paths with Vulkan validation, including resize, minimize/restore, swapchain recovery, and shutdown, and verify both load the version-2 level with three texture layers and report no validation errors.
- [x] 4.3 Run strict validation for `remove-legacy-fps-assumptions`, `add-level-object-placement`, and `add-terrain-sculpting`; review `git diff` to confirm archived changes are untouched, no compatibility machinery or replacement target content was added, and all current documentation agrees with `README.md`, `docs/VISION.md`, and `docs/GAMEPLAY.md`.
