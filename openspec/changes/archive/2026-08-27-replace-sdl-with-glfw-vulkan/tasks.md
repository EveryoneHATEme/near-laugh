## 1. Migration Safety and Build Foundation

- [x] 1.1 Review the existing dirty-worktree diff, especially `src/core/render/graphics_pipeline.cpp`, map its intended vertex-data changes to the Vulkan replacement, and verify with `git diff` that no unrelated user-owned change is silently lost.
- [x] 1.2 Add a pinned GLFW FetchContent dependency and `find_package(Vulkan 1.3 REQUIRED)` while temporarily retaining the SDL target during migration, and verify `cmake --preset debug` resolves GLFW and `Vulkan::Vulkan` successfully.
- [x] 1.3 Add Ninja configure/build/test `debug` presets, project warning flags, the development validation definition, `build/debug/bin` runtime output, and shader copying; verify the generated cache is Debug/Ninja and copied SPIR-V files exist beside the executable output path.

## 2. Platform and Runtime Ownership

- [x] 2.1 Implement a non-copyable RAII platform owner that initializes GLFW once, installs actionable error reporting, and terminates GLFW once; verify focused tests or a small platform smoke path cover successful and failed initialization cleanup.
- [x] 2.2 Implement the no-client-API window owner with event polling, close state, framebuffer extent, resize/minimize signaling, and event waiting; verify a window smoke test closes cleanly and reports framebuffer rather than logical-window dimensions.
- [x] 2.3 Implement Vulkan extension discovery and surface creation behind the window boundary, keeping `GLFWwindow*` out of renderer and gameplay-facing headers; verify a boundary search finds GLFW types only in platform files and a smoke test creates and destroys a surface.
- [x] 2.4 Implement the engine-owned keyboard/mouse snapshot and cursor-capture operations needed by one FPS player; verify tests cover key/button state, per-event-batch cursor delta reset, and capture toggling without exposing GLFW values to consumers.
- [x] 2.5 Add the concrete `Engine` owner beneath `Application` with platform, window, and renderer members ordered by dependency; verify compile-time non-copyability and a lifecycle test demonstrate renderer destruction before window and platform teardown.

## 3. Vulkan 1.3 Context and Device

- [x] 3.1 Add Vulkan result/error helpers plus a non-copyable instance owner that requests Vulkan 1.3, platform extensions, development validation, and a debug messenger; verify unit tests cover result naming and a development smoke run reports the active validation state.
- [x] 3.2 Implement presentation-surface ownership and physical-device suitability checks for Vulkan 1.3, swapchain support, Dynamic Rendering, Synchronization 2, and graphics/present queues; verify pure selection helpers are unit-tested and unsupported requirements produce named errors.
- [x] 3.3 Implement logical-device and queue creation, enabling only the required Vulkan 1.3 features and `VK_KHR_swapchain`; verify startup logs the selected GPU and works with either shared or distinct graphics/presentation queue-family indices.
- [x] 3.4 Make every partially initialized context stage clean itself up exactly once and delete copy operations on all owners; verify failure-injection or focused tests exercise failure after instance, surface, and device creation without validation errors or leaked handles.

## 4. Swapchain and Frame Lifecycle

- [x] 4.1 Implement swapchain creation with supported SRGB format preference, FIFO presentation, clamped framebuffer extent, queue-family sharing, images, and image views; verify a windowed smoke run logs the chosen format/extent and presents with both shared and distinct queue-family logic covered by tests where possible.
- [x] 4.2 Implement two frame slots, each owning its command pool, command buffer, binary semaphores, and initially signaled completion fence; verify tests or instrumented smoke output show a slot waits for completion before reset and reuse.
- [x] 4.3 Implement image acquisition, per-image in-flight tracking, `vkQueueSubmit2`, semaphore ordering, and `vkQueuePresentKHR`; verify repeated frame submission runs without synchronization validation errors.
- [x] 4.4 Implement swapchain recreation for resize, zero-sized framebuffer, `VK_ERROR_OUT_OF_DATE_KHR`, and `VK_SUBOPTIMAL_KHR` using the correctness-first device-idle policy; verify minimize/restore and a forced recreation complete without recreating instance/device resources or producing validation errors.

## 5. Direct Vulkan Triangle Path

- [x] 5.1 Replace SDL shader loading with an explicit binary-file helper and Vulkan shader-module creation that reports asset paths on failure; verify unit tests cover missing and malformed-size SPIR-V input and successful loading of the project shaders.
- [x] 5.2 Implement the immutable triangle vertex buffer with three float position components and four normalized byte color components using host-visible coherent memory; verify static layout assertions and tests match the Vulkan attribute formats and offsets.
- [x] 5.3 Replace the SDL graphics pipeline with a Vulkan pipeline using `VkPipelineRenderingCreateInfo`, dynamic viewport/scissor, and no `VkRenderPass`; verify source inspection and a validation-enabled smoke run confirm that no legacy render-pass object is created.
- [x] 5.4 Record swapchain image transitions with `vkCmdPipelineBarrier2`, draw inside `vkCmdBeginRendering`/`vkCmdEndRendering`, and transition to presentation; verify the first visible frame contains the correctly colored triangle and validation reports no layout or synchronization errors.

## 6. Application Cutover and SDL Removal

- [x] 6.1 Switch `Application` and `main` to the `Engine` loop, including contextual top-level exception reporting and orderly close behavior; verify the `fps` executable opens, renders, responds to close, and returns a non-zero exit code on forced startup failure.
- [x] 6.2 Replace the GPU-backed constructor-only test with deterministic platform/input/Vulkan-helper unit tests and a separately labeled Vulkan smoke executable that renders fixed frames and forces one swapchain recreation; verify `ctest --preset debug --output-on-failure` runs deterministic tests and the smoke target can be selected explicitly.
- [x] 6.3 Remove SDL FetchContent/linkage, all SDL includes/types/calls, the obsolete SDL renderer files, and the superseded handwritten Makefile only after the GLFW/Vulkan path passes; verify `rg -n "SDL3|SDL_|basic_renderer" CMakeLists.txt src tests` returns no matches and the debug build still succeeds.
- [x] 6.4 Update README/development instructions only where needed to match the `fps` target, presets, Vulkan SDK requirement, shader location, and optional GPU smoke command; verify every documented configure, build, test, run, and smoke command names an existing preset or target.

## 7. Final Validation

- [x] 7.1 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`; verify all commands succeed with project warnings enabled and no warning is silently disabled.
- [x] 7.2 Run the development Vulkan smoke path through initial rendering, forced swapchain recreation, resize, minimize/restore, and shutdown; verify the Khronos validation layer reports no errors, or explicitly report if the layer or practical window interaction is unavailable.
- [x] 7.3 Review `git diff`, confirm no SDL or unintended platform abstraction remains, check that unrelated working-tree changes are preserved, and verify `openspec validate replace-sdl-with-glfw-vulkan --strict` succeeds before reporting implementation complete.
