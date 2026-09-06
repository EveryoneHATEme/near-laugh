# Gameplay Model

## Purpose

This document defines the gameplay assumptions that the runtime is
explicitly allowed to make.

The project targets one specific game:
a single-player first-person narrative horror experience.

The runtime may be specialized for that game.

It must not assume conventional FPS combat mechanics unless they become
actual game requirements.

## Experience Model

The game is primarily built around:

* first-person exploration
* authored environments
* atmosphere and tension
* environmental storytelling
* interaction with the world
* scripted and triggered events
* spatial audio
* authored lighting and darkness
* deliberate pacing

The game may include characters, threats, pursuit, stealth-like situations,
or other forms of danger.

Those concepts do not imply a conventional shooter combat model.

## Player

There is exactly one local human-controlled player.

The player uses a first-person camera.

Primary input devices are:

* keyboard
* mouse

Gamepad support is not currently required.

The runtime does not need abstractions for multiple local players.

## Player Movement

The player uses a grounded, collision-constrained first-person character
controller.

Core requirements include:

* walking
* gravity
* collision against level geometry

Additional movement capabilities such as:

* sprinting
* crouching
* jumping

may exist when required by the game.

Their presence in the current prototype does not make them permanent
architectural requirements.

The player controller is gameplay-oriented and does not need to be
implemented as a general-purpose rigid body.

Shooter-specific movement features such as advanced air control,
bunny hopping, movement abilities, or weapon-driven movement are not
assumed.

### Current Prototype

The current executable uses one grounded Jolt virtual character.

Mouse input controls yaw and pitch.

W/A/S/D move relative to the horizontal view orientation.

The current prototype supports:

* walking
* sprinting
* crouching
* jumping
* gravity
* wall sliding
* walkable steps
* bounded air control
* stand-up clearance checking

These are current implementation details rather than promises about the
final movement design.

While the cursor is released, player controls are neutral while gravity
and collision simulation continue.

## Camera

Only a first-person gameplay camera is required.

Relevant capabilities may include:

* mouse look
* configurable field of view
* pitch limits
* subtle camera motion
* camera shake
* temporary visual effects required by authored events

Do not create a generic cinematic-camera framework unless a concrete
game requirement appears.

Weapon view models, recoil systems, crosshairs, and other shooter-camera
concepts are not baseline requirements.

## Interaction

Interaction with authored environments is a core gameplay concern.

The game may require interactions such as:

* opening or closing doors
* operating switches or controls
* activating authored objects
* picking up specific objects
* inspecting objects
* reading environmental information
* starting or advancing scripted events
* context-sensitive world actions

This list describes likely interaction categories, not a requirement for
one universal interaction framework.

Prefer concrete interactions with clear gameplay meaning over a generic
item/action/component abstraction.

A generalized inventory or equipment system must not be introduced unless
the actual game design requires one.

### Current Prototype Light Switch

The level may contain one fixed, non-blocking switch plate. A new E press
toggles its linked point light when the displayed eye ray hits the plate
within 2 metres, inclusive, and static collision does not obstruct it. Terrain,
solids, and the chair's authored box proxy are the blockers; the plate adds no
character collision. Targeting from inside the plate or blocking geometry is
rejected. The packaged example is on the spawn-facing central obstacle.

Interaction requires an observed release before the first press and between
presses. Held, missed, out-of-range, cursor-released, capture-transition,
minimized, and closing input cannot become a delayed activation. Each event
batch is evaluated once after simulation, even with zero fixed steps.

Only the linked light's run-local enable bit changes. Ambient, the other point
light, flashlight, authored intensities, and level files remain independent.
Presentation recovery preserves the current state; restarting restores the
authored initial state. This interaction has no HUD, animation, audio, or
save-game persistence.

## World

Levels primarily consist of authored content such as:

* static environment geometry
* props
* doors and other interactive objects
* lights
* spatial audio sources
* triggers
* scripted event markers or data
* visual effects
* character or threat placements where required

The game does not require an arbitrary hierarchical scene representation.

Level data should represent the information the runtime and authoring tools
actually need rather than trying to model every possible game object.

### Current Prototype

Startup loads a versioned level, using the packaged prototype by default or
an explicitly selected authored file. Each level has named entries and an
authored default. The selected entry supplies the initial foot position and
yaw, including both presentation snapshots before the first frame. Every
entry must have height-specific support and standing clearance.

