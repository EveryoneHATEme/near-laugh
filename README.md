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

## Build

The project requires CMake, Ninja, a C++20 compiler, and a Vulkan 1.3 SDK.
Configure, build, and run the deterministic tests with:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

SPIR-V shaders are copied to `build/debug/bin/resources/shaders`; the launcher
passes that executable-relative resource root explicitly to the runtime, so
asset loading does not depend on the current working directory. See
`docs/DEVELOPMENT.md` for run commands and the optional Vulkan smoke preset.
