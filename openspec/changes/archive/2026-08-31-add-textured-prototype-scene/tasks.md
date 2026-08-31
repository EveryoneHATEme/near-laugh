## 1. Texture Assets and Resource Contract

- [x] 1.1 Add a pinned `stb_image` revision as a private `near_laugh_render` dependency with only PNG decoding enabled, wrap forced-RGBA decoding in an engine-owned value, and verify build metadata and boundary tests expose no decoder headers or types outside the render/resource implementation.
- [x] 1.2 Create the four distinct tileable 256-by-256 opaque PNG assets for floor, boundary, obstacle, and shooting-target surfaces under `resources/textures`, and verify an asset test decodes every file to the required dimensions and RGBA byte count with fully opaque alpha.
- [x] 1.3 Extend internal runtime and renderer resource values with the four fixed texture paths, require them beneath the normalized explicit resource root, copy the texture directory beside the FPS, Vulkan smoke, and resource-layout probe executables, and verify resource tests cover another working directory plus the resolved path for each missing texture.

## 2. Prototype Surface and Vertex Data

- [x] 2.1 Add the four-value immutable prototype surface role, assign and validate one role for every built-in solid independently of structural kind, and verify world tests cover floor/boundary/obstacle/step/low-clearance/target mappings plus rejection of invalid roles.
- [x] 2.2 Extend the scene vertex and Vulkan attribute contract with two UV floats and one unsigned texture layer, generate one-repeat-per-metre coordinates with explicit mappings for all six box faces, and verify prototype-scene and vertex-layout tests cover finite values, shared-corner continuity, face orientation, large-surface repetition, stable layer mapping, offsets, formats, and standard layout.

## 3. Sampled Texture Vulkan Ownership

- [x] 3.1 Add deterministic helpers for full mip-count calculation and required `VK_FORMAT_R8G8B8A8_SRGB` optimal-tiling feature selection, and verify Vulkan utility tests cover valid dimensions, complete mip counts, supported capabilities, and actionable rejection of missing sampling/filter/blit support.
- [x] 3.2 Implement the concrete renderer-private sampled-texture RAII owner with contiguous staging, one four-layer device-local image, full GPU-generated mip chains, an array view, repeat/linear sampler, and Synchronization 2 upload submitted with `vkQueueSubmit2`; verify focused tests or smoke instrumentation cover final shader-read layouts and balanced normal/partial-construction cleanup.
- [x] 3.3 Give the sampled-texture owner one combined image-sampler layout, pool, and immutable descriptor set, expose only non-owning Vulkan handles to the scene pipeline, and verify ownership/source tests cover one descriptor, no post-startup descriptor update, and destruction after the pipeline but before the Vulkan device.

## 4. Single-Draw Textured Scene Integration

- [x] 4.1 Create the sampled texture once after Vulkan context creation, pass its descriptor handles into every format-dependent scene pipeline construction, and verify renderer lifecycle tests demonstrate that forced swapchain recreation rebuilds no texture image, sampler, or descriptor and retains exactly one opaque scene draw.
- [x] 4.2 Update the scene pipeline layout and draw recording to bind set 0 binding 0 without changing the 112-byte push constant or `FrameRequest`, and verify boundary/layout tests preserve backend-neutral frame data, target presentation masks, one descriptor bind, and one draw command.
- [x] 4.3 Update the prototype vertex and fragment shaders to pass UV/layer data, sample the texture array, multiply by authored tint, apply highlight before dimming, and then apply existing lighting; rebuild committed SPIR-V and verify shader-source, SPIR-V resource, CPU/GPU location, and push-layout tests agree.
- [x] 4.4 Extend Vulkan failure injection and smoke coverage across texture upload, descriptor creation, target highlight/dim states, resize, and forced swapchain recreation, and verify every path performs orderly cleanup with zero error-severity validation messages outside the intentional injected-validation case.

## 5. Documentation and Full Validation

- [x] 5.1 Update architecture, rendering, development, gameplay where relevant, and README descriptions for fixed surface roles, executable-relative texture packaging, mipmapped sampling, descriptor/texture ownership, and explicit non-goals; verify documentation names all four textures and retains the single-draw/non-general-purpose boundaries.
- [x] 5.2 Configure and build the debug preset and run `ctest --preset debug --output-on-failure`; verify compiler selection remains Clang and every deterministic, resource-layout, ownership, shader, vertex, and boundary test passes.
- [x] 5.3 Run `ctest --preset vulkan-smoke --output-on-failure` and the FPS executable on a presentation-capable desktop; verify tile scale and orientation across all box faces, stable minification on distant floor/walls, distinct four-role appearance, preserved target highlight/dim precedence, swapchain recovery, and no Vulkan validation errors, reporting any unavailable manual validation.
- [x] 5.4 Review `git diff` for scope, binary texture/SPIR-V assets, dependency pinning, CPU/GPU layout agreement, synchronization and partial-failure cleanup, and unrelated edits, then verify `openspec validate add-textured-prototype-scene --strict` succeeds before reporting implementation complete.
