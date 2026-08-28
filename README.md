# near-laugh

A purpose-built C++/Vulkan engine for a single-player
first-person shooter.

## Documentation

Vision: `docs/VISION.md`
Gameplay assumptions: `docs/GAMEPLAY.md`
Architecture: `docs/ARCHITECTURE.md`
Rendering: `docs/RENDERING.md`
Development: `docs/DEVELOPMENT.md`

## Runtime boundaries

The `fps` executable links the backend-neutral `near_laugh_runtime` facade.
Runtime composition owns `Platform -> Window -> Renderer` in that order and
destroys them in reverse. `near_laugh_platform` confines GLFW and physical
keyboard/mouse state; `near_laugh_render` confines Vulkan; the runtime maps the
physical state to the fixed controls for the one local FPS player.
Polling and blocking waits each open one input batch; the Engine samples a
waited batch before another poll can clear its cursor delta, while held actions
remain active. Each renderer request returns rendered, skipped, or recovered,
and the Engine exhaustively consumes that outcome while retaining loop and
application-lifetime control.

## Build

The project requires CMake, Ninja, a C++20 compiler, and a Vulkan 1.3 SDK.
Configure, build, and run the deterministic tests with:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

SPIR-V shaders are copied to `build/debug/bin/resources/shaders`; the launcher
uses the host's native process facility to discover the actual executable and
passes its adjacent resource root explicitly to the runtime. Asset loading is
therefore independent of both the current working directory and the spelling
of the invocation. Swapchain creation also validates color-attachment usage
and selects a supported composite-alpha mode from the queried surface
capabilities before calling Vulkan. See
`docs/DEVELOPMENT.md` for run commands and the optional Vulkan smoke preset.
