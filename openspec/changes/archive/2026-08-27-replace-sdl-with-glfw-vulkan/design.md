## Context

See `proposal.md` for motivation. The current application owns an SDL window, an SDL GPU device, and an SDL graphics pipeline directly. The repository has no platform layer or `Engine` owner, its one test constructs the GPU-backed application, and the documented CMake preset workflow is not present. The target remains a single-player desktop FPS maintained by one developer, so the replacement must favor explicit ownership and a small direct Vulkan implementation over reusable graphics abstractions.

The behavioral contracts are defined by `specs/platform-windowing/spec.md` and `specs/vulkan-renderer/spec.md`. Existing uncommitted changes overlap `graphics_pipeline.cpp`; implementation must inspect and reconcile them rather than silently discarding user work.

## Goals / Non-Goals

**Goals:**

- Establish the documented `Application -> Engine -> Platform/Renderer` ownership direction for the subsystems that currently exist.
- Confine GLFW lifecycle and types to a small platform implementation while using Vulkan directly in the renderer.
- Make Vulkan instance, device, swapchain, per-frame, pipeline, buffer, and debug-resource lifetimes explicit and exception-safe.
- Produce a minimal, validation-clean Vulkan 1.3 triangle path that is a suitable foundation for later FPS rendering work.
- Make the documented debug configure, build, test, and run path reproducible.

**Non-Goals:**

- Implement gameplay, physics, audio, a general input-remapping system, or any of the later rendering features listed in `docs/RENDERING.md`.
- Create an RHI, generic GPU resource framework, render graph, custom allocator, descriptor-indexing system, or asynchronous renderer.
- Support OpenGL through GLFW or preserve SDL-compatible interfaces.
- Promise support for every desktop platform supported by GLFW; only the project's tested desktop environment is part of acceptance.

## Decisions

### 1. GLFW is a platform dependency, not a rendering backend

A small `Platform` owner will initialize and terminate GLFW. A `Window` owned beneath it will create a no-client-API desktop window, poll/wait for events, report framebuffer size, capture the cursor, and expose an engine-owned input snapshot. Its implementation will obtain required Vulkan instance extensions and create the `VkSurfaceKHR`, but it will not issue rendering commands.

GLFW headers and `GLFWwindow*` stay in the platform implementation. Renderer code receives a narrow `Window` reference for extension and surface operations; gameplay-facing code receives only engine-owned window/input values. Vulkan types may cross the platform-renderer boundary but do not cross into gameplay.

Alternatives considered:

- Native Win32 would remove an external window dependency but adds platform code and maintenance disproportionate to this single-developer project.
- Keeping SDL for windowing would reduce migration work but contradicts the explicit decision to remove SDL completely.
- Using GLFW's OpenGL path is rejected because Vulkan is the only rendering API.

### 2. Application and Engine establish explicit lifetime order

`Application` will own one `Engine`. `Engine` will own, in dependency order, the platform lifetime, window, and renderer. Destruction occurs in reverse: renderer and its surface first, then window, then platform termination. The main thread continues to own events, updates, and render submission.

Only currently required subsystems are introduced. `Engine` is a concrete FPS runtime owner, not a subsystem registry or extensibility framework.

Alternative considered: retaining direct ownership in `Application` is smaller by one class but continues to violate the documented subsystem lifetime boundary precisely where Vulkan teardown ordering matters.

### 3. Use the Vulkan C API and loader supplied by the Vulkan SDK

The renderer will include Vulkan headers directly and link `Vulkan::Vulkan` discovered by CMake. It will request Vulkan API version 1.3 and use core 1.3 entry points. No Volk, Vulkan-Hpp RAII layer, VMA, or loader abstraction is added in this foundation change.

Owning C++ classes will group resources by real lifetime boundary rather than wrap every handle generically:

- instance and debug messenger;
- surface, physical-device selection, logical device, and queues;
- swapchain, images, and image views;
- graphics pipeline and pipeline layout;
- vertex buffer and its memory;
- a fixed array of per-frame command and synchronization resources.

Every owning type is non-copyable. Handles start as `VK_NULL_HANDLE`; initialization failure invokes explicit cleanup for the successfully completed stages before propagating a contextual exception. Raw handles passed between owners are non-owning.

Alternatives considered:

- A generic templated Vulkan handle wrapper reduces repeated destructors but obscures parent/device-specific destruction and creates an abstraction before recurring needs are established.
- Vulkan-Hpp RAII is viable but introduces another ownership and exception model while the project documentation emphasizes explicit Vulkan behavior.

### 4. Require Vulkan 1.3 features instead of supporting compatibility paths

Instance creation requests Vulkan 1.3 plus GLFW-required extensions and, in development builds, `VK_EXT_debug_utils`. Physical-device selection requires presentation support, `VK_KHR_swapchain`, Dynamic Rendering, and Synchronization 2. Device creation enables the required Vulkan 1.3 feature bits and retrieves one graphics queue plus a distinct presentation queue only when the hardware requires it.

The first suitable modern device is accepted, with a simple preference for a discrete GPU when multiple suitable devices exist. The selected device and queue configuration are logged. Missing mandatory capabilities cause startup to fail with the missing capability named; there is no Vulkan 1.0/1.2, legacy render-pass, or legacy barrier path.

