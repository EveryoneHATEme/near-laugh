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
        `-> near_laugh_render -> near_laugh_platform, near_laugh_world,
                                Vulkan, GLFW, stb_image, cgltf

level_editor
  |-> near_laugh_editor_core -> near_laugh_platform, near_laugh_world
  |-> near_laugh_editor_ui -> near_laugh_editor_core, near_laugh_platform,
  |                           near_laugh_world, ImGui, GLFW
  |-> near_laugh_editor_render -> near_laugh_render, near_laugh_platform,
  |                               near_laugh_world, ImGui, Vulkan
  |-> near_laugh_platform
  `-> near_laugh_world
```

The concrete targets have these responsibilities:

- `near_laugh_platform` owns GLFW lifetime, windows, event batches, cursor
  capture, and project-owned physical keyboard and mouse state.
- `near_laugh_world` owns the bounded version-6 level document, exact version-2/3/4/5 read
  compatibility, strict private JSON codec, shared validation, and immutable
  level data. It privately links
  `nlohmann/json`.
- `near_laugh_physics` owns Jolt lifetime, static proxies, kinematic door leaves, and one
  virtual character. It consumes immutable world data and privately links
  Jolt.
- `near_laugh_render` owns Vulkan presentation and scene resources. It consumes
  immutable world data and uses the narrow internal GLFW/Vulkan surface bridge.
  Image decoding and the one bounded GLB loader remain renderer-private.
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

All target include and link relationships are declared in `CMakeLists.txt`.
The public runtime boundary is the PImpl-based `near_laugh::Application` and
`RuntimeConfig` under `include/near_laugh`; those headers expose only standard
library types. Other subsystem headers are repository-internal. Vulkan, GLFW,
Jolt, GLM, JSON, and ImGui types do not cross the public runtime boundary.

## Runtime Ownership and Flow

The internal `Engine` is the concrete runtime composition owner; its name does
not establish a reusable engine layer. It constructs, in dependency order:

```text
Platform -> Window -> RuntimeResources -> PrototypeLevel
         -> selected LevelEntry -> PhysicsWorld -> PlayerController -> PlayerFlashlight
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
light, enabled values for the two point-light slots, and up to 192 changing
opaque boxes. Boxes carry geometry and tint, not door IDs or action policy. Rendering returns
`Rendered`, `Skipped`, or `Recovered`; the runtime handles every outcome and
retains application-lifetime control. Rendering does
not interpret player actions, update simulation, poll events, or decide when
the game exits.

The player and physics advance on the main thread through a fixed-step
accumulator. Jolt uses its single-threaded job implementation; the project has
no runtime job system.

## World Boundary

The bounded v6 document contains optional 97-by-97 terrain, 1–240 axis-aligned
solids, 1–16 named entries/default, two point lights plus ambient, 0–128 static
model placements, one optional switch and 0–32 hinged door definitions. Terrain
and solids select a game-owned structural material ID independently of collision
kind. Each prop has its own ID, model ID, transform and 0–8 local collision boxes.
The finite catalog contains only the selected game models/materials; resource
paths and importer types remain outside world data.

Exact v2/v3/v4/v5 shapes normalize on read. The singleton chair becomes one
`prototype-chair` placement with its original transform/box/material; old surface
roles map to their legacy materials. v2/v3 spawn becomes the `default` entry;
v2 has no switch; v2–4 have no doors; v5 retains all authored doors. Explicit
saves write canonical v6; opening never rewrites a source file.

World validation checks finite derived geometry, references, entry support and
standing clearance, and each initial door leaf against all blocking geometry
and entries. A blocked later swing is valid. Safe gameplay-invalid documents
remain editable; runtime construction and saving require full validity. Physics
never reads render models: every prop body comes from its authored box list.

`AuthoredInteraction` owns release latches and nearest-target arbitration for
the concrete switch/door actions. `LightSwitchController` owns light enables;
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
the level file; durable entry/door/prop string IDs do.
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
The optional switch uses one reserved transient ID below the allocated solid
IDs and shares property commands, placement, selection, and history. Preview
uses its authored initial state; changing its link or removing it restores
the previously linked light. Successful resource replacement also installs
the preview light state; a failed replacement retains the prior preview.
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
The editor is a concrete tool for this game, not a runtime mode or general
scene-editor framework.

## Architectural Constraints

The current requirements do not justify an ECS, render graph, RHI, plugin
system, general scripting runtime, generic scene hierarchy, job system, or
asset registry. New boundaries or abstractions require a concrete game,
authoring, reliability, or measured performance need.
