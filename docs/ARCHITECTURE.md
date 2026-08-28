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
Renderer

Physics, Audio, and World will be added only when their FPS requirements are
implemented.

Shutdown happens in reverse dependency order.

Resource destruction must never depend on already-destroyed subsystems.

The runtime loop owns platform polling, close decisions, input sampling,
minimized-window waiting, and the decision to issue at most one frame request
per iteration. A blocking wait begins its own input batch, and the runtime maps
that batch immediately after the wait returns, before a later poll can reset
its cursor delta. The renderer consumes the current framebuffer extent/resize
state and reports rendered, skipped, or recovered without controlling events or
application lifetime. The runtime exhaustively consumes each outcome before it
continues the application-owned loop.

Platform callbacks retain held physical keys/buttons and accumulate cursor
movement for one event batch. `FpsInputMapper` maps W/A/S/D, Space, Left Shift,
Left Control, Escape, and the left/right mouse buttons to the single-player FPS
action snapshot. Look delta resets when the next polling or blocking batch
begins, while held actions persist across both kinds of event dispatch.

The `fps` launcher uses a private, host-native helper to discover its actual
executable path and supplies the adjacent `resources` directory through
`RuntimeConfig`. Invocation text and the process working directory do not
participate in runtime layout discovery.

## Threading

The initial engine is primarily single-threaded.

The main thread owns:

- application lifecycle
- gameplay update
- world mutation
- render submission

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
