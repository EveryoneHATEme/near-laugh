## 1. Prerequisites and Document Identity

- [x] 1.1 Confirm `remove-legacy-fps-assumptions` is implemented and archived after `add-bounded-level-persistence` and `add-level-editor-foundation`, then verify the read-only editor opens, saves, and renders the version-2 packaged level with only floor, boundary, and obstacle surfaces before enabling mutation.
- [x] 1.2 Add editor-only object IDs for solids and fixed IDs for spawn, two lights, and prop placement, and verify deterministic tests preserve solid identity and selection across insertion, removal, and document replacement without changing serialized output.

## 2. Command Editing and History

- [x] 2.1 Implement UI-independent commands for supported property commits with field-level finite/range checks and post-commit shared validation, and verify tests cover every solid, spawn, light, prop, and proxy property plus rejected commits.
- [x] 2.2 Implement solid add, deterministic duplicate, and remove commands with the 240-solid bound and fixed-object deletion protection, and verify commands restore exact values, order, ID, and selection in focused tests.
- [x] 2.3 Implement the 128-entry undo/redo history and unique document revision model, and verify tests cover saved-state undo, redo, branching after undo, oldest-entry eviction, selection restoration, and history clearing on open.
- [x] 2.4 Integrate validation-gated saving and preview-dirty signaling with committed commands, and verify invalid intermediate documents remain editable but cannot be saved until corrected or undone.

## 3. Picking and Direct Placement

- [x] 3.1 Implement camera-pointer world rays plus nearest analytic intersection for solid AABBs, the yawed prop proxy, and light/spawn marker spheres, and verify known-ray tests cover overlap ordering, misses, inside origins, and edge hits.
- [x] 3.2 Implement exact CPU intersection with the existing heightfield triangulation and deterministic per-object terrain anchors, and verify placement tests cover solids resting on terrain, spawn feet, preserved prop values, light offset, and miss no-ops.
- [x] 3.3 Connect list and viewport selection to the same active ID with empty-space clearing and UI-capture suppression, and verify interaction tests cover consistent selection and camera input isolation.

## 4. Editor Presentation

- [x] 4.1 Add the flat object list and concrete property panels with commit-on-edit completion, and verify continuous numeric drags create one history entry rather than one per rendered frame.
- [x] 4.2 Add object creation, duplication, deletion, undo, redo, and terrain-placement actions with correct disabled states and diagnostics, and verify the UI exposes no hierarchy, arbitrary asset, variable-light, or extra-prop controls.
- [x] 4.3 Add the concrete editor overlay path for selected bounds, spawn/light markers, and placement feedback, and verify overlays match authored proxies, survive resize, and do not enter game render requests.

## 5. Integration and Validation

- [x] 5.1 Add save/reload integration tests for edited solids, spawn, lights, and prop values, and verify canonical document ordering plus exact semantic persistence after undo/redo workflows.
- [x] 5.2 Update editor documentation with selection, placement, property, history, invalid-document, and fixed-object behavior, and verify documented controls match the implementation.
- [x] 5.3 Build debug, run deterministic tests, manually exercise object editing and unsaved-close flows, run editor and `near_laugh` Vulkan validation, run `openspec validate add-level-object-placement --strict`, and review `git diff` for unrelated asset, hierarchy, gizmo, compatibility, or runtime-mutation architecture.

## Validation Record

- Debug configure and build succeeded. The debug CTest preset passed all 164 tests, including command/history, geometric picking, semantic persistence, and real ImGui interaction tests.
- All five Vulkan smoke tests passed, including editor mutation/resource replacement, invalid spawn preview, resize/recovery, and construction failure handling. Strict validation of all 18 OpenSpec items passed.
- Manual desktop inspection used a temporary level copy: list and viewport selection agreed; empty-space selection cleared; a chair yaw drag changed -25 to 5 degrees; terrain placement preserved that yaw and the proxy; Cancel preserved edits after a close request; Save closed successfully and the temporary JSON retained the edited transform. The packaged source level was unchanged.
- The manual run enabled Vulkan validation and exited successfully with no validation errors. Two warnings identified the third-party OW overlay/OBS hook layers advertising Vulkan 1.2 under the Vulkan 1.3 application.
