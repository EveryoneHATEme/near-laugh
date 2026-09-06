## 1. Authored switch and level format

- [x] 1.1 Add the optional concrete switch value, immutable runtime handoff, fixed plate bounds, and shared field validation; verify absent/present switches, rotated finite bounds, invalid light slots, and terrain-footprint failures with world tests.
- [x] 1.2 Implement strict version-3 parsing and deterministic saving with a required nullable switch field; verify null/present round trips, exact boolean/index types, missing and unknown fields, and byte-identical repeated saves with codec tests.
- [x] 1.3 Preserve strict version-2 loading through normalization to a current document without a switch; verify authored data preservation, no source-file rewrite on load, explicit version-3 save, and continued rejection of version 1 and mixed version-2/version-3 shapes.

## 2. Interaction input and runtime state

- [x] 2.1 Add physical E handling and the separate player interaction action; verify platform-to-action mapping and unchanged mouse/flashlight controls with input tests.
- [x] 2.2 Add a bounded read-only static visibility query behind the physics boundary; verify unobstructed segments, terrain/solid/rotated-prop obstruction, blockers beyond the endpoint, origins inside blockers, and player exclusion against actual physics fixtures.
- [x] 2.3 Implement concrete switch state, input arming, and view-ray eligibility; verify inclusive 2-metre reach, miss/inside-plate cases, initial on/off for either light slot, held-input rejection, inactive-press consumption, and independence of authored data and the other light with deterministic tests.
- [x] 2.4 Integrate one interaction evaluation per eligible runtime iteration using the displayed player view; verify zero and multiple fixed-step batches, cursor release/recapture, minimized waited input, and skipped/recovered rendering cannot duplicate or defer a press.

## 3. Switch geometry and lighting presentation

- [x] 3.1 Generate the fixed switch plate/rocker using existing opaque textures and tints in the shared world-mesh build path; verify yawed placement, finite outward normals, matching interaction bounds, and inclusion in both full and terrain-only editor rebuilds with geometry tests.
- [x] 3.2 Add backend-neutral enabled values for the two point lights and renderer-private packing within the existing 128-byte push-constant budget; verify default-enabled behavior, all point-light/flashlight combinations, and the host/shader ABI without changing the standalone spotlight contract.
- [x] 3.3 Update pipeline submission and both shader declarations to apply each point-light enable independently; regenerate the paired packaged SPIR-V, build game and editor render targets, and verify the shader assets load successfully while lighting descriptors and meshes remain immutable during toggles.
- [x] 3.4 Connect runtime light state to frame requests and editor initial state to preview requests; verify the linked slot changes independently, changing/removing the editor link restores the previous slot, and no invalid switch field can index lighting data unsafely.

## 4. Editor switch authoring

- [x] 4.1 Extend the concrete editor object value and commands with a collision-free transient switch ID, singleton add/remove, property replacement, and undo/redo; verify selection restoration, add limits, unavailable duplication, history branching, and return-to-saved dirty state with command tests.
- [x] 4.2 Add switch list/viewport selection, selected bounds, and terrain placement preserving its prior terrain-relative height; verify rotated picking, nearest-object selection, missed placement, and preservation of yaw/link/initial state with deterministic editor tests.
- [x] 4.3 Add switch creation and property controls for position, yaw, linked Point light 1/2, and Initially on; verify real ImGui actions, numeric commit/rejection, one history entry per committed edit, and suppression of conflicting navigation/sculpt input.
- [x] 4.4 Preserve transactional preview, shared validation, and authored light state across switch edits and terrain strokes; verify invalid-but-editable placements, blocked saves, correction/undo, and switch geometry retained after terrain rebuilds using editor tests.
- [x] 4.5 Integrate version-2 authoring compatibility and version-3 save visibility; verify opening an old level starts clean without rewriting it, adding a switch and saving/reopening preserves its fields, and the editor identifies the new save format.

## 5. Packaged example and documentation

- [x] 5.1 Update the packaged level to version 3 with a switch on the spawn approach controlling light slot 0, initially on; update affected fixtures and verify shared validation plus existing movement, terrain, chair, and resource-layout behavior.
- [x] 5.2 Update README and affected gameplay, architecture, rendering, and development documentation for E interaction, run-local state, the non-blocking plate, editor controls, and level-format compatibility; verify documented controls and paths against the implemented workflows and remove stale related immutable-light/file-loading descriptions.

## 6. Integration validation and review

- [x] 6.1 Follow `docs/DEVELOPMENT.md` to configure with `cmake --preset debug` and build with `cmake --build --preset debug`; verify the game, editor, tests, and packaged resources build successfully with the required Clang toolchain.
- [x] 6.2 Run `ctest --preset debug --output-on-failure`; verify affected deterministic, boundary, and process tests pass, including switch behavior and version-2/version-3 authoring round trips.
- [x] 6.3 Extend and run `ctest --preset vulkan-smoke --output-on-failure` for game and editor; verify changing light-enable combinations with the flashlight on/off, resize, minimize/restore, swapchain recovery, retained scene resources, and orderly teardown produce no error-severity Vulkan validation messages.
- [x] 6.4 Manually play the packaged example and verify recognizable placement, valid off/on presses, held-key behavior, out-of-range/missed/blocked rejection, flashlight independence, cursor recapture, and reset to authored state after restart without a level-file change; record any check the environment cannot perform.
- [x] 6.5 Manually exercise editor switch creation/removal, placement/yaw, both light links, initial state, undo/redo, sculpting, and save/reopen, including a version-2 input; verify preview matches the authored initial state and document any unavailable desktop check.
- [x] 6.6 Review `git diff`, verify proposal/design/spec/task consistency, and run `openspec validate add-light-switch --strict`; report validation limits and leave archiving until implementation and its required checks are finished.

Manual validation: the user confirmed on 2026-09-06 that all manual gameplay and editor checks in tasks 6.4 and 6.5 were completed and passed.
