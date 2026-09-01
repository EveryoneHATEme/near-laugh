## 1. Camera Frame Foundation

- [x] 1.1 Add a pinned private GLM dependency with Vulkan zero-to-one depth configuration, and verify debug configuration succeeds while public-header and target-interface checks show no GLM usage requirement exposed by `near_laugh_runtime`.
- [x] 1.2 Extend the engine-owned render request with a standard-layout, column-major camera frame that contains no GLM or backend types, and verify frame-policy and boundary tests cover its construction and propagation.

## 2. Deterministic Free-Fly Camera

- [x] 2.1 Implement the concrete runtime-owned free-fly camera pose, basis, view/projection conversion, initial scene-facing pose, and framebuffer-aspect handling, and verify focused unit tests cover known transformed points, Vulkan clip conventions, and aspect changes.
- [x] 2.2 Implement mouse look, pitch limiting, orientation-relative horizontal motion, jump/crouch vertical motion, sprint speed, and normalized combined axes from `FpsActionSnapshot`, and verify deterministic camera tests cover every control and prevent diagonal overspeed.
- [x] 2.3 Add bounded elapsed-time and cursor-capture transition policies, including Escape release, primary-action recapture, release precedence, inactive navigation while released, and tracking reset across transitions; verify focused tests cover ordinary deltas, the 100 ms cap, blocked-wait reset, and both capture directions.

## 3. Built-In Static Scene and Shaders

- [x] 3.1 Replace the triangle fixture with a private immutable scene builder for a floor, boundary geometry, and multiple colored boxes or pillars baked into one world-space triangle stream, and verify scene tests check non-empty geometry, required components, valid vertex layout, and overlapping depth from the initial pose.
- [x] 3.2 Replace triangle shader sources and committed SPIR-V with prototype-scene shaders that apply the camera view-projection push constant and preserve vertex color, and verify shader compilation succeeds with the Vulkan SDK toolchain.
- [x] 3.3 Update runtime shader resolution, executable-relative packaging, missing-resource diagnostics, pipeline push-constant layout, vertex upload, and draw count for the new scene resources, and verify resource, process-layout, vertex-layout, and pipeline-boundary tests no longer depend on triangle names or behavior.

## 4. Depth-Buffered Vulkan Rendering

- [x] 4.1 Add deterministic depth-format selection and memory-selection coverage for the renderer's supported candidate list, and verify tests cover preferred selection, fallback selection, and actionable failure when no candidate is usable.
- [x] 4.2 Add RAII ownership for one device-local depth image, allocation, view, and initialization state per swapchain image, integrate it into swapchain creation/cleanup/recreation and partial-construction failure paths, and verify lifecycle tests cover normal and failed teardown without leaks or double destruction.
- [x] 4.3 Enable compatible pipeline depth state, record Synchronization 2 depth transitions, clear and attach the per-image depth target through Dynamic Rendering, and push the current camera matrix before the scene draw; verify focused command/pipeline checks pass and no legacy render-pass or synchronization API is introduced.

## 5. Runtime Integration

- [x] 5.1 Integrate steady-clock sampling, cursor capture, free-fly updates, framebuffer aspect, and camera-frame submission into the Engine-owned loop while preserving close, minimized wait, and exhaustive renderer-outcome behavior; verify runtime/input tests cover normal render, release/recapture, minimize/restore without a camera jump, close, skipped, and recovered paths.
- [x] 5.2 Strengthen source and dependency boundary checks so renderer code cannot consume FPS actions or timing and runtime/public headers cannot expose Vulkan, GLFW, or GLM types; verify the deterministic boundary suite accepts the implementation and still rejects its forbidden fixtures.

## 6. Documentation and Integrated Validation

- [x] 6.1 Update `README.md` and the relevant architecture, gameplay, rendering, and development documentation with the free-fly controls, built-in-scene scope, camera/render ownership, depth lifetime, and explicit absence of collision or gravity; verify the documented run and resource paths match the implementation.
- [x] 6.2 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`, and verify all camera, scene, renderer, process, source, header, and target-boundary tests pass.
- [x] 6.3 Run `ctest --preset vulkan-smoke --output-on-failure`, and verify scene rendering, depth attachment use, forced swapchain recreation, lifecycle cleanup, injected-validation failure handling, and zero unexpected validation errors.
- [x] 6.4 Run the debug `fps` executable when a presentation-capable desktop is available and visually verify the initial scene, occlusion, resize aspect, WASD/mouse/vertical/sprint navigation, cursor release/recapture, and unconstrained passage through geometry; report any part that cannot be performed.
- [x] 6.5 Review `git diff` for unrelated changes or unsupported abstractions and run `openspec validate add-free-fly-prototype-scene --strict`, verifying the implementation and planning artifacts remain within the approved prototype scope.
