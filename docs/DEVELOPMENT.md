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
.\build\debug\bin\near_laugh.exe --level .\resources\levels\apartment-stairs.level.json --entry lower-landing
.\build\debug\bin\level_editor.exe
.\build\debug\bin\level_editor.exe .\resources\levels\prototype.level.json
```

On other supported desktop environments, use:

```sh
./build/debug/bin/near_laugh
./build/debug/bin/near_laugh --level ./resources/levels/apartment-stairs.level.json --entry lower-landing
./build/debug/bin/level_editor
./build/debug/bin/level_editor ./resources/levels/prototype.level.json
```

Both launchers derive the resource root from the actual executable path, not
the working directory or `argv[0]`.
The game accepts `--level <path>` and `--entry <id>` independently. Defaults
are the packaged prototype and the selected level's authored default entry.
Relative level arguments resolve against the invoking directory once; quote
paths containing spaces. Unknown, repeated, missing, and empty options fail
before application startup. Native Unicode paths are preserved. Other assets
always come from the executable's resource root.

## Current Packaged Resources

The build copies the following layout beside `near_laugh`, `level_editor`, and
the relevant smoke/process executables:

```text
resources/
  levels/prototype.level.json
  levels/apartment-stairs.level.json
  models/prototype_chair.glb
  shaders/prototype_scene_vertex.spv
  shaders/prototype_scene_fragment.spv
  textures/prototype_floor.png
  textures/prototype_boundary.png
  textures/prototype_obstacle.png
