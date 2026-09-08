# Architecture

## Goal

near-laugh is a purpose-built runtime and authoring toolchain for one
single-player first-person narrative horror game. Its architecture favors
explicit ownership, small concrete modules, and direct data flow that one
developer can understand and debug.

The project does not separate a reusable engine from a game layer. The
`near_laugh_*` targets are focused parts of this game's runtime and tools.

## Build Modules and Dependencies

```text
near_laugh
  `-> near_laugh_runtime
        |-> near_laugh_platform -> GLFW
        |-> near_laugh_world -> nlohmann/json
        |-> near_laugh_physics -> near_laugh_world, Jolt
        |-> near_laugh_audio -> near_laugh_world, near_laugh_text, miniaudio
        |-> near_laugh_animation -> near_laugh_cgltf, GLM
        `-> near_laugh_render -> near_laugh_platform, near_laugh_world,
                                near_laugh_text, near_laugh_animation,
                                Vulkan, GLFW, stb_image, near_laugh_cgltf

level_editor
  |-> near_laugh_editor_core -> near_laugh_platform, near_laugh_world,
  |                             near_laugh_animation
  |-> near_laugh_editor_ui -> near_laugh_editor_core, near_laugh_platform,
  |                           near_laugh_world, near_laugh_text, ImGui, GLFW
  |-> near_laugh_editor_render -> near_laugh_render, near_laugh_platform,
  |                               near_laugh_world, ImGui, Vulkan
  |-> near_laugh_platform
  |-> near_laugh_audio
  `-> near_laugh_world

character_animation_viewer
  `-> near_laugh_animation, near_laugh_render, near_laugh_platform,
      near_laugh_world, near_laugh_text
```

The concrete targets have these responsibilities:

- `near_laugh_platform` owns GLFW lifetime, windows, event batches, cursor
  capture, and project-owned physical keyboard and mouse state.
- `near_laugh_world` owns the bounded version-8 level document, exact version-2/3/4/5/6/7 read
  compatibility, strict private JSON codec, shared validation, and immutable
  level data. It privately links
  `nlohmann/json`.
- `near_laugh_physics` owns Jolt lifetime, static proxies, kinematic door leaves, and one
  virtual character. It consumes immutable world data and privately links
  Jolt.
- `near_laugh_render` owns Vulkan presentation and scene resources. It consumes
  immutable world data and uses the narrow internal GLFW/Vulkan surface bridge.
  Image decoding and the bounded static GLB profile remain renderer-private.
- `near_laugh_animation` owns the separate prepared skeletal GLB profile,
  immutable CPU assets, local TR sampling, short transitions and deformation.
  Runtime, viewer and editor link this CPU target; GLM and cgltf stay private.
  Static and animated loading share one compiled cgltf implementation owner.
- `near_laugh_audio` owns selected PCM/caption preparation, miniaudio playback,
  authored room transmission and the concrete cue coordinator. Backend types
  stay private. Device-free rendering runs the same mixer as device playback.
- `near_laugh_text` owns bounded UTF-8 decoding, trusted Noto Sans validation,
  atlas baking with the pinned stb dependency, and pure caption layout.
- `near_laugh_runtime` owns application composition, player input mapping,
  fixed-step player/door policy, interaction arbitration, flashlight and light state, frame interpolation,
  and the main-thread loop.
- `near_laugh` is the game launcher. It discovers its native executable path,
  supplies the adjacent resource root and optional level/entry selection, and
  links only `near_laugh_runtime`.
- `near_laugh_editor_core` owns editable document workflow, saved-file play
  preparation, native game-process ownership, and the free-fly editor camera.
- `near_laugh_editor_ui` owns Dear ImGui and the editor workspace.
- `near_laugh_editor_render` owns editor Vulkan presentation, active-document
  scene resources, and the ImGui Vulkan backend.
- `level_editor` composes the editor modules without linking
  `near_laugh_runtime` or `near_laugh_physics`.
- `character_animation_viewer` composes explicit animation inspection and
  measurement without the runtime, physics or audio coordinator.

