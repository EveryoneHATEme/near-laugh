# near-laugh

A purpose-built C++/Vulkan runtime for a single-player first-person horror game.

The project targets a grounded, narrative-driven horror experience focused on exploration, atmosphere, environmental storytelling, interaction, and scripted events rather than combat.

It is developed for one specific game and is intentionally **not** a general-purpose game engine.

## Project direction

The target game is a single-player first-person horror experience built around:

* exploration of authored environments
* environmental storytelling
* atmosphere, lighting, and spatial audio
* interactive objects and doors
* scripted and triggered events
* grounded first-person movement
* player-driven pacing and tension
* level loading and save/load support

Combat is not a core architectural assumption.

Features should be introduced only when required by the game rather than generalized for hypothetical future projects.

## Current state

The current prototype provides the technical foundation for the game:

* Vulkan 1.3 rendering
* GLFW platform and input layer
* Jolt Physics integration
* fixed-step player simulation
* first-person character controller
* collision-constrained movement
* crouching, sprinting, and jumping
* authored level loading from packaged JSON
* static world geometry and props
* point lights
* player-controlled flashlight
* depth-tested textured rendering
* deterministic tests for core simulation and physics behavior
* standalone level editing with object placement, terrain sculpting, property
  controls, undo/redo, and validation-gated saving

The prototype level is primarily a technical test environment and does not represent the intended final game.

## Architecture

The runtime is deliberately small and explicit.

Major responsibilities are separated into focused modules:

* `near_laugh_platform` — windowing and physical input
* `near_laugh_world` — level data, validation, and authored world state
* `near_laugh_physics` — player and world collision through Jolt Physics
* `near_laugh_render` — Vulkan rendering
* `near_laugh_runtime` — application composition and game loop

External libraries are kept behind module boundaries where practical.

The architecture favors:

* simple code over generic systems
* explicit ownership and lifetime
* RAII
* small APIs
* game requirements over engine purity
* measured optimization over speculative complexity

Generic engine infrastructure such as an ECS, render graph, RHI, plugin system, scripting runtime, or job system is not a goal unless a concrete game requirement justifies it.

## Controls

Current prototype controls:

* **Mouse** — look
* **W/A/S/D** — move
* **Left Shift** — sprint
* **Left Control** — crouch
* **Space** — jump
* **Left mouse button** — toggle flashlight while the cursor is captured
* **Escape** — release the cursor

Controls and movement behavior are still prototype-level and may change as the game develops.

## Build

Requirements:

* CMake
* Ninja
* Clang
* Vulkan 1.3 SDK

Configure, build, and run tests:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

For a fresh configuration:

```sh
cmake --preset debug --fresh
```

See `docs/DEVELOPMENT.md` for additional development and validation commands.

## Documentation

* `docs/VISION.md` — product direction and project constraints
* `docs/GAMEPLAY.md` — gameplay assumptions the runtime may rely on
* `docs/ARCHITECTURE.md` — architectural boundaries and ownership
* `docs/RENDERING.md` — rendering design
* `docs/DEVELOPMENT.md` — build, test, and validation workflow

## Philosophy

near-laugh exists to support one game.

New systems should solve an existing gameplay, content-production, or technical requirement. Features should not be generalized merely because another type of game might need them later.
