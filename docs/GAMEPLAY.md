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

The current executable uses one Jolt virtual character with a physics-visible
inner capsule. Standing and crouched collision shapes change together only
when clearance permits.

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

### Current Authored Interaction

The level may contain up to 16 non-blocking switch plates and 32 hinged doors.
The displayed eye ray selects the nearest door, switch, document, radio control
or physical box within 2 metres. Terrain, structural solids, every authored
prop proxy, accepted actor proxies, doors and boxes obstruct interaction.
A selected door or box can target its own front surface;
an inside origin is refused. Candidates within 0.1 mm of the true nearest
distance choose door, switch, box, document, then radio, followed by the durable
ID within each type. Reordering authored collections does not change the result.

E toggles the selected plate's linked light or requests the opposite door endpoint. Mid-swing E
reverses the last intent. R toggles a closed stationary door's lock from its
authored bolt side. Right mouse knocks without moving or unlocking the door.
Unsupported nearest actions and refusals do not act through another target.
Each action requires a release before its first press and between presses;
held/missed/inactive/minimized input cannot become a delayed action. Concurrent
edges are consumed with R, E, then knock priority. Left mouse retains the
independent flashlight/cursor behavior.

Doors stop before obstructing terrain, solids, props, other leaves, free or held
boxes, the player's current/interpolated presentation envelope and accepted
actor envelopes.
They do not push/crush participants or resume automatically after a blocker clears. Accepted poses are
shared by rendering, visibility and collision. Conservative clearance can
stop a door slightly early, including space the player has just vacated.

Generated handles, a sliding bolt and distinct brief knock/refusal cues provide
temporary visual feedback. Automatic door action sounds remain later work;
authored events can react to accepted door actions or actual endpoints/locks.
Audio transmission uses the accepted leaf angle.
Door motion, locks, feedback and switch light enables are run-local; recovery
preserves them and restarting restores authored initial values. No level file
is changed during play.

### Current Household Actions

Levels support up to 16 physical boxes, 32 readable documents and eight radio
controls. Boxes are generated 0.30 m cubes weighing one kilogram. E picks up
one box; while holding it, E drops it and right mouse throws it. Drop takes
priority over throw when both are pressed together. Other object actions
require empty hands. Pickup/drop/throw take effect once at the next physics
boundary; turning after pressing throw does not change its accepted direction.

A held box keeps its actual physical pose and collides with the world. A
bounded motor follows a point in front of the view; obstruction can make it
lag or release, without teleporting through a wall. Drop retains its pose and
velocities; throw adds one bounded impulse. Boxes can be pushed sideways,
fall, spin, collide and settle, but never provide ground, a stair or a jumping
surface. A falling player slides off when clearance permits; confinement keeps
collision without forcing either participant elsewhere. There is no inventory,
placement marker, damage, destruction or automatic respawn.

E opens a selected document. A/D change one page per press; E or Escape closes
it. Reading suppresses movement commands, look, stance changes, flashlight and
world actions while gravity, collision, boxes, doors, actors and audio continue.
Closing with Escape keeps the cursor captured. Held controls must be released
before they can act after a transition. Cursor release owes a drop at the next
active physics boundary; suspension freezes an existing hold, cancels pending
commands and preserves any drop already owed.

Documents contain a nonempty title of at most 80 Unicode scalar values and
1–16 nonempty ordered pages of at most 480 scalars each. Russian text, including
Ё/ё and explicit page line breaks, uses the trusted font; unsupported glyphs
or text that cannot fit at the supported minimum are diagnosed before Play.
The reader, action hints and feedback reserve space for both audio caption lanes.

E toggles a radio's exclusively linked captioned spatial ambience loop.
Turning it off stops sound and caption together; turning it on starts from the
beginning. The linked static `apartment-radio` prop has no collision proxies
and stays fixed; its transform supplies the audible position. Another radio or
actor cannot own that same source, and its autoplay must be off. Mute preserves
captions and time. A fresh process restores authored boxes and initial radio values.

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

The level contains static world geometry with independently assigned materials,
zero through eight authored point lights, zero through 128 fixed model placements,
up to 16 switches, hinged doors, authored audio, up to four characters and the
bounded household collections described above.
Placements have stable model identities and zero
through eight independent collision boxes. Decorative phone/radio placements
have no collision or interaction unless a radio control explicitly links the
radio prop. The separate P04 fixture places authored
radio and telephone sources at these props.

Interior levels may omit terrain and use authored boxes for floors, walls,
ceilings, and stairs. The packaged `apartment-stairs.level.json` blockout joins
Lena's room, a corridor, a kitchen, rear stairs, and a lower landing. Its
`apartment` and `lower-landing` entries exercise walking the route in both
directions after opening Lena's room door, without jumping or crouching. These
entries are authoring starts, not checkpoints or persistent progression.

The furniture and generated room door are acceptance content. Dynamic rigid
bodies are limited to the household box profile; moving platforms and further
world behavior require concrete gameplay needs.

## Lighting

Lighting is part of gameplay presentation and atmosphere, not merely a
rendering detail.

The game may use:

