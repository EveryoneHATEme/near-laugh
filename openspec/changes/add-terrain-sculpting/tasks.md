## 1. Prerequisites and Brush Model

- [ ] 1.1 Confirm the persistence, editor-foundation, and object-placement changes are implemented and archived with their main specs present, and verify object history, terrain picking, validation-gated saving, and editor overlays work before adding terrain commands.
- [ ] 1.2 Add editor-only raise, lower, and smooth brush settings with specified radius, strength, and falloff bounds, and verify table-driven tests accept boundary values and reject non-finite or out-of-range commits without changing prior settings.

## 2. Deterministic Sculpting Kernels

- [ ] 2.1 Implement row-major bounded sample iteration and the fixed falloff weight function, and verify tests cover center, interior, boundary, outside-radius, and terrain-edge samples.
- [ ] 2.2 Implement pure raise and lower stamps that change only finite in-radius samples, and verify identical inputs produce exact identical arrays while zero-effect and outside samples remain unchanged.
- [ ] 2.3 Implement snapshot-based 3-by-3 smoothing with clamped border neighborhoods, and verify tests prove order independence, zero-strength no-op behavior, border handling, and unchanged terrain layout.
- [ ] 2.4 Implement fixed-distance world-path resampling with carried remainder and press-time settings, and verify different input chunkings of the same sampled path produce the same ordered stamps and final terrain.

## 3. Stroke History and Validation

- [ ] 3.1 Implement continuous stroke state that records first-before and final-after values by sorted sample index, and verify multi-stamp overlap stores one compact command while no-op or missed strokes create no revision.
- [ ] 3.2 Add terrain-stroke commands to the existing 128-entry history, and verify one undo/redo restores all affected samples, preview state, validation, and dirty/saved revision behavior.
- [ ] 3.3 Extend shared terrain diagnostics with sample/cell coordinates and triangle identity for slope failures, and verify completed strokes, undo, and redo refresh invalid-slope and spawn support/clearance results while invalid documents remain editable and unsavable.

## 4. Viewport Tools and Preview

- [ ] 4.1 Add terrain brush targeting with nearest heightfield hit, UI-capture suppression, and radius/falloff footprint overlay, and verify pointer hits, misses, terrain edges, camera movement, and UI interaction behave as specified.
- [ ] 4.2 Integrate press/drag/release strokes with deterministic path stamping and brush-mode controls, and verify a continuous gesture creates exactly one history operation regardless of rendered frame count.
- [ ] 4.3 Regenerate the full editor terrain vertex stream at most once per frame and replace its GPU buffer after relevant fence completion, and verify multiple same-frame stamps coalesce while the next available frame shows correct geometry and normals.
- [ ] 4.4 Highlight invalid terrain cells through the concrete editor overlay and validation panel, and verify slope failures identify the matching cell/triangle and clear after repair or undo.

## 5. Integration and Validation

- [ ] 5.1 Add deterministic save/reload tests for sculpted height samples and mixed object/terrain history, and verify canonical level bytes stabilize after an unedited round trip.
- [ ] 5.2 Update editor documentation with brush controls, bounds, history semantics, invalid-terrain repair, and heightfield-only limitations, and verify documented behavior matches the UI.
- [ ] 5.3 Build debug, run deterministic tests, manually exercise long/border/smooth strokes and unsaved-close behavior, and confirm saved terrain produces matching game rendering and Jolt collision after restart.
- [ ] 5.4 Run editor and game Vulkan validation across active strokes, buffer replacement, resize, minimize/restore, and shutdown; run `openspec validate add-terrain-sculpting --strict` and review `git diff` for runtime deformation, voxel, material, or generic dynamic-mesh scope.