Alternative considered: supporting older Vulkan versions would immediately double rendering and synchronization paths and directly contradict `docs/RENDERING.md`.

### 5. Use a simple two-frames-in-flight model

The renderer uses two frame slots. Each slot owns one command pool, one primary command buffer, image-available and render-finished binary semaphores, and one initially-signaled completion fence. Before reusing a slot, the CPU waits for and resets its fence and command pool. Swapchain images track an associated in-flight fence when necessary to prevent reuse while earlier work remains pending.

Submission uses `vkQueueSubmit2`. Image transitions use `vkCmdPipelineBarrier2`, and drawing is enclosed by `vkCmdBeginRendering`/`vkCmdEndRendering`. Presentation retains `vkQueuePresentKHR`, since Synchronization 2 does not replace the presentation API.

Alternative considered: four frames preserve the current constant but consume more transient resources and add latency without a measured need. One frame is simpler but unnecessarily serializes all CPU/GPU progress.

### 6. Prefer correctness during swapchain recreation

The swapchain chooses a supported SRGB surface format where available, FIFO presentation, and the current framebuffer extent clamped to surface capabilities. A zero framebuffer extent suspends rendering and waits for events. Resize callbacks set a recreation flag; acquire/present out-of-date and suboptimal results feed the same path.

Recreation initially waits for the device to become idle, destroys only swapchain-lifetime resources, and rebuilds them using the new extent. Instance, surface, device, queues, frame slots, and device-lifetime resources remain intact. This deliberate stall is acceptable for resize and is easier to validate than deferred destruction.

Alternative considered: fence-tracked deferred swapchain destruction improves resize overlap but adds lifetime machinery without a demonstrated gameplay benefit.

### 7. Keep the triangle resources deliberately simple

The existing SPIR-V shaders remain build assets. The vertex layout is fixed as three 32-bit position floats followed by four normalized 8-bit color components, matching `VK_FORMAT_R32G32B32_SFLOAT` and `VK_FORMAT_R8G8B8A8_UNORM`. The tiny immutable triangle uses one host-visible, host-coherent vertex allocation; a staging allocator is deferred until real mesh uploads require it.

Pipeline creation uses `VkPipelineRenderingCreateInfo` with no `VkRenderPass`, and viewport/scissor are dynamic so swapchain resize does not require rebuilding the pipeline when its format is unchanged. Shader modules are temporary construction resources and are destroyed after pipeline creation.

Alternative considered: introducing VMA and staging buffers now would solve future asset-loading problems rather than a current requirement.

### 8. CMake expresses the required development contract

CMake will remove SDL FetchContent and link usage, discover the Vulkan SDK with `find_package(Vulkan 1.3 REQUIRED)`, and fetch a pinned GLFW release with its examples, tests, and documentation disabled. Project targets receive compiler warnings; development targets receive debug symbols, assertions, and a compile definition enabling Vulkan validation.

`CMakePresets.json` will provide the documented Ninja `debug` configure, build, and test presets and place the `fps` executable under `build/debug/bin`. Shader assets will be copied to the runtime output tree so running from that directory is deterministic. GPU-dependent smoke validation remains distinct from deterministic unit tests so machines without a desktop/Vulkan device can still run pure tests.

Alternative considered: retaining the handwritten Makefile would preserve two overlapping build interfaces and leave the documented preset workflow unverifiable.

## Risks / Trade-offs

- [Direct Vulkan introduces substantially more code than SDL GPU] -> Keep classes aligned with concrete resource lifetimes, implement only the triangle path, and reject generic rendering abstractions.
- [GLFW has process-global initialization and callbacks] -> Give one `Platform` object exclusive lifetime ownership and keep callbacks stateless or routed to the owning window instance.
- [GPU and window integration tests may not run on headless CI] -> Separate pure unit tests from an explicitly labeled Vulkan smoke executable/test and report when the smoke path cannot run.
- [Validation layers may be absent on an otherwise usable development system] -> Warn clearly, continue according to the spec, and record the missing validation step in the implementation report.
- [Device-idle swapchain recreation causes a visible resize stall] -> Accept the rare stall for correctness; optimize only if measurement shows gameplay impact.
- [Source-relative shader paths make execution directory-dependent] -> Copy shaders beside the configured executable and use one explicit runtime resource-root rule.
- [The working tree already contains edits to the renderer being replaced] -> Review the diff before implementation and preserve its intended vertex-data correction where compatible; never discard unrelated user work silently.

## Migration Plan

1. Record the current dirty-worktree diff and current smoke-test behavior so overlapping user edits can be reconciled explicitly.
2. Add the reproducible CMake preset and GLFW/Vulkan dependency configuration while keeping the existing target buildable during the transition.
3. Introduce the platform/window and ownership path, then add the direct Vulkan renderer from instance creation through triangle presentation.
4. Switch the application loop and tests to the new path; validate resize, minimize/restore, close, forced swapchain recreation, and partial-startup failures.
5. Remove SDL, the old SDL renderer implementations, SDL-specific build logic, and stale artifacts only after no project source or target references SDL.
6. Configure, build, run deterministic tests, run the validation-enabled Vulkan smoke path, review the final diff, and verify that documented commands work.

Rollback requires no data migration: revert the implementation commits to restore the SDL prototype. Planning artifacts remain as the record of the rejected approach unless the change is explicitly abandoned or archived.
