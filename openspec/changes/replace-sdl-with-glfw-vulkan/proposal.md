## Why

The current renderer is built on the cross-API SDL GPU abstraction, so it cannot enforce the project's required Vulkan 1.3 baseline, Dynamic Rendering, Synchronization 2, or explicit Vulkan validation and lifetime rules. Replacing SDL now, while the runtime is still a small triangle prototype, avoids carrying the wrong rendering and platform boundary into gameplay development.

## What Changes

- **BREAKING** Remove SDL from the runtime, renderer API, tests, and build dependencies; existing SDL-based engine interfaces are not preserved.
- Add a small GLFW-backed platform/window layer for window lifecycle, event polling, keyboard and mouse input access, Vulkan instance-extension discovery, and surface creation.
- Replace SDL GPU objects and commands with direct Vulkan 1.3 instance, physical-device, logical-device, queue, swapchain, pipeline, buffer, command, synchronization, and presentation management.
- Require Dynamic Rendering and Synchronization 2 instead of maintaining legacy Vulkan rendering or synchronization paths.
- Enable the Khronos validation layer and debug messenger in development builds when available, and surface actionable Vulkan errors during startup and rendering.
- Preserve the current executable smoke behavior: open a desktop window, render the colored triangle, handle resize/out-of-date swapchains, and exit through the window close event.
- Establish explicit RAII ownership and reverse-order shutdown for the GLFW platform state, window, Vulkan surface, renderer resources, and per-frame resources.
- Update CMake and validation coverage for GLFW, the Vulkan SDK/loader, shader assets, and the migrated runtime.

## Capabilities

### New Capabilities

- `platform-windowing`: GLFW-backed desktop window, event, input, Vulkan extension, and surface lifecycle behavior without exposing GLFW to gameplay code.
- `vulkan-renderer`: Direct Vulkan 1.3 device, swapchain, frame, resource, validation, and triangle rendering behavior.

### Modified Capabilities

None. The project currently has no main OpenSpec capabilities.

## Impact

- Affected code: application startup and loop, platform/window ownership, renderer and graphics-pipeline implementation, shader/resource loading boundaries, tests, and CMake configuration.
- Dependency changes: SDL3 is removed; GLFW and the Vulkan SDK/loader become required dependencies.
- API changes: all `SDL_*` types and functions disappear from project headers and runtime code. Renderer-facing platform integration is replaced by narrow window/surface operations.
- Runtime compatibility: this change targets the existing desktop-PC scope. GLFW provides native window/input integration, but this change does not add or promise support for additional platforms.
- Scope exclusions: no gameplay systems, generic RHI, alternative rendering backend, ECS, render graph, job system, bindless renderer, or asynchronous compute architecture are introduced.
