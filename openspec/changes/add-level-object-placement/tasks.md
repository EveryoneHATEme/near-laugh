## 1. Prerequisites and Document Identity

- [ ] 1.1 Confirm `add-bounded-level-persistence` and `add-level-editor-foundation` are implemented and archived with their main specs present, and verify the read-only editor opens, saves, and renders the packaged level before enabling mutation.
- [ ] 1.2 Add editor-only object IDs for solids and fixed IDs for spawn, two lights, and prop placement, and verify deterministic tests preserve solid identity and selection across insertion, removal, and document replacement without changing serialized output.

## 2. Command Editing and History

- [ ] 2.1 Implement UI-independent commands for supported property commits with field-level finite/range checks and post-commit shared validation, and verify tests cover every solid, spawn, light, prop, and proxy property plus rejected commits.
- [ ] 2.2 Implement solid add, deterministic duplicate, and remove commands with the 240-solid bound and fixed-object deletion protection, and verify commands restore exact values, order, ID, and selection in focused tests.
- [ ] 2.3 Implement the 128-entry undo/redo history and unique document revision model, and verify tests cover saved-state undo, redo, branching after undo, oldest-entry eviction, selection restoration, and history clearing on open.
- [ ] 2.4 Integrate validation-gated saving and preview-dirty signaling with committed commands, and verify invalid intermediate documents remain editable but cannot be saved until corrected or undone.

## 3. Picking and Direct Placement

- [ ] 3.1 Implement camera-pointer world rays plus nearest analytic intersection for solid AABBs, the yawed prop proxy, and light/spawn marker spheres, and verify known-ray tests cover overlap ordering, misses, inside origins, and edge hits.
- [ ] 3.2 Implement exact CPU intersection with the existing heightfield triangulation and deterministic per-object terrain anchors, and verify placement tests cover solids resting on terrain, spawn feet, preserved prop values, light offset, and miss no-ops.
- [ ] 3.3 Connect list and viewport selection to the same active ID with empty-space clearing and UI-capture suppression, and verify interaction tests cover consistent selection and camera input isolation.

## 4. Editor Presentation

- [ ] 4.1 Add the flat object list and concrete property panels with commit-on-edit completion, and verify continuous numeric drags create one history entry rather than one per rendered frame.
- [ ] 4.2 Add object creation, duplication, deletion, undo, redo, and terrain-placement actions with correct disabled states and diagnostics, and verify the UI exposes no hierarchy, arbitrary asset, variable-light, or extra-prop controls.
- [ ] 4.3 Add the concrete editor overlay path for selected bounds, spawn/light markers, and placement feedback, and verify overlays match authored proxies, survive resize, and do not enter game render requests.

## 5. Integration and Validation

- [ ] 5.1 Add save/reload integration tests for edited solids, spawn, lights, and prop values, and verify canonical document ordering plus exact semantic persistence after undo/redo workflows.
- [ ] 5.2 Update editor documentation with selection, placement, property, history, invalid-document, and fixed-object behavior, and verify documented controls match the implementation.
- [ ] 5.3 Build debug, run deterministic tests, manually exercise object editing and unsaved-close flows, run editor and game Vulkan validation, run `openspec validate add-level-object-placement --strict`, and review `git diff` for unrelated asset, hierarchy, gizmo, or runtime-mutation architecture.

