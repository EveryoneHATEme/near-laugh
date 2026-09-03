# Development

## Required Tools

- CMake
- Ninja
- Clang C and C++ compilers available as `clang` and `clang++`
- Vulkan SDK 1.3 or newer
- Git

## Configure, Build, and Test

The standard debug workflow is:

```sh
clang --version
clang++ --version
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Use CMake's fresh mode when the build tree contains a stale compiler or target
configuration:

```sh
cmake --preset debug --fresh
```

The preset selects the portable Clang executable names before either language
is enabled. On Windows these frontends target the MSVC-compatible ABI and use
the Microsoft runtime and Windows SDK; `MSVC` in CMake's simulation metadata
describes the ABI, not the selected compiler.

To inspect compiler selection in PowerShell:

```powershell
Select-String -Path build/debug/CMakeCache.txt -Pattern '^CMAKE_(C|CXX)_COMPILER:'
Get-ChildItem build/debug/CMakeFiles/*/CMakeCCompiler.cmake,
              build/debug/CMakeFiles/*/CMakeCXXCompiler.cmake |
    Select-String '^set\(CMAKE_(C|CXX)_COMPILER |COMPILER_ID|COMPILER_FRONTEND_VARIANT|SIMULATE_ID'
```

Both compiler IDs and frontend variants must report Clang and GNU
respectively. Windows additionally reports MSVC simulation IDs.

## Run

On Windows:

```powershell
.\build\debug\bin\near_laugh.exe
.\build\debug\bin\level_editor.exe
.\build\debug\bin\level_editor.exe .\resources\levels\prototype.level.json
```

On other supported desktop environments, use:

```sh
./build/debug/bin/near_laugh
./build/debug/bin/level_editor
./build/debug/bin/level_editor ./resources/levels/prototype.level.json
```

Both launchers derive the resource root from the actual executable path, not
the working directory or `argv[0]`.

## Current Packaged Resources

The build copies the following layout beside `near_laugh`, `level_editor`, and
the relevant smoke/process executables:

```text
resources/
  levels/prototype.level.json
  models/prototype_chair.glb
  shaders/prototype_scene_vertex.spv
  shaders/prototype_scene_fragment.spv
  textures/prototype_floor.png
  textures/prototype_boundary.png
  textures/prototype_obstacle.png
```

The level uses format version 2. Its bounded profile contains one 97-by-97
heightfield, at most 240 solids, one spawn, exactly two point lights and one
ambient intensity, and one chair placement with a box proxy. The document has
no resource paths and is loaded once before physics and renderer construction.

The three textures form a fixed sRGB array in floor/boundary/obstacle order.
Terrain and solid faces use outward normals, world-scaled UVs, authored tints,
and the matching layer. The chair uses its authored UVs with the obstacle
layer. Two authored point lights and a camera-mounted flashlight illuminate the
scene.

## Current Game Controls

The prototype starts with the cursor captured:

- mouse: look;
- W/A/S/D: move relative to view;
- Left Shift: sprint;
- Left Control: crouch;
- Space: jump while grounded;
- Escape: release the cursor;
- left mouse button: toggle the flashlight while captured, or recapture the
  cursor while released.

A recapture press is suppressed until release so it does not also toggle the
flashlight. Movement uses fixed-step gravity and static collision, slides along
walls, traverses the authored low step, and checks standing clearance beneath
the low passage. These are current prototype behaviors, not permanent product
requirements.

## Current Editor Behavior

The standalone editor uses right mouse for scene navigation, Escape to release
navigation, mouse movement to look, W/A/S/D for horizontal movement,
Space/Left Control for vertical movement, and Left Shift to sprint. UI capture
suppresses conflicting camera input.

File > Open and File > Save As use explicit path-entry dialogs. Opening is
transactional; Save and Save As use the shared validated deterministic codec;
and dirty open, close, or exit requests require Save, Discard, or Cancel.

The current workspace displays terrain, solids, authored lighting, player
spawn, and chair placement as read-only data. Object placement and terrain
sculpting are separate active OpenSpec changes and are not implemented by the
editor foundation.

## Build Targets

- `near_laugh_platform`: GLFW windowing and physical input collection.
- `near_laugh_world`: version-2 level data, private JSON codec, validation, and
  immutable runtime handoff.
- `near_laugh_physics`: Jolt lifetime, static collision, and one virtual
  character.
- `near_laugh_render`: Vulkan renderer, resource loading, and immutable scene
  GPU ownership.
- `near_laugh_runtime`: application facade, composition, player input,
  player/flashlight policy, fixed-step coordination, and main loop.
- `near_laugh`: game launcher linking only `near_laugh_runtime`.
- `near_laugh_editor_core`: editor document workflow and camera.
- `near_laugh_editor_ui`: Dear ImGui integration and workspace.
- `near_laugh_editor_render`: editor Vulkan rendering and active-document GPU
  replacement.
- `level_editor`: standalone authoring application.

## Vulkan Smoke Validation

The deterministic `debug` preset excludes window/GPU-dependent tests. Run both
game and editor Vulkan smoke paths explicitly:

```sh
ctest --preset vulkan-smoke --output-on-failure
```

The smoke paths exercise normal rendering, resize, minimize/restore,
swapchain recovery, partial construction, and orderly shutdown. They verify
that immutable texture, lighting, and mesh resources survive recovery and that
error-severity Vulkan validation messages fail the test. A desktop session and
a Vulkan 1.3 presentation-capable device are required.

For visual inspection, confirm stable one-metre texture scale across face
orientations, outward-normal lighting, mip stability at distance, the distinct
three surface roles, the dark transition between authored light pools,
flashlight cone/range behavior, chair rendering and collision, and persistence
through resize/recovery.

## Completion Checks

Before reporting an implementation complete:

1. configure the affected preset;
2. build the affected targets;
3. run affected deterministic tests;
4. run Vulkan smoke validation when rendering, resources, windows, or lifetime
   behavior changed;
5. review `git diff` and current OpenSpec validation;
6. report any step the environment could not perform.

Compilation alone is not behavioral validation.
