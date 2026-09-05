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

The Objects panel and left-click viewport picking share one selection. Solids
can be added, duplicated, deleted, and edited. The single spawn, two point
lights, and single packaged chair can be selected and edited but cannot be
added, duplicated, or removed. Terrain and ambient intensity remain read-only.

The Properties panel edits position, dimensions, tint, solid kind/surface,
spawn yaw, light color/intensity/radius, and chair translation/yaw/uniform
scale plus its box proxy. Drag numeric values, or Ctrl-click a numeric field
to type an exact value. Release or finish the field to commit one undo entry.
Invalid numeric commits retain the previous value and report the field error.
Changing a solid kind selects its matching surface; a manually changed surface
must match the kind before saving.

Enable **Place on terrain** with an object selected, then left-click the
terrain at the yellow marker. Solids rest their bottom at the hit, the spawn
places its feet there, and the chair places its translation there. Lights
retain their height above the previous terrain anchor. Other properties stay
unchanged. Escape cancels placement; a terrain miss has no effect. Yellow
bounds identify the selected solid or chair proxy; sphere markers identify
lights and spawn. These editor overlays remain visible through scene geometry.

Editor shortcuts (suppressed during camera navigation, active field editing,
or modal dialogs):

- Ctrl+Z: undo; Ctrl+Y or Ctrl+Shift+Z: redo.
- Ctrl+D: duplicate the selected solid at a visible horizontal offset.
- Delete: remove the selected solid.
- Ctrl+S: save the current valid document.

History retains up to 128 committed edits and clears on document replacement.
Undoing back to the saved state clears dirty state; editing after undo drops
the redo branch. Finite edits that violate level constraints, such as spawn
overlap, remain visible and editable with validation diagnostics. Save is
disabled until correction or undo restores validity. Unsaved-close dialogs
still offer Discard and Cancel when the document is invalid.

To use an authored level in the game, save it explicitly to the game's
executable-adjacent `resources/levels/prototype.level.json` and restart the
game. Saving elsewhere does not change the packaged level. Builds that copy
source resources may replace that executable-adjacent file; preserve authored
work separately or deliberately update the source asset. Terrain sculpting
remains the separate `add-terrain-sculpting` change.

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

The editor smoke also exercises object duplication/removal, undo, invalid
spawn preview and refused saving, canceled close, light/prop edits, and
semantic save/reload using a temporary level copy. Deterministic UI tests drive
real ImGui button, shortcut, capture, and numeric-drag behavior without a GPU.

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
