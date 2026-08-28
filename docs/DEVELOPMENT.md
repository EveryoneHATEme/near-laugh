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
The launcher queries the actual running module through the host's native
process facility, derives and normalizes that executable-relative directory,
and passes it through `RuntimeConfig::resource_root`. Changing the process
working directory or invoking the process through misleading text does not
change asset lookup. The deterministic process test verifies this without
initializing GLFW or Vulkan.

## Build Targets

- `near_laugh_platform` contains GLFW windowing and physical input collection.
- `near_laugh_render` contains Vulkan and the internal surface bridge.
- `near_laugh_runtime` contains the public facade, composition, FPS input
  mapping, and main loop.
- `fps` is the launcher executable and links through `near_laugh_runtime`.

The deterministic suite includes public-header, target-interface, source, and
compile-command boundary checks. It also verifies waited input-batch sampling,
exhaustive runtime frame-outcome handling, native executable discovery, and
swapchain capability selection. A deliberately backend-dependent fixture is
expected to fail the public-header check.

## Vulkan Smoke Test

The normal `debug` test preset contains deterministic tests and excludes the
window/GPU-dependent smoke test. Run the smoke path explicitly with:

```sh
ctest --preset vulkan-smoke --output-on-failure
```

The smoke executable renders fixed frames and forces one swapchain recreation.
It requires a desktop session and a Vulkan 1.3 presentation-capable device.
It keeps validation diagnostics alive through renderer teardown and fails after
cleanup if Vulkan validation recorded an error. The smoke preset also verifies
the expected failure result for an injected validation error and checks normal
and partial-construction destruction order. Swapchain creation and recreation
use only color-attachment usage and composite-alpha modes reported by the
current surface.

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
