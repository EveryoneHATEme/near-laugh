# Architecture

## Architectural Goal

The architecture should remain small enough to be understood
and maintained by one developer.

Prefer explicit dependencies and simple ownership over extensibility.

## High-Level Structure

fps_game
   │
   ▼
gameplay
   │
   ▼
engine
 ┌─┴──────────────────────────────┐
 │                                │
World   Renderer   Physics   Audio   Input
 │        │
 └──── Core / Platform ───────────┘

The executable-facing boundary is the PImpl-based
`near_laugh::Application` facade and `RuntimeConfig`. Its public headers contain
only standard-library types. The concrete build modules are:

- `near_laugh_runtime`: application composition, the main-thread loop, resource
  configuration, and fixed FPS input mapping;
- `near_laugh_platform`: GLFW lifetime, the window, event batches, and
  engine-owned physical keyboard/mouse state;
- `near_laugh_world`: the bounded version-1 level document and private JSON
  codec, field-aware validation, and the immutable terrain, solids, spawn,
  two world-space point lights, and static-prop placement shared with concrete
  consumers;
- `near_laugh_physics`: the concrete Jolt lifetime, static collision world,
  and one virtual character;
- `near_laugh_render`: Vulkan lifetime, presentation, explicit frame
  requests/outcomes, and renderer-private synchronous parsing of the one
  packaged static GLB through privately linked `cgltf`.

GLFW/Vulkan surface coupling is confined to one internal bridge. It is not a
rendering-backend abstraction.

 ## Dependency Rules

Game-specific code may depend on engine code.

Engine code must never depend on game-specific code.

Rendering must not depend on gameplay concepts such as Player,
Enemy, Weapon, or Health.

Physics must not depend on rendering.

Core must not depend on Rendering, Gameplay, Physics, or Audio.

Platform-specific APIs must remain behind the platform layer.

Vulkan types must not leak into gameplay code.

## Ownership

Ownership must be explicit.

Prefer stack ownership and RAII.

Use `std::unique_ptr` for exclusive dynamic ownership.

Use `std::shared_ptr` only when shared ownership is genuinely required
and the lifetime cannot be represented more simply.

Raw pointers and references are non-owning.

Do not introduce owning raw pointers.

Engine subsystem lifetimes are controlled by the Engine object.

GPU resource ownership is defined separately in RENDERING.md.

## Global State

Avoid mutable global state.

Global singletons are not the default architecture.

Subsystems should receive their required dependencies explicitly.

## Lifetime

The intended high-level lifetime is:

Application
  creates
Engine
  creates, in order
Platform
Window
RuntimeResources
PrototypeLevel
PhysicsWorld
PlayerController
PlayerFlashlight
Renderer

Shutdown happens in reverse dependency order.

Resource destruction must never depend on already-destroyed subsystems.

The runtime loop owns platform polling, close decisions, input sampling,
minimized-window waiting, bounded steady-clock sampling, prototype cursor
capture, fixed player simulation, interpolated first-person camera state, and
the decision to issue at most one frame request per iteration. A blocking wait
begins its own input batch, and the runtime maps that batch immediately after
the wait returns, before a later poll can reset its cursor delta. It also resets
both the clock origin and simulation accumulator across the wait so restoration
cannot create catch-up movement. The renderer consumes the current
framebuffer extent/resize state, a backend-neutral column-major camera matrix,
and at most one source-independent spot-light description, then reports
rendered, skipped, or recovered without controlling events, input, time,
camera state, light-source gameplay, or application lifetime. The runtime
exhaustively consumes each outcome before it continues the application-owned
loop.

Each complete fixed step advances player movement on the main thread. A
concrete Engine-owned `PlayerFlashlight` samples primary-action edges once per
event batch, independently of fixed simulation timing, and produces the one
generic `SpotLightFrame` from the same interpolated view pose used to build the
render camera. Its recapture suppression consumes an inactive press until the
button is released. The renderer receives only source-independent scalar light
data; it does not depend on player or flashlight types. The current frame
contract deliberately has one dynamic spot-light slot rather than a light
registry or arbitrary light collection.

