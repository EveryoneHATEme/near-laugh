# Gameplay Model

## Purpose

This document defines the gameplay assumptions that the engine
is explicitly allowed to make.

The project targets one specific class of game:
a single-player first-person shooter.

## Player

There is exactly one local human-controlled player.

The player uses a first-person camera.

Primary input devices:

- keyboard
- mouse

Gamepad support is not currently required.

The engine does not need abstractions for multiple local players.

## Player Movement

The expected movement model includes:

- walking
- running
- jumping
- crouching
- air movement
- gravity
- collision against level geometry

The player is represented by a gameplay-oriented collision shape.

The player controller does not need to be implemented as a
general-purpose rigid body.

## Camera

Only a first-person gameplay camera is initially required.

Required capabilities:

- mouse look
- configurable field of view
- camera pitch limits
- weapon/view-model rendering
- camera shake
- temporary effects such as recoil

Do not create a generic cinematic camera framework unless a concrete
game requirement appears.

The current executable uses a temporary concrete free-fly inspection camera to
exercise the first world-space rendering slice. Mouse controls yaw/pitch;
W/A/S/D translate relative to horizontal view orientation; Space and Left
Control move vertically; Left Shift selects sprint speed. Escape releases the
captured cursor and the primary mouse action recaptures it. Translation is
frame-rate independent with a 100 ms discontinuity cap.

This prototype camera is not the future physical player controller. It applies
no gravity, collision, jump impulse, crouch shape, or ground constraint, and it
may pass through the floor, boundaries, and objects. Those gameplay behaviors
remain requirements for a later player-controller change.

## Weapons

The game may contain:

- hitscan weapons
- projectile weapons
- ammunition
- reload mechanics
- recoil
- spread
- weapon switching

The weapon system should favor explicit FPS concepts over a generic
item/equipment framework.

## World

Levels consist primarily of:

- static environment geometry
- doors and other simple interactive objects
- lights
- enemies
- pickups
- triggers
- projectiles
- visual effects

The project does not require an arbitrary hierarchical scene editor.

## Enemies

Enemies may require:

- navigation
- perception
- combat states
- health
- damage
- simple animation state
- spawning and despawning

A generic AI framework is not a goal.

## Simulation

Gameplay runs independently from rendering frequency where necessary.

Exact timestep policy is defined by the relevant OpenSpec specification.

## Out of Scope

Networking, replication, multiplayer prediction, matchmaking,
split-screen, vehicles, strategy-game unit simulation, RPG inventory
systems, procedural open worlds, and MMO-scale entity counts are out
of scope.