The level currently contains static world geometry, authored point lights,
a fixed prop with a simplified collision proxy, and an optional light switch.

The current collision world is primarily static.

Interior levels may omit terrain and use authored boxes for floors, walls,
ceilings, and stairs. The packaged `apartment-stairs.level.json` blockout joins
Lena's room, a corridor, a kitchen, rear stairs, and a lower landing. Its
`apartment` and `lower-landing` entries exercise walking the route in both
directions with the current controller, without jumping or crouching. These
entries are authoring starts, not checkpoints or persistent progression.

Dynamic rigid bodies, moving platforms, interactive doors, and other
world behavior should be added only when required by gameplay.

## Lighting

Lighting is part of gameplay presentation and atmosphere, not merely a
rendering detail.

The game may use:

* authored environment lights
* local point or spot lights
* darkness and deliberately unlit spaces
* a player-carried light source
* lighting changes triggered by game events

The current prototype includes authored point lights and one
camera-mounted flashlight.

This does not imply a requirement for a generic runtime light registry.

Features such as shadows, flicker, volumetric lighting, fog, exposure
changes, or additional dynamic lights should be introduced from concrete
visual or gameplay requirements.

## Audio

Spatial audio and authored sound are expected to be important to the
target experience.

Likely requirements include:

* ambient sounds
* localized environmental sounds
* one-shot event sounds
* sounds associated with interactions
* sounds triggered by scripted events
* character or threat audio where required

Audio systems should serve authored gameplay and atmosphere.

Do not introduce a generic audio graph, middleware abstraction layer,
or procedural audio architecture without a concrete need.

## Events and Narrative State

The game may require authored sequences whose behavior depends on player
location, interaction, previous events, or persistent state.

Useful concrete concepts may include:

* trigger volumes
* one-shot events
* event conditions
* simple sequencing
* local state flags
* changes to world objects
* changes to lighting or audio
* character or threat activation
* progression checkpoints

Prefer explicit data and game-specific event logic while the requirements
remain small.

Do not introduce a general-purpose scripting language, behavior-tree
framework, or visual scripting system merely to implement simple authored
sequences.

If event complexity eventually demonstrates that a scripting mechanism is
needed, that should be treated as a new architectural requirement and
evaluated at that time.

## Characters and Threats

The game may contain non-player characters or threats.

They may be:

* completely scripted
* driven by simple state machines
* reactive to player position or actions
* capable of navigation or perception
* activated only during specific authored sequences

Do not assume that such actors require:

* health
* damage
* weapons
* combat states
* loot
* conventional enemy AI

A generic AI framework is not a goal.

Navigation, perception, animation state, spawning, or other actor systems
should be added only when a concrete encounter requires them.

## Combat

Combat is not a baseline gameplay assumption.

Do not introduce:

* weapon frameworks
* ammunition systems
* reload mechanics
* projectile architecture
* hitscan infrastructure
* damage frameworks
* enemy health systems
* combat inventories
* combat-oriented AI abstractions

unless the actual game design introduces a concrete need for them.

If the game later contains a specific weapon or defensive interaction,
implement the smallest model that serves that mechanic rather than
assuming the project has become a general FPS.

## Simulation

Gameplay simulation runs independently from rendering frequency where
necessary.

The current player simulation uses a fixed timestep.

Exact timestep policy is defined by the relevant implementation and
OpenSpec requirements.

Not every narrative or interaction system needs to run at the physics
frequency.

Systems should use the simplest timing model appropriate to their behavior.

## Save and Persistent State

The game is expected to require save/load support.

Persistence should focus on actual game progression, for example:

* player progression or location where appropriate
* completed events
* important interaction state
* progression flags
* relevant world state

Do not serialize arbitrary runtime internals merely because they exist.

The save model should be designed around the state needed to reconstruct
the intended game experience.

## Out of Scope

The following are outside the current gameplay scope:

* networking
* replication
* multiplayer prediction
* matchmaking
* split-screen
* competitive multiplayer systems
* vehicles
* strategy-game unit simulation
* MMO-scale entity simulation
* procedural open worlds
* generic RPG systems

Shooter mechanics are also outside the baseline scope unless explicitly
introduced by the game design.

## Prototype Versus Product

Current prototype behavior is evidence about what exists today, not a
permanent definition of the final game.

Prototype mechanics may be removed, simplified, or redesigned when the
actual horror experience provides better requirements.

Do not preserve a prototype feature solely because other systems or tests
currently assume it exists.