```

Levels use format version 4. The bounded profile contains an optional 97-by-97
heightfield, 1–240 solids, 1–16 named entries and a default entry ID, exactly
two point lights and one ambient intensity, one chair placement with a box
proxy, and zero or one light switch. `light_switch` is null or an object with
`position`, `yaw_degrees`, `point_light_index` (0/1), and `initially_on` (boolean).
Exact previous version-2/3 shapes load without rewriting the source. The old
spawn becomes the `default` entry; version 2 has no switch, while version 3
retains its nullable switch. Explicit saves always write version 4. Version 1
and unknown versions remain unsupported. The document has no resource paths
and is loaded and validated once before physics and renderer construction.
Every entry must have solid-top or terrain support at its authored height and
standing clearance, including entries not chosen for the current launch.

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
- E: toggle the switch's linked light while looking at its plate within
  2 metres and with an unobstructed view;
- Escape: release the cursor;
- left mouse button: toggle the flashlight while captured, or recapture the
  cursor while released.

A recapture press is suppressed until release so it does not also toggle the
flashlight. Movement uses fixed-step gravity and static collision, slides along
walls, traverses the authored low step, and checks standing clearance beneath
the low passage. These are current prototype behaviors, not permanent product
requirements.

The packaged switch is the pale plate on the central obstacle facing spawn.
Walk forward from spawn to reach it. It controls Point light 1, initially on,
and adds no collision body. Static collision, including the chair's box proxy,
blocks interaction. E requires a release before the first press and between
presses; holding it through a miss, cursor transition, or minimization cannot
trigger a later toggle. The light state persists through presentation recovery
and resets on restart without modifying the level file. Flashlight controls
and ambient remain independent.

## Current Editor Behavior

The standalone editor uses right mouse for scene navigation, Escape to release
navigation, mouse movement to look, W/A/S/D for horizontal movement,
Space/Left Control for vertical movement, and Left Shift to sprint. UI capture
suppresses conflicting camera input.

File > Open and File > Save As use explicit path-entry dialogs. Opening is
transactional; Save and Save As use the shared validated deterministic codec;
and dirty New, Open, Close, or Exit requests require Save, Discard, or Cancel.
File > New Interior creates a valid starter floor, default entry, two lights,
ambient 0.12, and the packaged chair without terrain or a switch. It begins
dirty and unsaved; Save uses Save As until a path has been chosen. Legacy files
open clean with a migration notice. Safely decoded gameplay-invalid files
remain editable with diagnostics; malformed or unsafe files preserve the
current document.

The Objects panel and left-click viewport picking share one selection. Solids
can be added, duplicated, deleted, and edited. Entries can also be added,
duplicated, renamed, and moved. IDs match `[a-z][a-z0-9-]{0,63}`; new entries use
the first unused `entry-N`. Make default changes the authored startup entry.
Renaming it updates the reference in one undoable edit. Choose another default
before deleting the default entry; the last entry cannot be deleted. The two
point lights and single packaged chair can be selected and edited but cannot
be added, duplicated, or removed. Ambient and terrain layout remain read-only.

**Add light switch** creates the optional singleton near the spawn at standing
interaction height. Select **Light switch** in Objects or click its plate in
the viewport. Delete removes it; duplication is unavailable. Properties expose
Position, Yaw, Linked light (Point light 1/2), and Initially on. Exact numeric
placement is available alongside surface mounting. All switch edits share
undo/redo, validation, and dirty state. Switches and the chair may lie outside
terrain bounds. Preview follows the initial state, including link changes,
removal, sculpting, and recovery. The editor does not run E interaction.

The Properties panel edits position, dimensions, tint, solid kind/surface,
entry ID and yaw, light color/intensity/radius, and chair translation/yaw/uniform
scale plus its box proxy. Drag numeric values, or Ctrl-click a numeric field
to type an exact value. Release or finish the field to commit one undo entry.
Invalid numeric commits retain the previous value and report the field error.
Changing a solid kind selects its matching surface; a manually changed surface
must match the kind before saving.

Enable **Place on surface** with an object selected. **Scene surfaces** uses
the nearest structural face or terrain triangle and displays the target,
face, elevation, and normal. It excludes the moved solid. **Terrain only**
retains terrain placement and is unavailable when terrain is absent.
On top surfaces, solids rest their bottom at the hit, entries place their
feet, and the chair places its translation. Lights and switches use the visible
height offset (initially 2 m and 1.4 m, or the previous terrain offset).
Vertical faces support solids, lights with a visible outward offset, and
switches with their back 1 mm outside the wall and front aligned outward.
Entries and the chair cannot be wall-mounted. Undersides block placement;
the editor never searches through an unsuitable nearer face. Escape, a miss,
UI capture, or navigation cancels/suppresses placement without an edit.
Yellow bounds identify selected geometry; sphere markers identify lights and
entries. Editor overlays remain visible through scene geometry.

Editor shortcuts (suppressed during camera navigation, active field editing,
or modal dialogs):

- Ctrl+Z: undo; Ctrl+Y or Ctrl+Shift+Z: redo.
- Ctrl+D: duplicate a solid at a horizontal offset or an entry at the same pose.
- Delete: remove the selected solid, removable entry, or switch.
- Ctrl+S: save the current valid document.

History retains up to 128 committed edits and clears on document replacement.
Undoing back to the saved state clears dirty state; editing after undo drops
the redo branch. Finite edits that violate level constraints, such as entry
overlap, remain visible and editable with validation diagnostics. Save is
disabled until correction or undo restores validity. Unsaved-close dialogs
still offer Discard and Cancel when the document is invalid.

Under Properties > Terrain, enable **Sculpt terrain** and select Raise, Lower,
or Smooth. Left-drag the scene to apply the brush; Escape returns to object
selection. Object placement and sculpting are mutually exclusive. UI controls
and camera navigation suppress brush input. The yellow outer ring shows the
radius; inner rings show falloff strength and follow the heightfield. Arcs
outside the terrain are omitted.

- Radius: 0.5 to 8 metres.
- Raise/lower strength: 0.01 to 1 metre per stamp.
- Smooth strength: 0 to 1; zero leaves the terrain and history unchanged.
- Falloff: 0 to 1, blending constant interior influence toward smoothstep
  attenuation. Samples exactly on the radius remain unchanged.

Drag controls or Ctrl-click to type values. Non-finite or out-of-range typed
commits retain the previous valid value and report the field. A stroke captures
its settings on press and stamps at 0.25-metre intervals along the observed X/Z
pointer path. Holding still adds no stamps. A terrain miss breaks the path;
returning to terrain starts a new segment within the same gesture. Smoothing
uses a pre-stamp 3-by-3 neighborhood with [1, 2, 1] weights in each axis and
clamped border coordinates.

Each modifying gesture shares the 128-entry history with object edits. Undo
restores the whole stroke, selection, and brush settings. Mesh preview updates
during the stroke, and full validation runs when it ends or is undone/redone.
Entering UI, starting navigation, minimizing, or requesting close ends the
current stroke. Invalid slope triangles appear in red; Validation lists their
zero-based cell X/Z and triangle 1 or 2. Repair with lower/smooth strokes or
undo. Terrain strokes revalidate all entries. If a stroke leaves an entry
unsupported, place it back on a suitable surface or correct its numeric pose.
Clear upper-floor entries remain supported by their authored structural floors.
Terrain tools and footprints clear when switching to an interior.

Brushes edit only the fixed 97-by-97 height samples. They do not change layout,
surface roles, textures, or runtime collision and cannot author holes, caves,
overhangs, voxel terrain, paint, procedural terrain, or erosion.

In **Playtest**, choose **Start entry** and **Play**. The selection is editor
state and does not change the authored default or dirty state. Play finishes
pending edits and validates all entries. Dirty work requires **Save and Play**
or **Cancel**; unsaved work then uses Save As. A fresh read must match the
prepared editor document before launch. If the disk file changed externally,
explicitly Save or Open it and try again. Errors and canceled dialogs launch
nothing and leave no deferred request.

The editor starts the sibling game with the saved absolute path and chosen
entry. One game child can run at a time. Authoring remains available; process
creation and eventual exit status appear in Playtest. Closing the editor
leaves the game running independently. Saved edits do not change that run.
Keep authored files at their own paths; playing them does not require changing
the packaged prototype. Shared resource copying runs once per build for the
executables in `build/<preset>/bin`.

The M1 blockout has upper floors at Y=3 and the lower landing at Y=0. From
`apartment`, leave Lena's room through the east doorway into the corridor;
the kitchen is east of the corridor at Z=-2.3. Follow the corridor north to
the rear stairs at Z=-6 and descend to Z=-16.2. `lower-landing` faces back
upstairs. Fourteen 0.6 m treads give fifteen 0.2 m rises in a 1.8 m-wide stair.
This route uses ordinary walking. No doors, narrative events, or checkpoints
are delivered by this authoring change.

## Build Targets

- `near_laugh_platform`: GLFW windowing and physical input collection.
- `near_laugh_world`: version-4 level data with version-2/3 read compatibility,
  private JSON codec, validation, and immutable runtime handoff.
- `near_laugh_physics`: Jolt lifetime, static collision, and one virtual
  character.
- `near_laugh_render`: Vulkan renderer, resource loading, and immutable scene
  GPU ownership.
- `near_laugh_runtime`: application facade, composition, player input,
  player/flashlight policy, fixed-step coordination, and main loop.
- `near_laugh`: game launcher linking only `near_laugh_runtime`.
- `near_laugh_editor_core`: document workflow, play preparation, native child
  ownership, and camera.
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
entry preview and refused saving, canceled close, light/prop edits, and
semantic save/reload using a temporary level copy. Deterministic UI tests drive
real ImGui button, shortcut, capture, and numeric-drag behavior without a GPU.
Terrain smoke coverage includes active multi-stamp strokes, coalesced buffer
replacement, smoothing, undo/redo, sculpted save/reload, and resize/minimize
recovery followed by an unsaved exit decision.
Interior smoke covers both named starts, runtime selected-entry failures,
terrain/interior replacement, an empty world mesh, and failed replacement
followed by rendering with the prior resources. Deterministic tests exercise
the saved-file Play transaction and a real native child argument probe.

Light-switch coverage includes all point-light/spotlight enable combinations,
editor add/remove and link changes, initial-state preview, and terrain rebuilds.
After shader changes, regenerate and validate both packaged stages:

```sh
glslc -fshader-stage=vert --target-env=vulkan1.3 resources/shaders/prototype_scene_vertex.glsl -o resources/shaders/prototype_scene_vertex.spv
glslc -fshader-stage=frag --target-env=vulkan1.3 resources/shaders/prototype_scene_fragment.glsl -o resources/shaders/prototype_scene_fragment.spv
spirv-val --target-env vulkan1.3 resources/shaders/prototype_scene_vertex.spv
spirv-val --target-env vulkan1.3 resources/shaders/prototype_scene_fragment.spv
```

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