All target include and link relationships are declared in `CMakeLists.txt`.
The public runtime boundary is the PImpl-based `near_laugh::Application` and
`RuntimeConfig` under `include/near_laugh`; those headers expose only standard
library types. Other subsystem headers are repository-internal. Vulkan, GLFW,
Jolt, GLM, JSON, miniaudio and ImGui types do not cross the public runtime boundary.

## Runtime Ownership and Flow

The internal `Engine` is the concrete runtime composition owner; its name does
not establish a reusable engine layer. It constructs, in dependency order:

```text
Platform -> Window -> RuntimeResources -> PrototypeLevel
         -> selected LevelEntry -> CaptionFont -> prepared audio/CueCoordinator
         -> PhysicsWorld -> PlayerController -> PlayerFlashlight
         -> LightSwitchController -> DoorController -> AuthoredInteraction -> Renderer
```

RAII destruction reverses that order. Raw pointers and references are
non-owning; exclusive dynamic Vulkan owners use `std::unique_ptr`. Mutable
global subsystem ownership is not used.

Startup resolves shaders and the selected level's packaged model/material
dependencies beneath the executable-relative resource root. Unselected models
and the raw source pack are not required. The level defaults to
`levels/prototype.level.json`; `--level` selects another file and `--entry`
selects a named start. Relative level arguments resolve once against the
launcher's working directory. Only the selected level must exist. All entries
are validated before the selected pose reaches physics, the player, or the
renderer. Selection leaves the immutable authored default and order intact.

The main-thread loop owns event processing, close and minimize decisions,
player-input sampling, elapsed-time accumulation, fixed simulation steps,
cursor-capture transitions, camera interpolation, flashlight and switch
updates, and the decision to request a frame. Blocking event waits form and sample their own
input batch before another poll can clear relative mouse movement; timing is
reset after the wait.

The renderer receives immutable level data at construction and a
backend-neutral `FrameRequest` at runtime. A request contains framebuffer
state, a column-major camera matrix, at most one source-independent spot
light, an exact-size borrowed span of point-light enables, up to 192 changing
opaque boxes, borrowed resolved foreground/ambience captions, and the exact
selected set of zero through four character poses. Boxes carry
geometry and tint, not door IDs or action policy. Rendering returns
`Rendered`, `Skipped`, or `Recovered`; the runtime handles every outcome and
retains application-lifetime control. Rendering does
not interpret player actions, update simulation, poll events, or decide when
the game exits.

The player and physics advance on the main thread through a fixed-step
accumulator. Jolt uses its single-threaded job implementation; the project has
no runtime job system.

## Audio ownership and timing

Selected audio is decoded once through native filesystem paths: PCM16 mono or
stereo at 48 kHz, at most 120 seconds per clip and 128 MiB aggregate decoded
memory. Spatial and foreground clips are mono. Caption tracks are bounded UTF-8
with finite ordered, non-overlapping intervals inside the clip duration. The
trusted font validates glyph coverage and minimum-size fit before playback.
These checks require no output device.

The concrete playback owner retains prepared buffers, stable voices and its
private miniaudio engine. Device callbacks perform mixing and atomic loss
notification only; they do not access the level/editor, load files or trigger
cue transitions. Shutdown stops and joins device processing before releasing
voices, decoded buffers and the engine. Missing selected content is a startup
error; device initialization/loss enters reported silent mode without replay.

The coordinator owns immutable authored definitions and run-local instances.
Its injected monotonic clock advances by uncapped active time independently of
fixed simulation steps, and freezes during suspension. Hardware cursors are
corrected on resume or drift above 100 ms; repeated failure continues the same
cue timeline silently. Rendering borrows resolved caption strings synchronously.
Listener pose is the same interpolated eye used for the frame; door connection
gains use accepted angles. Half-open room boxes distinguish regions, strongest
connection-path products model transmission, and gains smooth over 50 ms.

## World Boundary

