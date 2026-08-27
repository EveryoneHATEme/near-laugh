# near-laugh

A purpose-built C++/Vulkan engine for a single-player
first-person shooter.

## Documentation

Vision: `docs/VISION.md`
Gameplay assumptions: `docs/GAMEPLAY.md`
Architecture: `docs/ARCHITECTURE.md`
Rendering: `docs/RENDERING.md`
Development: `docs/DEVELOPMENT.md`

## Build

The runtime target is `fps` and requires CMake, Ninja, a C++20 compiler, and a
Vulkan 1.3 SDK. Configure, build, and run the deterministic tests with:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

SPIR-V shaders are copied to `build/debug/bin/resources/shaders`. See
`docs/DEVELOPMENT.md` for run commands and the optional Vulkan smoke preset.
