## 1. Level Data and Validation

- [x] 1.1 Add an exact pinned `nlohmann/json` dependency linked privately to `near_laugh_world`, and verify CMake target-interface checks show no JSON include or target leaking through runtime public headers.
- [x] 1.2 Introduce the bounded `LevelDocument` data, format constants, level diagnostics, and validated immutable `PrototypeLevel` construction path, and verify focused tests cover the 97-by-97 terrain, 240-solid cap, fixed spawn, two-light environment, and one-prop constraints.
- [x] 1.3 Replace Boolean-only validation authority with field-aware structural and gameplay diagnostics while retaining necessary compatibility wrappers, and verify tests cover finite values, slopes, spawn support/clearance, solids, lights, and prop proxy locations.

## 2. Strict Level Codec

- [x] 2.1 Implement strict version-1 JSON parsing into project-owned values with complete required/unknown-field and enum-string checks, and verify table-driven tests reject malformed syntax, unsupported versions, unknown fields, missing fields, invalid array sizes, paths, and excessive solids with contextual diagnostics.
- [x] 2.2 Implement canonical locale-independent JSON serialization with stable root/member/object order, round-trip float precision, LF endings, and one trailing newline, and verify semantic and byte-identical load-save-load-save tests under multiple locales.
- [x] 2.3 Implement validation-gated atomic file saving and explicit filesystem diagnostics, and verify failure tests preserve the prior destination and report unwritable, replacement, and invalid-document cases.

## 3. Prototype Asset Migration

- [x] 3.1 Create `resources/levels/prototype.level.json` from the existing authored terrain, solids, spawn, two lights, ambient value, and chair placement, and verify a semantic parity test compares every loaded value with the pre-migration prototype fixture.
- [x] 3.2 Extend runtime resource resolution and build copying for the fixed level path, and verify process/resource-layout tests find it independently of working directory and report its resolved path when absent.
- [x] 3.3 Change engine composition to load and validate the level before physics and renderer construction, and verify injected missing, parse, validation, physics, and renderer failures preserve dependency-safe cleanup and never enter the main loop incorrectly.
- [x] 3.4 Remove hard-coded level authoring from `PrototypeLevel` while preserving shared terrain math and immutable consumer interfaces, and verify existing world, physics, static-model, scene, lighting, and texture tests pass against the loaded asset.

## 4. Documentation and Validation

- [x] 4.1 Update vision-adjacent architecture, rendering, gameplay, development, and run/resource documentation for the packaged level boundary and immutable runtime policy, and verify documented paths and limits match the implementation.
- [x] 4.2 Configure and build the debug preset, run the deterministic suite, run the FPS from a different working directory, and verify scene appearance, traversal, lighting, chair collision, and validation diagnostics remain equivalent.
- [x] 4.3 Run the Vulkan smoke preset and inspect for validation errors, run `openspec validate add-bounded-level-persistence --strict`, and review `git diff` for unrelated refactors or editor/runtime-mutation scope.