The bounded v8 document contains optional 97-by-97 terrain, 1–240 axis-aligned
solids, 1–16 named entries/default, 0–8 point lights plus ambient, 0–128 static
model placements, 0–16 switches and 0–32 hinged door definitions. Terrain
and solids select a game-owned structural material ID independently of collision
kind. Each prop has its own ID, model ID, transform and 0–8 local collision boxes.
The finite catalog contains only the selected game models/materials; resource
paths and importer types remain outside world data.

Audio records add up to 128 cues, 64 sources, 32 non-overlapping room boxes and
64 connections. Clip/caption identities come from the finite game catalog;
connections reference rooms, outside, and optionally a door. Audio metadata
validation performs no device, file decoding or GPU construction.

Exact v2–v7 shapes normalize on read. The singleton chair becomes one
`prototype-chair` placement with its original transform/box/material; old surface
roles map to their legacy materials. v2/v3 spawn becomes the `default` entry;
v2 has no switch; v2–4 have no doors; v5 retains all authored doors. Explicit
saves write canonical v8; opening never rewrites a source file. Versions 2–6
normalize to empty audio. Legacy light slots become `point-light-0/1`, both
unshadowed; the optional switch becomes `light-switch-0` and moves its initial
enable to the linked light. A missing switch leaves both lights on. v7 audio
survives unchanged. Older executables cannot read v8; use Save As to
retain an original needed by an older build.

World validation checks finite derived geometry, references, entry support and
standing clearance, and each initial door leaf against all blocking geometry
and entries. A blocked later swing is valid. Safe gameplay-invalid documents
remain editable; runtime construction and saving require full validity. Physics
never reads render models: every prop body comes from its authored box list.

Light IDs and switch IDs are unique within their collections. Switches link
by light ID; multiple plates can operate one source. Initial enables and
shadow flags belong to lights, independently of switch presence. Ambient is
authored in [0, 0.20]. Four configured sources may cast shadows, including
disabled ones; their radii are limited to [0.25, 20] metres.

`AuthoredInteraction` owns release latches and nearest-target arbitration for
the concrete switch/door actions. It finds the true minimum distance before
applying the 0.1 mm tie tolerance, then chooses doors before switches and
durable IDs within a type. `LightSwitchController` resolves links once and
owns run-local enables in immutable authored light order;
`DoorController` owns door intent, accepted angle, lock and short feedback state.
Physics privately owns zero-velocity kinematic leaves and continuous conservative
angular clearance queries. Each fixed step moves the player against installed
leaves, retains a swept player envelope including character skin/stance, then
advances doors in durable-ID order and installs accepted poses. Rendering and
targeting use those poses without separate door interpolation. No event queue,
entity registry, physical hinge simulation or general interaction framework is
introduced.

The renderer decodes each selected GLB/material once per scene, expands fixed
placements into immutable material batches and uploads changing generated boxes
only after their frame-slot fence. The existing 128-byte camera/light push range
does not grow. See RENDERING for material/profile and resource ownership.

The explicit `character_animation_viewer` selects the prepared mannequin without
changing level v8 or interpreting a filename as animation policy. Its caller owns
independent playback and supplies up to four render-instance handles, skeleton
identities, model-relative joint palettes and world translation/yaw. Frame spans
are borrowed synchronously. The renderer validates the complete selected set,
deforms source vertices once into separate fenced character buffers and shares
immutable indices/materials per selected asset. Indexed color and shadow draws
use the same slot geometry with per-instance vertex offsets. It never advances clips.
The finite catalog carries measured contact/interaction phases for subsequent
scripted-character work; actor records, routes and authoring remain P07b/P07c.

## Editor Ownership

The standalone editor constructs Vulkan diagnostics, `Platform`, `Window`, the
GLFW/ImGui callback bridge, `EditorDocument`, and `EditorRenderer`. Shutdown
reverses that order so ImGui backends are released before their Vulkan and GLFW
dependencies.

The editor loop owns event polling, minimized waits, camera timing, UI capture,
and render outcomes. `EditorDocument` loads candidates transactionally and
tracks its resolved path, diagnostics, dirty state, and pending save/discard/
cancel decision. A failed load or save preserves the active document and dirty
state.

