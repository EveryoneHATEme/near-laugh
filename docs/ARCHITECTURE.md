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
- `near_laugh_world`: the immutable axis-aligned prototype solids, player
  spawn, and two validated world-space point lights shared with the concrete
  consumers that need them;
- `near_laugh_physics`: the concrete Jolt lifetime, static collision world,
  and one virtual character;
- `near_laugh_render`: Vulkan lifetime, presentation, and explicit frame
  requests/outcomes.

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
`RuntimeConfig`. The packaged prototype shaders and the four fixed textures
`prototype_floor.png`, `prototype_boundary.png`, `prototype_obstacle.png`, and
`prototype_shooting_target.png` are resolved beneath that explicit root.
Invocation text and the process working directory do not participate in
runtime layout discovery.

The current world is one immutable `PrototypeLevel` containing tinted
axis-aligned solids, a player spawn, exactly two world-space point lights, a
near-black ambient scalar, and three inert plate solids. Each
solid independently carries exactly one fixed surface role: floor, boundary,
obstacle, or shooting target. Rendering expands each solid into the existing
UV/layer-bearing triangle stream, while physics creates one matching static box
body. The renderer validates and uploads the level point lights once; they do
not follow the camera or become mutable frame state. The inert plates carry no
target descriptions, health, damage, or feedback state. The four roles are not
a material system, the dynamic spot-light frame is not a registry, and the
level is not a scene hierarchy, asset pipeline, ECS, or generic level format.

Renderer lifetime owns the sampled texture and immutable lighting resources
after the Vulkan context and before the format-dependent graphics pipeline.
Pipelines borrow their descriptor layouts and sets, are destroyed first, and
may be recreated without rebuilding either owner. Both owners are released
before the logical device.

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
