## 1. Parser Dependency and Packaged Asset

- [x] 1.1 Add `cgltf` at a fixed commit through CMake, compile its implementation once inside `near_laugh_render`, keep its headers and target private, and verify a fresh debug configure plus runtime-interface boundary tests expose no `cgltf` dependency.
- [x] 1.2 Add the project-owned bounded `resources/models/prototype_chair.glb`, copy the models directory into executable and Vulkan-smoke runtime layouts, and verify the packaged file satisfies the accepted one-scene/one-node/one-mesh/one-triangle-primitive profile.

## 2. World Description and Runtime Resources

- [x] 2.1 Add the one concrete prototype static-prop placement, obstacle surface role, and box-proxy data with finite/positive validation, and verify world tests cover the valid chair plus non-finite transform, non-positive scale, and invalid proxy cases.
- [x] 2.2 Extend runtime resource resolution with the normalized absolute chair GLB path and missing-file diagnostics, and verify unit and executable-layout process tests find it independently of the working directory and report its resolved path when absent.
- [x] 2.3 Compose the resolved model path with the filesystem-free prototype prop description at renderer construction without changing the public facade, and verify ownership/source-boundary and minimal-consumer tests still pass.

## 3. Static GLB CPU Loading

- [x] 3.1 Implement renderer-private file parsing, `cgltf` RAII cleanup, structural validation, and the narrower engine profile checks, and verify temporary malformed/unsupported GLB tests cover required extensions, external buffers, child nodes, extra meshes/primitives, non-triangle modes, sparse accessors, skins, animations, and morph targets with path-bearing errors.
- [x] 3.2 Decode required position, normal, UV, and optional unsigned index accessors into a non-indexed opaque triangle stream, and verify generated indexed and non-indexed GLB fixtures preserve declared order while missing, mismatched, out-of-range, empty, and oversized data fail deterministically.
- [x] 3.3 Combine the root-node and prototype placement transforms, apply inverse-transpose normal conversion, normalize and validate results, and assign white tint plus the obstacle texture layer; verify selected fixture vertices cover translation, yaw, uniform and node scaling, finite UV preservation, singular transforms, and non-finite data.
- [x] 3.4 Load the packaged chair through the production loader and verify it produces a non-empty triangle stream divisible by three with finite world-space positions, unit normals, preserved UVs, white tint, and only the obstacle layer.

## 4. Static Collision Proxy

- [x] 4.1 Create one static Jolt box body from the prototype chair proxy without adding model-loader or renderer dependencies to physics, and verify headless physics tests confirm body lifetime, placement, player blocking, and cleanup on normal and partial startup.

## 5. Vulkan Mesh Ownership and Drawing

- [x] 5.1 Extract host-visible coherent vertex-buffer allocation, upload, binding, and cleanup from `GraphicsPipeline` into a renderer-private immutable mesh-buffer RAII owner, and verify focused tests and lifecycle events cover invalid vertex counts, normal destruction, and each partial-construction failure stage.
- [x] 5.2 Make `GraphicsPipeline` own only pipeline/layout state, construct separate generated-world and imported-chair mesh buffers during renderer startup, and record deterministic world-then-chair draws with one pipeline, descriptor pair, and scene push constant; verify renderer tests observe two valid mesh draws without another pipeline or descriptor update.
- [x] 5.3 Keep both mesh buffers alive across swapchain recreation and destroy them before the logical device after dependent pipeline work, and verify the Vulkan smoke test detects no re-upload on forced recreation, balanced model/world buffer lifetimes, correct injected-failure cleanup, or validation errors.

## 6. Documentation and Full Validation

- [x] 6.1 Update architecture, rendering, development, and gameplay-facing prototype documentation with the bounded GLB profile, chair controls/appearance/collision behavior, resource layout, two-draw ownership, and explicit non-goals; verify documentation matches the implemented filenames and runtime behavior.
- [x] 6.2 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`, and verify configuration, compilation, deterministic tests, process tests, and boundary tests all pass.
- [x] 6.3 Run `ctest --preset vulkan-smoke --output-on-failure` and interactively inspect the chair from the initial route under point and flashlight illumination, including collision and swapchain recreation; verify Vulkan validation reports no errors or record any environment-dependent validation that could not be performed.
  - Automated Vulkan smoke, forced swapchain recreation, collision, lifecycle, and validation checks passed. Interactive visual inspection was not performed in the non-interactive agent session.
- [x] 6.4 Review the final `git diff` for scope and ownership correctness and run strict OpenSpec validation for `load-static-glb-model`; verify no unrelated refactor, general asset/material system, public backend leakage, or incomplete task remains before completion.