`EditorDocument` also owns transient object IDs, one selection, concrete object
commands, and 128-entry undo/redo history. These transient handles never enter
the level file; durable entry/door/prop/light/switch string IDs do.
Terrain gestures share that history as sorted sparse sample before/after pairs.
Brush settings and the active path are editor-only state. Pure brush kernels
read pre-stamp samples; the editor resamples horizontal motion at fixed distance
and updates samples immediately while validating at stroke completion.
Saved-state identity uses document revisions; a separate preview generation
changes on edits, undo/redo, and document replacement. Property widgets edit a
draft and commit once when editing ends. CPU rays pick solid/door geometry, catalog model bounds (including decorative
props), and light/entry markers; placement uses the nearest visible structural face or
present terrain triangle, excluding the moved solid. An unsuitable nearest
face blocks placement. Terrain-only mode remains available for terrain files.

The editor renderer draws structurally safe document fields, then selection
overlays and Dear ImGui in the same Dynamic Rendering pass. Full gameplay
validation gates saving, playtesting, and runtime construction, allowing an invalid entry
placement to remain visible during repair. Replacement GPU resources are built
temporarily and swapped in only after success. Failure retains the usable
prior resources and explicitly labels a preview stale relative to its document.
Terrain diagnostics also carry sample or cell coordinates and triangle identity
for the validation panel and red viewport outlines. Terrain preview changes
coalesce once per editor frame and replace the world buffer after both frame
fences complete, preserving prop/material, initial-door, lighting and pipeline owners.
Swapchain recovery preserves the document, UI state, texture owner, and camera.
Every light and switch has a distinct allocated transient selection ID and
shares property commands, placement and history. Light renames update all
incoming links atomically; deletion retains broken references for repair.
Preview uses light initial values independently of links. Successful resource
replacement installs the scene, lighting, shadows and matching enable vector
together with its selected character resources; a failed replacement retains
the prior coherent preview and its matching character-pose contract.
New Interior creates a valid floor, default entry and lights, with no terrain,
props or doors. Durable entry strings are separate from transient selection IDs;
renaming a default entry updates its reference in one undoable command.

Play prepares the current document and selected entry, completes pending edits,
and requires Save and Play or Cancel when dirty. Unsaved work uses Save As.
After saving, or for a clean document, the editor rereads and validates the
file and compares normalized content before consuming one launch request.
External mismatches require an explicit Save or Open. The editor application
also decodes all selected assets before creating a process; asset failures
launch nothing. A native process owner
launches the sibling game with literal Unicode-capable arguments, monitors at
most one child without blocking normal authoring, and reports exit status.
Creating a process is distinct from successful game startup. Editor shutdown
releases the handle or detaches the POSIX reaping obligation without killing
or waiting for the game. No readiness protocol or hot reload is introduced;
the child independently validates its file after the ordinary filesystem race
between preflight and load.

Concrete audio commands use the same history and saved revision rules. Cue,
room and door renames update affected audio references in one step; deletion
leaves incoming references visible as validation errors. A new duplicate keeps
outgoing references and gets a fresh durable ID. Source placement uses authored
surface offsets; room wire edges are selectable independently of their empty
interior. Neither adds collision objects.

The editor application owns explicit snapshot audition, using its camera and
authored initial door angles. Edits, undo/redo, replacement, minimization and
Play stop it without restart. UI controls change only audition state. Saved-file
Play also preflights selected audio, captions and the current trusted font;
audio-device availability never blocks launch. Diagnostics outlive the editor's
Vulkan resources so smoke checks include final destruction.

The editor is a concrete tool for this game, not a runtime mode or general
scene-editor framework.

## Architectural Constraints

The current requirements do not justify an ECS, render graph, RHI, plugin
system, general scripting runtime, generic scene hierarchy, job system, or
asset registry. New boundaries or abstractions require a concrete game,
authoring, reliability, or measured performance need.
