## Why

The Vulkan/GLFW migration established the correct graphics foundation, but its public headers and control flow still expose backend details and mix application, platform, input, and renderer responsibilities. Tightening these boundaries before gameplay and mesh systems arrive prevents those systems from depending on Vulkan, GLFW lifecycle, working-directory assumptions, or renderer-controlled event processing.

## What Changes

- **BREAKING** Replace the concrete-subsystem public `Engine` shape with a narrow runtime API whose headers and usage requirements do not expose Vulkan or GLFW interfaces.
- Establish enforceable build/module boundaries for runtime composition, platform implementation, FPS input, and Vulkan rendering without introducing a generic subsystem framework.
- Move event polling, close handling, minimize waiting, framebuffer-resize coordination, and frame-loop decisions into the main-thread Engine flow; rendering reports outcomes instead of controlling window events or application termination.
- Express the required `Platform -> Window` lifetime in construction APIs instead of relying only on member declaration order and mutable process-global bookkeeping.
- Keep GLFW callbacks and physical keyboard/mouse state inside the platform boundary, then map that state to the small fixed set of FPS actions in an engine-level input component.
- Add an explicit runtime resource root so shader loading does not depend on the process working directory.
- Make Vulkan validation errors observable to the smoke harness so a run fails when validation reports an error rather than merely printing it.
- Remove or internalize production API hooks that exist only to drive tests, while retaining deterministic failure and swapchain-recreation coverage through test-scoped seams.

## Capabilities

### New Capabilities

- `runtime-composition`: Backend-neutral runtime API, explicit subsystem construction/lifetime, resource configuration, and main-thread loop ownership.
- `fps-input`: Concrete keyboard/mouse-to-action mapping for the single local FPS player, independent of the platform library.

### Modified Capabilities

- `platform-windowing`: Make platform lifetime explicit, expose physical input rather than FPS action semantics, and keep all platform/backend types behind the platform implementation boundary.
- `vulkan-renderer`: Make frame rendering independent of event-loop control, consume explicit runtime resources/framebuffer state, and turn validation errors into a test-observable failure condition.

## Impact

- Affected code: application and Engine headers/implementation, platform/window/input code, renderer construction and frame API, shader-path resolution, Vulkan validation reporting, smoke tests, ownership tests, and CMake target organization.
- API impact: current direct construction and inclusion of `Platform`, `Window`, and `Renderer` from runtime consumers changes; renderer frame results and configuration become explicit values.
- Dependency impact: Vulkan and GLFW remain required implementation and final-link dependencies, but their headers and usage requirements no longer propagate through the gameplay-facing/runtime-facing interface.
- Runtime behavior: close, resize, minimize/restore, rendering, input actions, and triangle output remain externally equivalent; startup and smoke validation failures become more deterministic and actionable.
- Scope exclusions: graphics-pipeline/mesh separation, new rendering features, fixed-timestep gameplay simulation, configurable key bindings, additional platforms, RHI abstractions, ECS, jobs, and render graphs are not part of this change.
