## 1. Prerequisite and Target Boundaries

- [ ] 1.1 Confirm `add-bounded-level-persistence` is implemented and archived with `level-persistence` present in main specs, and verify the packaged prototype level loads through the shared document API before starting editor work.
- [ ] 1.2 Add an exact pinned Dear ImGui dependency containing core plus GLFW/Vulkan backends and editor-only docking configuration, and verify the `fps`, `near_laugh_runtime`, and public-header dependency checks contain no ImGui usage requirement.
- [ ] 1.3 Add concrete editor UI/render modules and a `level_editor` executable with executable-relative resource copying, and verify the editor target builds without linking `near_laugh_runtime` or constructing gameplay objects.

## 2. Editor Lifetime, Input, and Camera

- [ ] 2.1 Implement the internal GLFW/ImGui editor bridge with explicit callback-chain and RAII ordering, and verify focused lifecycle tests or instrumentation cover successful startup, partial failure, and reverse-order shutdown without changing gameplay callbacks.
- [ ] 2.2 Implement the editor-owned fixed free-fly camera and backend-neutral frame conversion, and verify deterministic tests cover movement axes, sprint, pitch limits, mouse look, aspect changes, and known transformed points.
- [ ] 2.3 Implement editor input ownership using UI capture intent and scene-navigation state, and verify tests show menus/text entry suppress camera input while active scene navigation remains responsive.
- [ ] 2.4 Implement the editor main loop with event polling, minimized waiting, close handling, bounded camera timing, and explicit render outcomes, and verify deterministic loop tests cover normal, close, zero-extent, restore, skipped, and recovered paths.

## 3. Document Lifecycle

- [ ] 3.1 Implement `EditorDocument` with transactional open, optional resolved path, validation state, dirty state, and non-destructive error handling, and verify tests preserve an existing document when candidate loading fails.
- [ ] 3.2 Implement save and save-as through the shared deterministic codec, and verify success updates the path and clears dirty state while validation or filesystem failure preserves the document and previous state.
- [ ] 3.3 Implement the pending open/close/exit state machine with save, discard, and cancel outcomes, and verify table-driven tests cover every dirty and clean transition without unintended replacement or exit.

## 4. Editor Rendering and ImGui Workspace

- [ ] 4.1 Implement the concrete editor Vulkan renderer with scene-first and ImGui-last dynamic rendering, explicit document-resource replacement, and RAII teardown, and verify focused tests cover ownership plus injected construction failures.
- [ ] 4.2 Add the pass-through dock workspace, menus, read-only document summary/properties, validation panel, and path-entry modals, and verify an opened level displays every bounded content category without exposing mutation controls.
- [ ] 4.3 Handle resize, swapchain recovery, zero extent, and document replacement while retaining UI/document state, and verify the editor remains usable across minimize/restore and repeated valid/invalid opens.

## 5. Integration and Validation

- [ ] 5.1 Add boundary tests proving editor/ImGui types do not leak into game targets or public headers and editor camera/UI concepts do not enter renderer frame or level persistence contracts.
- [ ] 5.2 Update architecture and development documentation with editor build/run commands, controls, file workflow, dependency boundaries, and explicit non-goals, and verify the instructions match executable-relative resources.
- [ ] 5.3 Configure and build debug, run deterministic tests, exercise open/save-as/dirty-close workflows manually, and verify the `fps` executable remains behaviorally unchanged.
- [ ] 5.4 Run Vulkan validation for editor startup, level replacement, resize, minimize/restore, and shutdown plus the existing smoke preset; run `openspec validate add-level-editor-foundation --strict` and review `git diff` for unsupported generic editor or renderer architecture.

