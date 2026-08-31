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

The executable uses one grounded, collision-constrained player. Mouse controls
yaw/pitch; W/A/S/D move relative to horizontal view orientation at 4.0 m/s;
Left Shift selects the 7.0 m/s sprint speed; Space performs a grounded jump;
and Left Control is hold-to-crouch. Escape releases the captured cursor. The
primary mouse action recaptures it while released and fires the rifle while
captured; a recapture click never fires.

Simulation advances on the main thread in fixed 1/60-second steps. Each sampled
interval contributes at most 100 milliseconds, fractional time is retained for
the next iteration, and render camera position interpolates the latest two
valid player poses. Gravity, wall sliding, a 0.30 m walkable step, bounded air
control, and stand-up clearance are implemented through one Jolt virtual
character. While the cursor is released, player controls are neutral but
gravity and collision continue.

The current collision world is static. Dynamic rigid bodies, moving platforms,
doors, projectiles, reciprocal pushing, and free-fly movement are not part of
this prototype.

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

The current prototype contains exactly one automatic hitscan rifle with
unlimited ammunition, a fixed fire interval and range, and bounded upward
camera recoil that recovers during later fixed steps. Shots originate from the
non-interpolated simulated eye and use base look plus already accumulated
recoil; a shot is emitted before its new recoil kick affects aim. The rifle
queries only the closest static collision surface.

The built-in range has exactly three fixed, collidable target plates with
equal starting health. Every plate uses the fixed shooting-target surface
texture while floor, boundaries, ordinary obstacles, the walkable step, and
the low-clearance structure use their corresponding immutable surface roles.
A damaging hit briefly highlights only the affected plate; the final hit still
highlights its textured appearance before persistent destroyed dimming takes
over. Destroyed plates remain visible and collidable and continue to stop
hitscan rays. Finite ammunition or ammo tracking, reloads, switching,
spread, projectiles, weapon models, crosshairs, audio, particles, enemies, AI,
physical target destruction, and generic health/damage/entity systems are not
implemented by this prototype.

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

The built-in prototype level currently authors exactly two fixed world-space
point lights: a dim cool pool around the player spawn and a warmer destination
pool deeper along the route. Their bounded radii do not overlap, leaving an
intentional dark transition over a small near-black ambient floor. The lights
do not follow the camera and are not loaded, animated, triggered, or controlled
by gameplay. Flashlights, shadows, fog, exposure adaptation, and a general
lighting system are outside this milestone.

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
