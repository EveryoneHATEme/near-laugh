# Project Vision

## Purpose

This project is a purpose-built C++/Vulkan runtime for developing a
single-player first-person narrative horror game.

The target experience is grounded, authored, and atmosphere-driven.
Exploration, interaction, environmental storytelling, lighting, audio,
pacing, and scripted events are more important architectural assumptions
than combat.

The project is not intended to become a general-purpose game engine.

The runtime is developed by a single developer.
Architecture and tooling should therefore optimize for simplicity,
debuggability, fast iteration, and low maintenance cost.

## Target Game

The runtime is designed around a game with the following assumptions:

* first-person perspective
* exactly one local human-controlled player
* keyboard and mouse as the primary input devices
* real-time 3D rendering
* authored environments
* grounded player movement and collision
* interactive world objects
* scripted and triggered events
* environmental storytelling
* atmosphere driven by lighting, darkness, and spatial audio
* level loading
* persistent gameplay state where required
* save/load support

These assumptions are allowed to influence runtime architecture.

Combat is not a baseline assumption.

Weapons, ammunition, projectiles, enemy health, damage systems,
combat AI, and similar shooter-specific concepts must not influence
architecture unless the game acquires a concrete requirement for them.

The game may contain characters, threats, chase sequences, hostile actors,
or other sources of danger without requiring a conventional combat model.

## Authored Content

The game is expected to rely heavily on intentionally authored spaces,
events, interactions, lighting, and audio.

The runtime may therefore prefer concrete game-specific representations
over generic scene, entity, scripting, or content frameworks.

A feature does not need to support arbitrary game genres or arbitrary
content structures if a smaller representation serves this game better.

The current prototype level is stored as a versioned packaged JSON asset.
Startup parses and validates it before handing runtime data to the systems
that require it.

Standalone authoring tools may create or modify game assets, but the game
runtime itself is not an editor.

Runtime editing, arbitrary asset paths, a general-purpose scene format,
and hot reload are not requirements unless a concrete production need
justifies them.

## Primary Goals

The runtime should:

* make first-person horror gameplay straightforward to implement
* support rapid iteration on authored levels and events
* make interactions and scripted sequences easy to reason about
* support atmosphere-critical systems such as lighting and audio cleanly
* provide predictable frame times and responsive input
* expose Vulkan and runtime errors clearly during development
* keep ownership and lifetime rules explicit
* minimize hidden global state
* remain understandable by one developer
* support automated behavioral tests where practical
* keep content-production workflows proportional to the needs of the game

## Design Priorities

When several technically valid solutions exist, prefer the one that:

1. solves the current game requirement directly;
2. introduces fewer concepts;
3. has explicit ownership and data flow;
4. is easier to debug;
5. is easier for one developer to modify later;
6. makes authored game content easier to build or iterate on.

Engine purity, theoretical extensibility, and reuse by hypothetical future
projects are not priorities.

## Non-Goals

The runtime does NOT aim to support:

* arbitrary game genres
* multiplayer or networking
* split-screen multiplayer
* mobile platforms
* consoles
* VR or AR
* visual scripting
* a plugin ecosystem
* an Unreal/Unity-style general-purpose editor
* user-created mods
* a general-purpose runtime scripting platform
* ray tracing as an architectural requirement
* multiple rendering backends
* DirectX, Metal, or OpenGL backends
* a generic ECS framework
* a generic asset pipeline for arbitrary applications
* procedural open-world generation
* MMO-scale entity counts

These are not forbidden forever, but they must not shape current
architecture without a concrete game requirement.

## Scope Rule

A feature belongs in the project only if at least one of these is true:

1. the game currently requires it;
2. it significantly simplifies development or content production for the game;
3. it solves an existing technical limitation;
4. measured evidence shows that it is required for performance or reliability.

"Another game may need it later" is not sufficient justification.

"An engine normally has this" is not sufficient justification.

"A more generic abstraction would be cleaner" is not sufficient
justification by itself.

## Initial Technical Direction

Language: C++

Graphics API: Vulkan

Build system: CMake

Primary target: desktop PC

The project should prefer modern Vulkan APIs and a simple explicit
rendering architecture over compatibility with old Vulkan implementations.

External libraries should remain behind meaningful subsystem boundaries
where practical, but wrappers must not be introduced merely to hide a
dependency.

## Performance Philosophy

Performance work should be driven by measurement.

Do not introduce complicated data structures, multithreading,
GPU-driven rendering, asynchronous compute, custom allocators,
streaming systems, or similar infrastructure without evidence that
they solve an existing problem.

Stable and predictable performance matters more than pursuing technically
interesting optimizations.

## Evolution

The architecture is expected to evolve as the actual game exposes new
requirements.

Existing architecture is not assumed to be correct merely because it is
documented or tested.

When real gameplay requirements conflict with an earlier architectural
assumption, revisit the assumption instead of forcing the game through an
obsolete abstraction.
