# Project Vision

## Purpose

This project is a purpose-built game engine/runtime for developing
a single-player first-person shooter.

It is not intended to become a general-purpose game engine.

The engine is developed by a single developer.
Architecture and tooling should therefore optimize for simplicity,
debuggability, fast iteration, and low maintenance cost.

## Target Game

The engine is designed around a game with the following assumptions:

- first-person perspective
- single local player
- keyboard and mouse input
- real-time 3D rendering
- static and dynamic level geometry
- hitscan and projectile weapons
- AI-controlled enemies
- collision and rigid-body physics
- spatial audio
- level loading
- save/load support

These assumptions are allowed to influence engine architecture.

The current prototype level is a versioned packaged JSON asset with deliberately
fixed FPS limits. Startup parses and validates it once, then hands one immutable
value to physics and rendering. Runtime editing, saving, hot reload, arbitrary
asset paths, and a general scene format are not part of the game runtime.

## Primary Goals

The engine should:

- make FPS gameplay straightforward to implement
- provide predictable frame times
- expose Vulkan errors clearly during development
- keep ownership and lifetime rules explicit
- minimize hidden global state
- remain understandable by one developer
- support automated tests where practical

## Non-Goals

The engine does NOT aim to support:

- arbitrary game genres
- multiplayer or networking
- mobile platforms
- consoles
- VR or AR
- visual scripting
- a plugin ecosystem
- an Unreal/Unity-style general purpose editor
- user-created mods
- runtime language scripting
- ray tracing
- multiple rendering backends
- DirectX, Metal, or OpenGL
- a generic ECS framework
- a generic asset pipeline for arbitrary applications

Features must not be generalized for hypothetical future use.

## Scope Rule

A feature belongs in the engine only if at least one of these is true:

1. the FPS currently requires it;
2. it significantly simplifies development of the FPS;
3. it solves an existing technical limitation.

"Another type of game may need it later" is not sufficient justification.

## Initial Technical Direction

Language: C++
Graphics API: Vulkan
Build system: CMake
Primary target: desktop PC

The project should prefer modern Vulkan APIs and simple explicit
rendering architecture over compatibility with old Vulkan implementations.

## Performance Philosophy

Performance work should be driven by measurement.

Do not introduce complicated data structures, multithreading,
GPU-driven rendering, asynchronous compute, or custom allocators
without evidence that they solve a measured problem.
