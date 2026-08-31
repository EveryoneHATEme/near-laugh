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

The executable loads its copied SPIR-V and texture assets beneath the runtime
output directory. The required shader files are
`resources/shaders/prototype_scene_vertex.spv` and
`resources/shaders/prototype_scene_fragment.spv`. The required fixed texture
files are `resources/textures/prototype_floor.png`,
`prototype_boundary.png`, `prototype_obstacle.png`, and
`prototype_shooting_target.png`.
The launcher queries the actual running module through the host's native
process facility, derives and normalizes that executable-relative directory,
and passes it through `RuntimeConfig::resource_root`. Changing the process
working directory or invoking the process through misleading text does not
change asset lookup. The deterministic process test verifies this without
initializing GLFW or Vulkan.

The prototype begins with the cursor captured. Use the mouse to look, W/A/S/D
to walk, Left Shift to sprint, Space to jump while grounded, and hold Left
Control to crouch. Escape releases the cursor; the left mouse button recaptures
it while released and fires the automatic prototype rifle while captured. A
recapture click is suppressed until the button is released. Movement uses
gravity and static collision, slides along walls, traverses
the cyan 0.30 m step, and can pass beneath the purple low-clearance roof only
while crouched. The opaque scene tiles its fixed floor, boundary, obstacle, and
shooting-target textures once per metre across explicit outward-oriented box
face UVs. A complete GPU-generated mip chain stabilizes distant floor and wall
sampling. A dim cool point light surrounds the spawn and a warmer point light
marks the destination; their finite non-overlapping radii leave a deliberately
dark transition over a near-black ambient floor. Three target plates form the
shooting range: hits
flash with a bright orange highlight, four damaging hits destroy a plate, and
destroyed plates remain visibly dimmed and collidable. Rifle recoil is bounded and
recovers through fixed simulation. This milestone deliberately has no
finite ammunition or ammo tracking, reloads, switching, spread, projectiles,
weapon model, crosshair, audio, particles, enemies/AI, physical target
destruction, shadows, general materials, arbitrary textures, runtime asset
discovery or streaming, descriptor indexing, flashlight, arbitrary or mutable
lights, fog, exposure adaptation, or HDR post-processing.

## Build Targets

- `near_laugh_platform` contains GLFW windowing and physical input collection.
- `near_laugh_world` contains immutable prototype solids and the player spawn.
- `near_laugh_physics` privately links Jolt v5.6.0 and contains its RAII
  lifetime, single-threaded static world, and virtual character.
- `near_laugh_render` contains Vulkan and the internal surface bridge.
- `near_laugh_runtime` contains the public facade, composition, FPS input
  mapping, player policy, the concrete rifle/target gameplay, fixed-step
  shooting coordination, interpolation, and main loop.
- `fps` is the launcher executable and links through `near_laugh_runtime`.

The deterministic suite includes public-header, target-interface, source, and
compile-command boundary checks. It also verifies waited input-batch sampling,
exhaustive runtime frame-outcome handling, fixed-step timing, grounded
player/camera policy, headless Jolt collision, built-in shared scene
composition, static ray queries, rifle/target determinism, native executable
discovery, depth/memory selection, push-constant and vertex layouts, and
swapchain capability selection. It also verifies fixed texture decoding and
resource layout, surface-role mapping, world-scaled UV/layer generation,
texture mip counts and format requirements, the six-location CPU/GPU vertex
contract, the 80-byte push constant, two-light `std140` upload layout, ordered
texture/lighting descriptors, and single-draw ownership boundaries.
Deliberately Vulkan- and Jolt-dependent fixtures are expected to fail the
public-header check.

## Vulkan Smoke Test

The normal `debug` test preset contains deterministic tests and excludes the
window/GPU-dependent smoke test. Run the smoke path explicitly with:

```sh
ctest --preset vulkan-smoke --output-on-failure
```

The smoke executable renders the depth-buffered, locally lit and mipmapped
textured prototype scene for fixed frames and forces one swapchain
recreation.
It requires a desktop session and a Vulkan 1.3 presentation-capable device.
It keeps validation diagnostics alive through renderer teardown and fails after
cleanup if Vulkan validation recorded an error. The smoke preset also verifies
the expected failure result for an injected validation error and checks normal
and partial-construction destruction order, including injected failures across
texture staging/upload, view/sampler/descriptor creation, immutable lighting
buffer/memory/upload/descriptor creation, and depth attachment creation. It
verifies the immutable texture and lighting descriptors survive forced
swapchain recreation without another update. Swapchain creation and recreation use only
color-attachment usage and composite-alpha modes reported by the current
surface, and own one depth target per swapchain image.

When running the FPS or smoke executable interactively, inspect differently
oriented faces for consistent one-metre tile scale and orientation, verify
distant floors and walls select stable mip levels, and confirm no face appears
lit from the inward normal. Also confirm all four surface roles look distinct,
target hits highlight immediately,
destroyed plates remain dim after the final highlight, recoil stays bounded
and recovers, a recapture click does not fire, and held fire respects its
cadence. Confirm the spawn is contained by a dim cool pool, the destination is
warmer, the intervening route falls into intentional darkness, light does not
leak beyond either authored radius, textures still respond to surface
orientation, and live/highlighted/dimmed target states remain readable. Treat
every error-severity Vulkan validation message as a failure.

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
