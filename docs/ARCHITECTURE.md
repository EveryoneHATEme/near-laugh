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
  creates
Platform
Renderer
Physics
Audio
World

Shutdown happens in reverse dependency order.

Resource destruction must never depend on already-destroyed subsystems.

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