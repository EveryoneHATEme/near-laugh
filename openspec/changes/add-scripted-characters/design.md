## Context

[P07b's proposal](proposal.md) retains the original P07 change ID and narrows it
to authored runtime behavior after P07a. Current v8 data has doors, audio and
point shadows but no actors. PhysicsWorld owns one CharacterVirtual; its
stepCharacter currently advances the physics world, and door sweeps protect the
player's previous/current stance envelope. P04 cue time is uncapped active
time, while physics contributes at most 100 ms per sampled interval.

These differences matter: creating another player controller per actor would
advance the world several times and mix player input/stance policy into NPCs.
Advancing a route by audio time would also bypass collision after a stall.

## Goals / Non-Goals

**Goals:** small immutable actor definitions, explicit route results, shared
accepted motion and bounded action/audio coordination. Provide a runnable
neutral scene without P05 or editor-only data.

**Non-Goals:** generic actor framework, CharacterVirtual per NPC, navigation,
avoidance, automatic door use, gameplay scene scripting, root motion,
full animated-mesh collision, prop manipulation or save-game serialization.
Full authoring remains explicitly assigned to P07c.

## Decisions

### 1. One v9 character object; simple routes owned by actors

Use the exact fields and bounds in
[scripted-characters](specs/scripted-characters/spec.md). Marks own feet position
and yaw; actors reference their initial mark; routes reference one actor and an
ordered mark list. A null initial route means idle. Optional final_clip is
null or interact; idle/walk selection is fixed by the model catalog. One shared
mark can be referenced by several records. No transform hierarchy or arbitrary
animation state graph is needed.

Actor placement scale is catalog-owned, initially 1. The P07a preparation
record calibrates model origin, forward axis, capsule, clip stride and markers.
World metadata exposes those small values without decoding meshes. Source
links identify P04 sources; a non-null source cannot belong to another actor
or fill both actor roles. Referenced sources cannot autoplay. Short non-looping
footsteps use ambience-kind cues; the short neutral interaction uses an
essential cue with a matching Russian caption.

A route-owned actor prevents accidental commands driving the wrong cast member
and avoids actor-local duplicated route lists. Shared definitions are immutable
during play. Resolve durable IDs at load; retain IDs for diagnostics/results,
using ordinary bounded vectors and resolved indices internally.

### 2. Explicit action state with accepted-distance walking

The controller is concrete gameplay code under core/gameplay with pure
decision/result tests. An action stores an instance serial, route/mark cursor,
accepted feet/yaw, phase, saved walking distance, clip/transition state and
whether its final marker has fired. Start returns started/already-active/busy;
cancel retains position, cancels owned cues and transitions to idle. New start
after completion/cancel uses the current pose. Initial routes start once.

At each mark, turn to the segment heading, walk to its supported feet position,
then face the mark yaw. Turning uses idle plus bounded shortest-arc yaw; it
does not manufacture a turn clip. Zero horizontal distance skips only travel,
not required facing. If the mark's height cannot be reached by ordinary
grounded traversal, report support blockage instead of treating matching X/Z
as arrival. Repeated marks remain valid ordered stops.

Drive walk phase by accepted horizontal travel divided by the calibrated
cycle distance, retaining phase when blocked. Blend toward idle on blockage
and back to the saved walk phase when moving again. Blend clocks can advance
while waiting but walking distance/contact clocks cannot. A final one-shot
advances only after accepted arrival/facing. Clip completion transitions to
idle and keeps a completed result until a new explicit start.

Automatic route retry is deliberately separate from P03 door intent. No
timeout, sidestep or blocker removal is synthesized. Opposing actors can wait
indefinitely; an authored scene must provide a clear route or later concrete
start/cancel request.

### 3. Kinematic actor capsules in the existing physics world

Use catalog-sized upright capsule bodies owned privately by PhysicsWorld.
They have no native velocity that pushes other bodies and no ragdoll or
per-bone collision. Accept route proposals through swept shape/clearance and
ground-support queries, then install the accepted position explicitly. Preserve
the intended route direction; do not slide laterally around a blocker.
Reuse the existing step/slope limits for small steps and stairs with swept
up/forward/down clearance and supported landing. Never install an unsupported
candidate. The capsule is a gameplay proxy; the authored fixture must leave
visible arm/body clearance at the interaction mark.

Refine the existing fixed-step composition only as needed:

1. Advance the physics world once; move the player against installed actor and
   door poses, recording its previous/current stance envelope.
2. In durable actor-ID order, accept grounded actor displacement against static
   bodies, installed doors, current other actor bodies and the swept player
   envelope. Exclude the actor's own body from its casts. Keep other actors'
   already-accepted swept envelopes where needed to prevent crossing swaps.
3. Advance doors in their existing durable-ID order against accepted actor
   bodies/envelopes and the player's protected view envelope.
4. Publish one accepted actor/door snapshot, then resolve contact/action events.

Do not run PhysicsSystem::Update once per actor. Prevent actor capsules from
becoming player support/step surfaces; use the existing character contact
boundary to reject walkable actor contacts while retaining side/head
blocking. Include actors in stance clearance, continuous door sweeps and
interaction segment tests. The player's own representation remains excluded
from targeting. No actor interaction candidate is added.

Initial validation uses catalog proxy bounds and current world support helpers,
with full actor/entry/door overlap checks. Marks validate support and static
clearance; doors blocking later routes are valid. Route segment traversability
is tested in play, not certified from endpoint validation alone.

### 4. Two explicit clocks with bounded marker handoff

Motion, turn, animation/blend and marker decisions use completed fixed steps.
P04 retains uncapped active time for already-started cues. Advancing audio
cannot move an actor or cause an arrival. The cue position override is the
accepted actor position; the listener remains the displayed player eye and
door transmission uses accepted angles.

Record each crossed walking contact once in fixed-step order. Calibration
must permit at least 0.20 s between contacts at the allowed speed; selected
footstep clips are at most 0.15 s, so the 100 ms simulation batch cannot
accumulate a multi-footstep burst from one actor. Start contacts at the next
main-loop audio handoff; do not replay discarded wall time or make a walking
sound loop continue while blocked. Record actual visual/audio drift during
acceptance; target a contact onset within 100 ms plus measured output latency.
Device-free event tests do not establish hardware latency.

Interact advances to the catalog marker and attempts the actor-owned essential
source. On Busy, hold at the marker and retry on later steps; preserve the
existing clue. On Started, latch its instance and continue the animation on
later fixed steps. Null source advances silently. Missing/invalid content was
already rejected at preflight. Muted/no-device playback is logically Started,
so captions and completion remain available. Cancel the owned instance on
route cancellation; never call cancelAll for one actor. Reject external starts
of actor-reserved sources at the composition boundary. Dialogue synchronization
and longer acting sequences are P05/later content work; short action sounds
are not a general animation/audio timeline.

Add an explicit development suspension control through the internal fixture
entry, exercised by tests and a development executable/control path. It freezes
player, doors, all actor clocks and CueCoordinator together. Clear accumulators
on pause/resume/minimized wait and consume input edges. Cursor release continues
ordinary world/audio time. Recovery never changes actor or cue identities.
A fresh run reconstructs state; this does not add live scene reload.

### 5. Runtime fixture and minimal editor compatibility

Package a neutral route level based on supported interior structures and
lighting, with safe starts away from actor placement, a turn, short stairs,
a player-operated door and final interaction mark. Select its initial route
through authored data; launch via ordinary --level/--entry. Provide explicit
development restart/cancel/pause controls separately from filename behavior.
A four-actor variant covers independent actions and opposing blockage.

Prepare P07a assets before physics handoff, own actor state in Engine, and pass
render handles/palettes through the P07a frame boundary. Render current accepted
placements; do not separately interpolate actors through blocking doors.
Minimal editor work preserves v9 data, initial actor rendering and selected
resource preflight. No route autoplay in editor. Dedicated selection/properties
and interactive inspection are implemented by P07c.

### 6. Acceptance exercises the actual coordination boundaries

Behavior tests cover canonical/legacy codec, valid/invalid references, every
supported state transition, start/cancel idempotence, known swept-collision
cases, player stance, step/support failure, door reversal, actor reordering,
cue Busy/mute/device loss and equivalent fixed-step batches. Explicitly test
intervals over 100 ms, zero-step frames, minimized waits and repeated restart.

Vulkan smoke exercises both moving characters and doors, matching pose/shadow
state, capacity, partial construction and teardown. Use P07a's timing setup on
one/four moving-actor scenes and the same light-only comparison, retaining
failed measurements and visual/listening observations. Record turning foot
pivot, capsule/limb limits and any missing latency evidence. T2 remains open
until P07c authors and plays its second scene.

## Risks / Trade-offs

- Deterministic actor priority can create a deadlock → expose blocker/action
  state and document authored clearance; do not add speculative avoidance.
- Capsule collision is not limb contact simulation → keep interaction space
  clear in acceptance and show proxy versus visual bounds in P07c.
- Existing physics stepping hides world advancement → preserve the player
  regression suite while moving only the shared once-per-step operation.
- Fixed simulation and active-time audio differ during a stall → constrain
  these cues to short markers and explicitly verify the capped-motion policy.
- Shared schema changes can drift after P07a → rebase affected main
  requirements before implementation, without preassigning versions beyond v9.

## Migration Plan

Preserve exact v2-v8 readers and fixtures. Add strict v9 shapes and empty
character normalization for older versions; explicit saves write v9. Update
packaged levels and their deterministic preparation scripts in the same
implementation, retaining versioned old fixtures for compatibility tests.
Opening never rewrites a file; document Save As for old-build compatibility.

Build/test game and minimal editor compatibility before acceptance. During
implementation update ARCHITECTURE/GAMEPLAY/RENDERING/DEVELOPMENT for actual
ownership, v9, controls and asset workflow (including stale v7 module summaries
in the current architecture/development docs). Record validation, archive P07b
after acceptance, then rebase P07c. An older executable requires a retained
v8 original; removing characters from a v9 file is not an automatic downgrade.
