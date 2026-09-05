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
- `near_laugh_world` owns the bounded version-2 level document, strict private
  JSON codec, shared validation, and immutable level data. It privately links
  `nlohmann/json`.
- `near_laugh_physics` owns Jolt lifetime, the static collision world, and one
  virtual character. It consumes immutable world data and privately links
  Jolt.
- `near_laugh_render` owns Vulkan presentation and scene resources. It consumes
  immutable world data and uses the narrow internal GLFW/Vulkan surface bridge.
  Image decoding and the one bounded GLB loader remain renderer-private.
- `near_laugh_runtime` owns application composition, player input mapping,
  fixed-step player policy, flashlight state, frame interpolation, and the
  main-thread loop.
- `near_laugh` is the game launcher. It discovers its native executable path,
  supplies the adjacent resource root, and links only `near_laugh_runtime`.
- `near_laugh_editor_core` owns editable document workflow and the free-fly
  editor camera.
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
         -> PhysicsWorld -> PlayerController -> PlayerFlashlight -> Renderer
```

RAII destruction reverses that order. Raw pointers and references are
non-owning; exclusive dynamic Vulkan owners use `std::unique_ptr`. Mutable
global subsystem ownership is not used.

Startup resolves shaders, the floor/boundary/obstacle textures, the chair GLB,
and `levels/prototype.level.json` beneath the executable-relative resource
root. The version-2 document is parsed and validated before physics or renderer
construction. Neither the process working directory nor level-authored paths
participate in resource discovery.

The main-thread loop owns event processing, close and minimize decisions,
player-input sampling, elapsed-time accumulation, fixed simulation steps,
cursor-capture transitions, camera interpolation, flashlight updates, and the
decision to request a frame. Blocking event waits form and sample their own
input batch before another poll can clear relative mouse movement; timing is
reset after the wait.

The renderer receives immutable level data at construction and a
backend-neutral `FrameRequest` at runtime. A request contains framebuffer
state, a column-major camera matrix, and at most one source-independent spot
light. Rendering returns `Rendered`, `Skipped`, or `Recovered`; the runtime
handles every outcome and retains application-lifetime control. Rendering does
not interpret player actions, update simulation, poll events, or decide when
the game exits.

The player and physics advance on the main thread through a fixed-step
accumulator. Jolt uses its single-threaded job implementation; the project has
no runtime job system.

## World Boundary

The bounded level document contains one 97-by-97 heightfield, at most 240
axis-aligned solids, one player spawn, exactly two point lights and an ambient
intensity, and one packaged chair placement with an authored box collision
proxy. Solids use the fixed floor, boundary, or obstacle surface roles. The
document contains no resource paths.

The editable `LevelDocument` may temporarily contain invalid data. Saving and
construction of an immutable `PrototypeLevel` both use the same field-aware
validation. The running game loads once and does not mutate, save, discover, or
hot-reload level documents.

Rendering expands terrain and solids into an immutable world triangle stream
and flattens the validated chair GLB into a separate immutable stream. Physics
builds matching terrain and solid collision plus the chair's authored box
proxy; it does not derive collision from renderer geometry. Shared level data
contains no Vulkan, Jolt, parser, or filesystem types.

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
commands, and 128-entry undo/redo history. IDs never enter the level file.
Saved-state identity uses document revisions; a separate preview generation
changes on edits, undo/redo, and document replacement. Property widgets edit a
draft and commit once when editing ends. CPU rays pick authored boxes and
light/spawn markers; placement intersects the exact heightfield triangles.

The editor renderer draws structurally safe document fields, then selection
overlays and Dear ImGui in the same Dynamic Rendering pass. Full gameplay
validation gates saving and runtime construction, allowing an invalid spawn
placement to remain visible during repair. Replacement GPU resources are built temporarily
and swapped in only after success. Swapchain recovery preserves the document,
UI state, texture owner, and camera. The editor is a concrete tool for this
game, not a runtime mode or general scene-editor framework.

## Architectural Constraints

The current requirements do not justify an ECS, render graph, RHI, plugin
system, general scripting runtime, generic scene hierarchy, job system, or
asset registry. New boundaries or abstractions require a concrete game,
authoring, reliability, or measured performance need.
