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
  levels/audio-captions.level.json
  audio/*.wav
  captions/*.captions
  fonts/NotoSans-Regular.ttf
  fonts/OFL.txt
  characters/test-mannequin.glb
  characters/catalog.json
  characters/LICENSE-Quaternius.txt
  models/prototype_chair.glb
  shaders/prototype_scene_vertex.spv
  shaders/prototype_scene_fragment.spv
  shaders/point_shadow_vertex.spv
  shaders/point_shadow_fragment.spv
  shaders/caption_vertex.spv
  shaders/caption_fragment.spv
  textures/prototype_floor.png
  textures/prototype_boundary.png
  textures/prototype_obstacle.png
```

Levels write format version 9 and read exact versions 2–8 without modifying
the source. The profile contains optional 97-by-97 terrain, 1–240 solids,
1–16 entries/default, 0–8 lights/ambient, 0–128 props, 0–16 switches,
and 0–32 doors, plus audio (up to 128 cues, 64 sources, 32 rooms and 64 connections).
The required `characters` object contains arrays of up to four actors, 32 marks
and 16 routes. Earlier packaged scenes retain empty character arrays; the
one/four-actor scripted-character fixtures exercise authored initial routes.
The editor renders initial idle poses and preserves these records through
unrelated edits. Dedicated character authoring remains P07c.
Prop/model/material and clip/caption IDs are logical names, never paths.
Props have finite translation/yaw, positive uniform scale and 0–8 local boxes.
Legacy chair/texture roles normalize to explicit legacy identities; v5 doors
survive migration. Versions 2–6 map to empty audio. New fields in old versions,
unknown fields and v1 fail. Legacy lights map to `point-light-0/1` without
shadows, and the singleton switch maps to `light-switch-0`; its initial
enable moves to the linked light. v7 audio is preserved. Exact v8 inputs preserve
all prior values and normalize to empty characters, as do v2–v7 after their
existing migrations. Versions 2–8 reject character fields. Older builds cannot
read v9: use Save As or retain
the original before conversion when it is still needed by an older build.

The selected apartment derivatives are `models/apartment_chair.glb`,
`apartment_table.glb`, `apartment_phone.glb`, `apartment_radio.glb`, plus
`textures/apartment_wood_floor.png` and `apartment_wallpaper.png`.
See [asset preparation/provenance](../resources/models/APARTMENT_ASSETS.md).
The raw `house_interior_pack/` is local source material, excluded from Git by
the root `.gitignore` and not copied by builds. A checkout needs only the
prepared files in `resources/`; the full pack is needed only for regeneration.
Only models/materials referenced by the chosen level are loaded.

Structural materials are independent of collision kind; terrain has one
whole-surface material. Generated geometry keeps world-scaled UVs, while props
keep authored UVs. Apartment textures use nearest sampling; legacy textures
retain linear sampling. Phone cord alpha uses MASK cutoff 0.5; radio is OPAQUE.
All surviving fragments use the bounded authored point lights/flashlight. There is
no PBR, emission, blended transparency or texture-paint workflow.

## Current Game Controls

The prototype starts with the cursor captured:

- mouse: look;
- W/A/S/D: move relative to view;
- Left Shift: sprint;
- Left Control: crouch;
- Space: jump while grounded;
- E: operate the nearest switch or door within 2 metres;
- R: lock/unlock a fully closed, stationary door from its authored bolt side;
- right mouse: knock once on the nearest door;
- Escape: release the cursor;
- left mouse button: toggle the flashlight while captured, or recapture the
  cursor while released.

A recapture press is suppressed until release so it does not also toggle the
flashlight. Movement uses fixed-step gravity and static/accepted door collision, slides along
walls, traverses the authored low step, and checks standing clearance beneath
the low passage. These are current prototype behaviors, not permanent product
requirements.

The packaged switch is the pale plate on the central obstacle facing spawn.
Walk forward from spawn to reach it. It controls Point light 1, initially on,
and adds no collision body. Terrain, solids, all prop boxes, door leaves and actor proxies block interaction. E/R/knock require a release before the first press and between
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
File > New Interior creates a valid starter floor, default entry, two lights
and ambient 0.12, with no terrain, props, doors or switch. It begins
dirty and unsaved; Save uses Save As until a path has been chosen. Legacy files
open clean with a migration notice. Safely decoded gameplay-invalid files
remain editable with diagnostics; malformed or unsafe files preserve the
current document.

The Objects panel and left-click viewport picking share one selection. Solids
can be added, duplicated, deleted, and edited. Entries can also be added,
duplicated, renamed, and moved. IDs match `[a-z][a-z0-9-]{0,63}`; new entries use
the first unused `entry-N`. Make default changes the authored startup entry.
Renaming it updates the reference in one undoable edit. Choose another default
before deleting the default entry; the last entry cannot be deleted. Lights
and switches support add/duplicate/delete with durable IDs and limits of 8/16.
Props and doors support independent add/duplicate/delete, durable
IDs, finite property edits and undo/redo. Removing the last prop is valid.
Changing a prop model preserves its boxes; Reset model collision boxes is
explicit. Yellow render bounds and cyan proxy bounds distinguish appearance
from collision. Missing model references retain red selectable markers.
Ambient is editable in [0, 0.20], including zero. Terrain layout remains
read-only; terrain material is separate.

**Add point light** creates an enabled, unshadowed source. Light properties
include ID, position, RGB, intensity, radius, Initially on and Casts shadows.
The summary shows the four-caster budget, which includes disabled sources.
Duplicating a caster may exceed that budget; the repairable document remains
editable, with Save/Play blocked and a clearly stale coherent preview.
Renaming a light updates every incoming switch link in one undo step.
Deleting it keeps broken links visible for repair.

**Add light switch** creates a plate near the first entry at standing
interaction height, linked to the first light, or an explicit broken link in
a lightless document. Select a plate in Objects or the viewport. Properties
expose ID, Position, Yaw and Linked light by durable ID. Duplication preserves
the link. Selected lights show ranges/incoming links; selected plates show
their outgoing link. Numeric edits and surface mounting share undo/redo and
dirty state. Preview uses light initial values, independent of relinking or
removing switches. The editor does not run E interaction.

Properties expose solid geometry/tint/kind/material, entry ID/pose, lights,
prop identity/model/transform/box list, and door ID, bottom hinge, closed yaw,
leaf dimensions, opening angle/speed, lock side and initial state. Drag numeric
values or Ctrl-click to type. Finishing a field commits one undo step. Unsafe
fields retain their old values; safe cross-field errors remain repairable and
block Save/Play. Door overlays show hinge, opening arc and lock side.

Enable **Place on surface** with an object selected. **Scene surfaces** uses
the nearest structural face or terrain triangle and displays the target,
face, elevation, and normal. It excludes the moved solid. **Terrain only**
retains terrain placement and is unavailable when terrain is absent.
On top surfaces, solids rest their bottom at the hit, entries place their
feet, and props place their translation anchor. Doors place their bottom hinge
with the visible floor-clearance offset (default 0.02 m). Lights and switches use the visible
height offset (initially 2 m and 1.4 m, or the previous terrain offset).
Vertical faces support solids, lights with a visible outward offset, and
switches with their back 1 mm outside the wall and front aligned outward.
Entries, props and doors cannot be wall-mounted. Undersides block placement;
the editor never searches through an unsuitable nearer face. Escape, a miss,
UI capture, or navigation cancels/suppresses placement without an edit.
Yellow bounds identify selected geometry; sphere markers identify lights and
entries. Editor overlays remain visible through scene geometry.

Editor shortcuts (suppressed during camera navigation, active field editing,
or modal dialogs):

- Ctrl+Z: undo; Ctrl+Y or Ctrl+Shift+Z: redo.
- Ctrl+D: duplicate a solid, door or prop at an offset, an entry at the same
  pose, or an audio record with a new durable ID.
- Delete: remove the selected solid, removable entry, switch, prop, door or
  audio record.
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
material assignments, props/doors, or runtime collision and cannot author holes, caves,
overhangs, voxel terrain, paint, procedural terrain, or erosion.

In **Playtest**, choose **Start entry** and **Play**. The selection is editor
state and does not change the authored default or dirty state. Play finishes
pending edits and validates all entries. Dirty work requires **Save and Play**
or **Cancel**; unsaved work then uses Save As. A fresh read must match the
prepared editor document before launch. Required selected assets are decoded
before creating the child; a missing/unsupported asset launches nothing. If the disk file changed externally,
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
Open Lena's room door with E before crossing, using the doorway near Z=3.5.
The hinge is (-1.12, 3.02, 3.07), width 1.26 m, closed yaw -90 degrees and
opening angle -90 degrees. Roughly 4 cm jamb clearance accommodates the
conservative angular query; the leaf opens west into the room. Positive local Z
puts its bolt on the room side. E reverses mid-swing; blocked movement stops
until another E press. Refusal/knock geometry lasts 0.3 seconds, without sound.

The temporary switch post is at (-0.65, 3.7, 4.05); its plate is
(-0.721, 4.3, 4.05). Open the door from the apartment start before approaching
the switch-view position: standing in the swing arc correctly stops its motion.
From room feet around (-2.3, 3, 4.05), aim down slightly
toward the plate: the closed leaf blocks it, and opening permits interaction.
The kitchen table is at (2.6, 3, -4); phone and radio sit on top. Cross the
kitchen doorway at Z=-2.3, then turn toward Z=-1.9 to pass the nearer chair.
Both authored starts support the full ordinary-walking route after opening.
These are temporary acceptance positions, not narrative/progression content.

## P04 audio and caption fixture

Run `build/debug/bin/audio_captions_fixture.exe` from any working directory.
Add `--silent` to exercise the same sequence without opening an output device.
The executable explicitly selects the packaged `audio-captions.level.json`:
radio and localized ring, moving corridor footsteps, a complete Russian phone
call, two seconds of silence, then the contradictory invitation behind the door.
The ordinary game does not activate this sequence from the level filename.

Fixture-only controls (while the cursor is captured): F5 cancels all instances
and restarts, M toggles mute, P suspends/resumes world and cue time together. Restart retains mute
and pause settings. Existing E/R door controls and camera navigation remain
available; Escape releases the cursor without pausing audio. Minimize suspends
before waiting, and restore preserves cue offsets. There is no save-game or
P05 narrative state in this fixture.

In the editor, use **Audio authoring** to add/list cues, sources, rooms and
connections; **Properties** edits the selected record. Source placement uses
**Place on surface**, height above the floor and wall offset. Room wireframes
are selectable on their edges. Selecting a connection draws its room/door links.
Broken references remain visible for repair and block Save/Play. Rename and
reference changes share one undo step; deletion does not cascade.

Select a source and use **Audio audition → Start audition**. Stop, Mute and Pause
operate on a validated snapshot; camera movement updates the listener without
dirtying the file. The panel shows source/listener regions, effective gain,
Russian captions and device warnings. Any edit, undo/redo, replacement, minimize
or Play request stops audition. Restore and selection do not restart it.

The shared packaging target copies `audio/`, `captions/`, `fonts/` and compiled
caption shaders beside game, editor, demo and smoke binaries. See
[audio preparation, hashes and provenance](../resources/audio/README.md) and
[trusted font provenance/license](../resources/fonts/README.md). Regenerate the
fixture level with `python scripts/prepare_audio_level.py`; audio generation has
separate explicit eSpeak NG/FFmpeg requirements documented with the assets.

Recompile and validate caption shaders after editing them:

```sh
glslc -fshader-stage=vert --target-env=vulkan1.3 resources/shaders/caption_vertex.glsl -o resources/shaders/caption_vertex.spv
glslc -fshader-stage=frag --target-env=vulkan1.3 resources/shaders/caption_fragment.glsl -o resources/shaders/caption_fragment.spv
spirv-val --target-env vulkan1.3 resources/shaders/caption_vertex.spv
spirv-val --target-env vulkan1.3 resources/shaders/caption_fragment.spv
```

Deterministic tests render real offline PCM and run the complete sequence in
audible, muted and silent modes. Vulkan smoke adds changing captions, empty
frames, attachment-format changes, atlas retention, allocation failures,
runtime minimize/restore/close, and editor audition/failed Play preflight.
Both validation sinks remain alive through final GPU destruction.

For listening acceptance, use headphones/speakers and record output hardware,
distinct source positions, listener rotation, moving footsteps, closed/open/
obstructed-door gain, ducking, pause/minimize/resume, shutdown, Russian speech
intelligibility and the completed-call contradiction. Measure output latency
and caption/audio drift against 100 ms plus measured device latency, stating the
measurement method. Device-free cursor tests are not that measurement. Repeat
muted and `--silent`, inspect captions at 800x600 through 3840x2160 and HiDPI,
and author/save/audition/Play from another working directory. Record observations
and unavailable checks in the [P04 validation record](../openspec/changes/archive/2026-09-07-add-spatial-audio-and-captions/validation.md).

## Build Targets

- `near_laugh_platform`: GLFW windowing and physical input collection.
- `near_laugh_world`: version-9 level data with exact version-2/3/4/5/6/7/8 read compatibility,
  private JSON codec, validation, and immutable runtime handoff.
- `near_laugh_physics`: Jolt lifetime, static proxies, accepted kinematic doors,
  up to four catalog actor capsules, and one virtual player character. The
  fixed-step caller advances the shared world before participant movement.
- `near_laugh_render`: Vulkan renderer, resource loading, and immutable scene
  GPU ownership.
- `near_laugh_runtime`: application facade, composition, player input,
  player/flashlight policy, fixed-step coordination, and main loop.
- `near_laugh_audio`: bounded PCM/caption preparation, miniaudio playback,
  authored room/door transmission, cue coordination and compiled P04 fixture.
- `near_laugh_text`: trusted font validation, atlas baking and caption layout.
- `near_laugh_animation`: bounded animated GLB decoding, deterministic TR
  sampling/transitions and CPU deformation; one compiled cgltf owner is shared
  with the existing static loader.
- `character_animation_viewer`: explicit P07a animation inspection and timing.
- `scripted_characters`: P07b route development controls through the runtime.
- `scripted_character_measure`: opt-in Release route comparison and short
  Debug/Release timing-recovery checks.
- `audio_captions_fixture`: explicit P04 demo using the internal runtime entry.
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
followed by rendering with the prior resources. Furnished-scene smoke changes
door open/lock previews, duplicates/deletes/restores shared-model placements,
and exercises changing geometry through fenced frame slots and recovery. Deterministic tests exercise
the saved-file Play transaction and a real native child argument probe.

Light-switch coverage includes all point-light/spotlight enable combinations,
editor add/remove and link changes, initial-state preview, and terrain rebuilds.
After scene/shadow shader changes, regenerate and validate the changed stages:

```sh
glslc -fshader-stage=vert --target-env=vulkan1.3 resources/shaders/prototype_scene_vertex.glsl -o resources/shaders/prototype_scene_vertex.spv
glslc -O -fshader-stage=frag --target-env=vulkan1.3 --target-spv=spv1.5 resources/shaders/prototype_scene_fragment.glsl -o resources/shaders/prototype_scene_fragment.spv
glslc -fshader-stage=vert --target-env=vulkan1.3 resources/shaders/point_shadow_vertex.glsl -o resources/shaders/point_shadow_vertex.spv
glslc -fshader-stage=frag --target-env=vulkan1.3 --target-spv=spv1.5 resources/shaders/point_shadow_fragment.glsl -o resources/shaders/point_shadow_fragment.spv
spirv-val --target-env vulkan1.3 resources/shaders/prototype_scene_vertex.spv
spirv-val --target-env vulkan1.3 resources/shaders/prototype_scene_fragment.spv
spirv-val --target-env vulkan1.3 resources/shaders/point_shadow_vertex.spv
spirv-val --target-env vulkan1.3 resources/shaders/point_shadow_fragment.spv
```

Both material fragment stages deliberately target SPIR-V 1.5: GLSL `discard` lowers to
`OpKill` without requiring the optional `shaderDemoteToHelperInvocation`
device feature. Keep the Vulkan 1.3 runtime feature baseline unchanged.

For visual inspection, confirm stable one-metre texture scale across face
orientations, outward-normal lighting, mip stability at distance, independent structural materials, the phone cord cutout, the dark transition between authored light pools,
flashlight cone/range behavior, chair rendering and collision, and persistence
through resize/recovery.

## Completion Checks

The opt-in P10 timing executable is excluded from the default build. Build
`interior_lighting_measure` explicitly in a separate Clang/Ninja Release tree
for performance measurements; use the Debug target's `check` mode for Vulkan
validation of query readback and recovery. It requires an available primary
monitor mode of 1920x1080 at 60 Hz and switches its own window to fullscreen.
Closing the window or interrupting its framebuffer invalidates a measurement.

```powershell
cmake --build build/p10-release --target interior_lighting_measure
.\build\p10-release\bin\interior_lighting_measure.exe resources/levels/apartment-stairs.level.json stationary build/baseline.csv
python scripts/summarize_lighting_timings.py build/baseline.csv
.\scripts\measure_interior_lighting.ps1 -OutputDirectory build/p10-final-measurements
```

The output path must be new. Each `stationary` or `route` invocation records
10 seconds warm-up and 60 seconds sampling and closes automatically. `route`
drives the furnished interior's ordinary walking, both doors, switch and
flashlight actions; it is a measurement fixture, not gameplay inferred from a
filename. `check` records 40 frames with explicit swapchain recovery and is not
a performance run. CSV writing occurs after GPU teardown and retains all raw
rows; summaries use nearest-rank percentiles after warm-up. Retained evidence
may use lossless `.csv.gz` files, which the summarizer also reads. Blank GPU fields
mean unavailable timing. See the current
[P10 validation record](../openspec/changes/archive/2026-09-07-add-interior-lighting/validation.md)
for hardware, timing scopes, exact commands, results and acceptance status.

The batch script runs three stationary and three route samples for each
packaged six/eight-light scene, plus one equivalent unshadowed stationary and
route baseline for each. It writes baseline copies with only `casts_shadows`
disabled and retains per-run CSV/logs and `summary.json`. Run no concurrent
game/editor, build or GPU tests during performance sampling. Run this GUI
measurement on the ordinary interactive Windows desktop. An agent's isolated
execution environment can change presentation pacing; use its approved desktop
execution mode and retain failed diagnostic runs separately. The local T1
gates are CPU/GPU p95 <=16.67 ms and frame p50/p95/p99 <=16.9/20/33.4 ms in
every run. Preserve failures and inspect separated waits and action costs.

`interior_lighting_visual` is a separate opt-in GPU readback target. It checks
fixed 1920x1080 views, actual player-obstructed/reversed door poses, independent
flashlight state and matching editor/runtime initial values. Lossless PNGs
retain actual stored framebuffer RGB; they are not performance measurements.
The optional isolated GLB controls verify texture alpha, factor/cutoff and
both-sided OPAQUE/MASK shadow coverage without collision proxies.

```powershell
cmake --build --preset debug --target interior_lighting_visual
python scripts/prepare_lighting_material_fixture.py build/material-controls
.\build\debug\bin\interior_lighting_visual.exe build/lighting-captures build/material-controls
python scripts/analyze_lighting_captures.py build/lighting-captures --prune-ppm
```

Both output directories must be fresh. The analyzer can rerun directly from
its PNG files. `--prune-ppm` removes redundant PPMs only after verifying an
exact RGB round trip. D16 fallback validation uses the existing test control
`NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE=shadow_d32_unavailable`; clear it before
ordinary runs. Reproduce the packaged lighting scenes with
`python scripts/prepare_interior_lighting.py`; the historical level migration
and audio preparation scripts also emit deterministic v9 data. Run
`python scripts/level_characters_v9.py` for explicit packaged v8 migration;
retained compatibility fixtures under `tests/fixtures/levels` stay unchanged.

Before reporting an implementation complete:

1. configure the affected preset;
2. build the affected targets;
3. run affected deterministic tests;
4. run Vulkan smoke validation when rendering, resources, windows, or lifetime
   behavior changed;
5. review `git diff` and current OpenSpec validation;
6. report any step the environment could not perform.

Compilation alone is not behavioral validation.

## P07a character animation

Run `build/debug/bin/character_animation_viewer.exe` from any working directory.
F5 cycles the first mannequin through idle/walk/interact; additional instances
use independent clips and offsets. P pauses, A/D seeks by 0.1 s and pauses,
Space restarts, M switches one/four instances, E changes the fixed inspection
view, R toggles the spotlight and Escape closes. Clip/time/status and controls
appear on screen. Minimize freezes playback without accumulating waited time.
The viewer owns no gameplay or audio coordinator and ordinary level filenames
activate no viewer sequence.

`--preflight` validates/deforms all three prepared clips without creating a
window. The process tests also run a copied executable containing only the
prepared character resources, from a different working directory.

```powershell
cmake --preset debug
cmake --build --preset debug --target character_animation_viewer character_animation_smoke
.\build\debug\bin\character_animation_viewer.exe --preflight
.\build\debug\bin\character_animation_smoke.exe build/character-captures
python scripts/retain_character_captures.py build/character-captures build/character-captures-png
.\scripts\check_character_viewer.ps1 -OutputDirectory build/character-viewer-controls
ctest --preset vulkan-smoke --output-on-failure
cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/p10-release --target character_animation_viewer
.\build\p10-release\bin\character_animation_viewer.exe --check 4 build/character-timing-check.csv
.\scripts\measure_character_animation.ps1 -OutputDirectory build/character-timings
```

Capture/timing output paths must be fresh. The smoke executable records bind,
clip and interrupted-transition poses, off-screen shadows and editor retention,
and checks injected allocation/upload failures and recovery with validation
alive through teardown. The retention script verifies exact stored RGB after
converting PPM readbacks to PNG, and preserves hashes and readback results.
The Windows desktop control script sends actual viewer keys and retains
screenshots/logs. `--check` records 40 frames with requested swapchain recovery
and drains timing queries through teardown; its short CSV is a functional check.
It also saves the rendered measurement view after recovery beside the CSV with
the same stem and a `.ppm` extension; both paths must be fresh. Inspect this
readback to verify character placement and visibility in the fullscreen scene.
Use the Debug viewer with this mode to enable Vulkan validation, then recheck
the final Release build before sampling. Capture waits are excluded from
performance runs; `--measure` requests no framebuffer readback.

Release measurement uses the packaged eight-light/four-caster interior with
the same fixed camera and initial doors for zero, one and four characters.
The script records three runs per setup, each with 10 s warm-up and 60 s
sampling at fullscreen 1920x1080/60 Hz. It preserves all nine raw CSV/log pairs
and writes `summary.json`. Raw CSV separates active CPU, character deformation/upload,
GPU whole-frame/shadows and presentation/fence waits. Run it on the ordinary
interactive desktop without concurrent builds or GPU work. CPU/GPU p95 must
meet 16.67 ms and frame p50/p95/p99 must meet 16.9/20/33.4 ms in every run;
retain failed gates and unavailable GPU fields. Existing T1 gates remain
unchanged; this P07a fixture alone does not establish full T2.
See [source preparation](../resources/characters/README.md) and the
[P07a validation record](../openspec/changes/archive/2026-09-08-add-character-animation/validation.md).

## P07b scripted characters

Run `build/debug/bin/scripted_characters.exe`, optionally with `--four` or
`--silent`. Ordinary `near_laugh --level resources/levels/scripted-characters.level.json
--entry view` also starts the authored route; the filename selects no
special gameplay. The walker turns at the corner, climbs three 20 cm rises,
waits at the closed door, and performs the final interaction after the player
opens it with E. The four-actor file adds an independent east route and opposing
north/south actors that intentionally wait for each other.

Development controls while the cursor is captured: F5 explicitly restarts each
initial route from current accepted placement, F6 cancels, P suspends/resumes
player/door/actor/animation/cue time together, and M mutes. F5 retains pause/mute
settings. A new process restores authored placements. Escape releases the
cursor and continues world/audio time. Minimized waits discard suspended time
and held control edges; restore neither catches up nor restarts actions.

The editor shows frozen initial idle poses, preserves character data through
unrelated edits/undo/save, and preflights selected mannequin and actor-linked
audio before Play. Invalid references have red diagnostic markers; missing
initial marks use the default entry as their marker anchor. An orphan route
uses its first surviving mark, falling back to the default entry. Dedicated lists,
placement and snapshot preview are P07c.

```powershell
python -B scripts/prepare_scripted_characters.py
python -B scripts/prepare_scripted_character_audio.py
cmake --build --preset debug --target engine_tests scripted_characters scripted_character_measure level_editor vulkan_smoke -j 4
ctest --preset debug --output-on-failure
ctest --preset vulkan-smoke --output-on-failure
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-debug-check -Check -DebugBuild
cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/p10-release --target scripted_character_measure -j 4
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-release-check -Check
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-release-timings
```

Use fresh output directories. The measurement keeps P07a's eight lights/four
casters, initial doors, camera, fullscreen 1920x1080/60 Hz FIFO and disabled
flashlight. It uses collision-valid short lanes beside the room chair, with
explicit alternating out/back requests and final interactions. Geometry,
entries and lighting remain identical for zero/one/four actors. Audio runs the
real offline mixer at 48 kHz; captions are excluded from timing presentation
to preserve the P07a color workload. This does not measure device callbacks.
The script activates the measurement window; startup focus notifications are
drained before timing. Any interruption during sampling invalidates the run.

Three runs per count retain 10 seconds warmup and 60 seconds sampling each,
with the unchanged P07a gates and no concurrent builds/tests/GPU work. Added
CSV scopes separate route decisions, actor collision, pose/contact processing,
world/player/doors and audio handoff/mixing from deformation/upload and GPU
costs. Only callers supplying timing storage perform these clock reads.
`-Check` instead captures 40 frames and a lossless measurement view, including
recovery; these rows are functional evidence, never performance samples.

The runtime Vulkan smoke retains route/door/action/caption readbacks beneath
its working directory's `build/scripted-character-runtime-captures`. Contact
tests retain a simulation/handoff/offline-onset CSV under `build/`. Neither
proves physical output latency or listening quality. See the
[P07b validation record](../openspec/changes/archive/2026-09-08-add-scripted-characters/validation.md)
for retained runs, observations and unavailable evidence. T2 remains pending
P07c's second scene authored and played with the editor.
