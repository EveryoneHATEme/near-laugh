# Development

## Required Tools

- CMake
- Ninja
- Clang C and C++ compilers, discoverable as `clang` and `clang++`
- Vulkan SDK 1.3 or newer
- Git

## Configure

Confirm that both portable compiler names resolve, then configure the standard
debug preset:

```sh
clang --version
clang++ --version
cmake --preset debug
```

The preset selects Clang for both C and C++ before either language is enabled;
it does not hard-code a local LLVM installation path. If `build/debug` already
contains a cache created with another compiler, discard the cached selection
through CMake's fresh-configuration mode:

```sh
cmake --preset debug --fresh
```

On Windows, the selected `clang`/`clang++` GNU-style frontends target the
MSVC-compatible Windows ABI so the build continues to use the Microsoft
runtime, Windows SDK, Vulkan SDK, and compatible native dependencies. CMake
therefore records `MSVC` as the compiler simulation ID; this ABI label does not
mean that the MSVC compiler was selected.

## Verify Compiler Selection

After configuration, PowerShell users can inspect the resolved compiler paths
and generated compiler identities with:

```powershell
Select-String -Path build/debug/CMakeCache.txt -Pattern '^CMAKE_(C|CXX)_COMPILER:'
Get-ChildItem build/debug/CMakeFiles/*/CMakeCCompiler.cmake,
              build/debug/CMakeFiles/*/CMakeCXXCompiler.cmake |
    Select-String '^set\(CMAKE_(C|CXX)_COMPILER |COMPILER_ID|COMPILER_FRONTEND_VARIANT|SIMULATE_ID'
```

On POSIX shells, use:

```sh
grep -E '^CMAKE_(C|CXX)_COMPILER:' build/debug/CMakeCache.txt
grep -E '^set\(CMAKE_(C|CXX)_COMPILER |COMPILER_ID|COMPILER_FRONTEND_VARIANT|SIMULATE_ID' \
    build/debug/CMakeFiles/*/CMakeCCompiler.cmake \
    build/debug/CMakeFiles/*/CMakeCXXCompiler.cmake
```

Both compiler IDs must be `Clang` and both frontend variants must be `GNU`.
Windows builds must additionally report `MSVC` simulation IDs for both
languages.

## Build

```sh
cmake --build --preset debug
```

## Test

```sh
ctest --preset debug --output-on-failure
```

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
`resources/shaders` beneath the runtime output directory. The required files
are `prototype_scene_vertex.spv` and `prototype_scene_fragment.spv`.
The launcher queries the actual running module through the host's native
process facility, derives and normalizes that executable-relative directory,
and passes it through `RuntimeConfig::resource_root`. Changing the process
working directory or invoking the process through misleading text does not
change asset lookup. The deterministic process test verifies this without
initializing GLFW or Vulkan.

The prototype begins with the cursor captured. Use the mouse and W/A/S/D to
look and move, Space/Left Control for vertical motion, and Left Shift to
sprint. Escape releases the cursor; the left mouse button recaptures it.
Movement intentionally ignores collision and gravity.

## Build Targets

- `near_laugh_platform` contains GLFW windowing and physical input collection.
- `near_laugh_render` contains Vulkan and the internal surface bridge.
- `near_laugh_runtime` contains the public facade, composition, FPS input
  mapping, and main loop.
- `fps` is the launcher executable and links through `near_laugh_runtime`.

The deterministic suite includes public-header, target-interface, source, and
compile-command boundary checks. It also verifies waited input-batch sampling,
exhaustive runtime frame-outcome handling, free-fly camera transforms and
control policy, built-in scene composition, native executable discovery,
depth/memory selection, push-constant and vertex layouts, and swapchain
capability selection. A deliberately backend-dependent fixture is expected to
fail the public-header check.

## Vulkan Smoke Test

The normal `debug` test preset contains deterministic tests and excludes the
window/GPU-dependent smoke test. Run the smoke path explicitly with:

```sh
ctest --preset vulkan-smoke --output-on-failure
```

The smoke executable renders the depth-buffered prototype scene for fixed
frames and forces one swapchain recreation.
It requires a desktop session and a Vulkan 1.3 presentation-capable device.
It keeps validation diagnostics alive through renderer teardown and fails after
cleanup if Vulkan validation recorded an error. The smoke preset also verifies
the expected failure result for an injected validation error and checks normal
and partial-construction destruction order, including an injected failure after
a depth attachment is created. Swapchain creation and recreation use only
color-attachment usage and composite-alpha modes reported by the current
surface, and own one depth target per swapchain image.

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
