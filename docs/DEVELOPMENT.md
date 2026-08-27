# Development

## Required Tools

- CMake
- Ninja
- C++ compiler
- Vulkan SDK 1.3 or newer
- Git

## Configure

cmake --preset debug

## Build

cmake --build --preset debug

## Test

ctest --preset debug --output-on-failure

## Run

On Windows:

```powershell
.\build\debug\bin\fps.exe
```

On other supported desktop environments:

```sh
./build/debug/bin/fps
```

The executable loads its copied SPIR-V assets from
`resources/shaders` beneath the runtime output directory.

## Vulkan Smoke Test

The normal `debug` test preset contains deterministic tests and excludes the
window/GPU-dependent smoke test. Run the smoke path explicitly with:

```sh
ctest --preset vulkan-smoke --output-on-failure
```

The smoke executable renders fixed frames and forces one swapchain recreation.
It requires a desktop session and a Vulkan 1.3 presentation-capable device.

## Debug Build Requirements

Development builds should enable:

- compiler warnings
- assertions
- Vulkan validation layers
- debug symbols

Warnings should not be silently ignored.

## Validation

Before completing an implementation task:

1. configure successfully;
2. build successfully;
3. run affected tests;
4. run the relevant executable when practical;
5. verify that Vulkan validation reports no new errors;
6. review the final diff.

A task is not complete solely because the code compiles.