Platform callbacks retain held physical keys/buttons and accumulate cursor
movement for one event batch. `FpsInputMapper` maps W/A/S/D, Space, Left Shift,
Left Control, Escape, and the left/right mouse buttons to the single-player FPS
action snapshot. Look delta resets when the next polling or blocking batch
begins, while held actions persist across both kinds of event dispatch.

The `fps` launcher uses a private, host-native helper to discover its actual
executable path and supplies the adjacent `resources` directory through
`RuntimeConfig`. The packaged prototype level `levels/prototype.level.json`,
the shaders, the four fixed textures
`prototype_floor.png`, `prototype_boundary.png`, `prototype_obstacle.png`, and
`prototype_shooting_target.png`, plus `models/prototype_chair.glb`, are resolved
beneath that explicit root.
Invocation text and the process working directory do not participate in
runtime layout discovery.

After the window exists, runtime composition resolves the fixed resource set,
strictly parses and validates `levels/prototype.level.json`, and only then
constructs physics, player, and renderer owners. Missing, malformed,
unsupported, or invalid level data therefore cannot reach a dependent
subsystem or the frame loop. Destruction remains the reverse of member order.

The version-1 document contains exactly one 97-by-97 heightfield, no more than
240 axis-aligned solids, one player spawn, exactly two point lights and one
ambient value, and one placement of the packaged chair with a box proxy. It
contains no resource paths. The private `nlohmann/json` codec rejects unknown
or missing fields and emits canonical locale-independent JSON. The editable
`LevelDocument` may hold invalid work, while saving and construction of an
immutable `PrototypeLevel` both use the same field-aware validation.

The current world is one immutable loaded `PrototypeLevel` containing tinted
axis-aligned solids, a player spawn, exactly two world-space point lights, a
near-black ambient scalar, three inert plate solids, and one fixed chair
placement with an obstacle surface role and an independently authored box
collision proxy. The chair description contains no path, parser, Vulkan, or
Jolt type. Each
solid independently carries exactly one fixed surface role: floor, boundary,
obstacle, or shooting target. Rendering expands each solid into the existing
UV/layer-bearing world triangle stream and synchronously flattens the validated
chair GLB into a separate world-space triangle stream. Physics creates matching
solid boxes plus the chair's authored proxy without reading model geometry.
The renderer validates and uploads the level point lights once; they do not
follow the camera or become mutable frame state. The chair and inert plates
carry no gameplay identity, health, damage, interaction, or feedback state.
The four roles are not a material system, the one model is not an asset
registry, the dynamic spot-light frame is not a registry, and the level is not
a scene hierarchy, asset pipeline, ECS, or generic level format. The running
game does not mutate, save, discover, or hot-reload level documents.

Renderer lifetime owns the sampled texture and immutable lighting resources
plus separate immutable generated-world and imported-chair vertex buffers after
the Vulkan context and before teardown. The format-dependent pipeline borrows
descriptor layouts and sets and owns no geometry. It binds one descriptor pair
and one scene push constant before deterministic world-then-chair draws. The
pipeline may be recreated without rebuilding or re-uploading textures,
lighting, or either mesh. It is destroyed before those dependent owners, and
all are released before the logical device.

## Threading

The initial engine is primarily single-threaded.

The main thread owns:

- application lifecycle
- gameplay update
- world mutation
- render submission

Jolt uses its library-provided temporary allocator and
`JobSystemSingleThreaded`; the initial physics world creates no worker pool
and requires no engine job system.

Background threads may later be introduced for clearly isolated work
such as asset loading.

Do not introduce:

- a job system
- task graphs
- parallel ECS updates
- asynchronous compute
- complicated lock-free structures

without a dedicated design change and measured justification.

## World Model

The project does not require a generic ECS architecture.

Initially prefer the simplest world representation that supports
the FPS requirements described in GAMEPLAY.md.

Composition is preferred where useful, but engine architecture
must not be reorganized around ECS terminology without a concrete need.

If entity scale or update performance becomes a measured problem,
an ECS or data-oriented representation may be proposed through
an OpenSpec change.