* authored environment lights
* local point or spot lights
* darkness and deliberately unlit spaces
* a player-carried light source
* lighting changes triggered by game events

The current interior supports zero to eight authored point lights and one
camera-mounted flashlight. Each light has a durable ID, initial enable and
shadow flag. Several switches may operate the same light; deleting a switch
does not change the light's initial value. Unlinked lights are valid.
Ambient may be zero and remains independent of point-light and flashlight
toggles. Runtime changes reset to authored values on a fresh run.

Up to four configured point lights cast shadows from rendered walls, furniture
and accepted door, box and character poses. A closed leaf blocks light through
its rendered surface; opening or stopping it changes its shadow at the accepted angle. The flashlight
retains its independent cone/range behavior. The neutral six-light interior
and eight-light capacity scene exercise this bounded implementation; their
T1 measurements are tracked separately from narrative acceptance.

This does not imply a requirement for a generic runtime light registry.

Features such as flicker, volumetric lighting, fog, exposure
changes, or additional dynamic lights should be introduced from concrete
visual or gameplay requirements.

## Audio

P04 supports explicit one-shot and looping authored sources. One foreground
dialogue/essential cue can run at a time; competing starts return busy, and
starting an active source is idempotent. Cancellation stops its sound and text.
Foreground speech ducks ambience to one quarter of its authored gain. Essential
captions remain available while muted, out of range or without an audio device.

An uncapped active-time clock determines cue order independently of rendering
and hardware completion. Mute changes gain without stopping time. Explicit
suspension and minimization freeze time; restore keeps the offset. Cursor
release permits ordinary audio to continue. Completed and canceled cues never
restart automatically. Device failure warns and continues silently; restarting
the run or explicitly starting a new editor audition retries initialization.

Sources use the displayed player view, distance attenuation and authored room
connections. Door transmission follows accepted motion, including an obstructed
partial angle. Lock changes alone do not change transmission. This is gain-only
authored transmission, with no geometric occlusion, reverb or diffraction.

`audio_captions_fixture` explicitly runs a temporary apartment sequence: radio
and ring, moving footsteps, the entire phone conversation, a two-second pause,
then an invitation contradicting the completed call. Neutral source labels do
not reveal hidden identity. Opening its level in the ordinary game runs only
authored autoplay ambience; this legacy fixture contains no authored events.
See [fixture controls and acceptance](DEVELOPMENT.md#p04-audio-and-caption-fixture).

Do not introduce a generic audio graph, middleware abstraction layer,
or procedural audio architecture without a concrete need.

## Events and Narrative State

The v11 authoring profile supports Boolean facts, named regions and finite
linear events. Triggers are scene entry, region entry, accepted player actions
and condition transitions. Conditions can observe facts, regions, lights,
actual door endpoints/locks, radio state, held boxes, open documents, actor
actions, event terminal states and elapsed active time. Opening a document
does not establish that its text was read or understood.

Steps set facts, lights, door targets/locks and radios, play a one-shot cue,
run an actor route, delay or wait for conditions. Door target acceptance is
distinct from arrival. A blocked door stays stopped until an explicit request;
a blocked actor retains its existing retry policy. Busy audio or actors wait
without preemption. Refused commands fail the event. Cancellation discards later
steps and stops only cue/route instances started by that run; accepted world
changes remain. Events never take the player's hands or force reader changes.

The ordinary neutral T4 scene enables a light on region entry, plays a captioned
cue, delays, runs a character route and records completion. Turning its
initially-on radio off cancels the sequence. Its validation remains separate
from story development and from human listening/visual acceptance; see the
[T4 evidence record](../openspec/changes/add-narrative-state-and-sequences/validation.md).

Facts and sequence history currently belong to one running process. Story
content, progression checkpoints and persistent session state remain separate
work. Prefer explicit data and game-specific event logic as those concrete
requirements arrive.

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

### Current neutral character routes

P07b supplies a neutral mannequin route fixture, independent of narrative
events. Actors turn toward a segment, walk on accepted support, face each mark,
then optionally perform Interact. Turns use idle at 120 degrees/second, with
positive rotation for an exact 180-degree tie. Arrival requires feet within
2 cm and facing within one degree. Supported steps are at most 30 cm; terrain
uses the existing 50-degree slope limit. Actors do not jump, fall across gaps,
sidestep, find paths or operate doors automatically.

Player, static, door and actor obstruction retain the last supported pose and
retry without timeout. Opposing routes may wait indefinitely. Standing time
produces no walking contacts. Repeating an active start is idempotent; a
different active route reports busy. Cancel retains placement and stops only
the action's sounds; explicit restart uses that placement. Fresh process entry
restores the authored starts, without changing the saved file.

Short footsteps follow distance-calibrated contacts. The final interaction
holds at its catalog marker if another foreground cue is active. Once free,
one spatial cue starts at accepted feet with a one-second Russian caption.
Its PCM contains a short effect followed by silence; logical completion and
arbitration use the full second. Mute and device loss preserve route identities
and captions. Capsule collision does not model animated limb contacts or foot
planting; the neutral mannequin retains its source sole dip and standing pivot.
See [development controls and evidence](DEVELOPMENT.md#p07b-scripted-characters).

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
