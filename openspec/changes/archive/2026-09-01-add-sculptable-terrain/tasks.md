## 1. Terrain level data

- [x] 1.1 Add the concrete immutable 97 by 97 height-grid terrain description, authored sample data, terrain queries, and validation for dimensions, finite samples, continuous cells, and the supported slope limit; verify with focused `test_world` terrain-validation cases.
- [x] 1.2 Replace the prototype flat floor box with the terrain, extend enclosing boundaries below the terrain minimum, and resolve the player spawn foot elevation from valid terrain data; verify the existing movement-test solids, plate count, and a supported clear spawn remain valid in `test_world`.

## 2. Opaque terrain rendering

- [x] 2.1 Extend prototype-scene vertex generation to append consistently wound terrain triangles with finite upward-facing unit normals, floor texture layer, and world-X/Z tiled UVs into the existing combined vertex buffer; verify terrain cell counts, shared-edge positions/UVs, normals, and vertex layout in `test_prototype_scene` and `test_vertex_layout`.
- [x] 2.2 Preserve the existing one-pipeline, one-vertex-buffer, one-draw renderer lifetime while accepting the enlarged scene geometry; verify source-boundary checks and renderer tests continue to assert the established ownership and draw model.

## 3. Static terrain collision

- [x] 3.1 Create one contained static Jolt heightfield collision shape from the prototype terrain samples alongside existing box bodies, with dependency-safe cleanup and actionable initialization failures; verify headless physics tests ground the character on terrain features and retain collision for every existing solid.
- [x] 3.2 Add deterministic traversal coverage for both terrain-cell triangle directions, a terrain depression near the enclosure, and slope-limit rejection; verify the player cannot pass beneath boundaries or through the matching terrain surface.

## 4. Integration validation

- [x] 4.1 Configure and build the debug preset, then run `ctest --preset debug --output-on-failure`; verify all affected deterministic, boundary, world, rendering, and physics tests pass.
- [x] 4.2 Run `ctest --preset vulkan-smoke --output-on-failure` on a presentation-capable desktop, inspect the sculpted ground, tiled texture scale, and player traversal interactively, and verify no Vulkan validation errors occur.
- [x] 4.3 Review `git diff` against the approved terrain scope and confirm no editor, terrain asset format, runtime sculpting, material-system, or unrelated architecture has been introduced.
- [x] 4.4 Raise the prototype ambient contribution from 0.03 to 0.12 and its validation cap from 0.08 to 0.20, retaining the existing point lights and flashlight; add deterministic environment-light validation coverage and re-run the affected rendering tests.
