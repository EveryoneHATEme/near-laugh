## Context

See [proposal.md](proposal.md) for the intended outcome. P04, P10 and the full
P07a-P07c chain are accepted and archived; current main specs and
`level_format_version` describe v9. This plan replaces the older P06 assumption
of nonphysical carrying with the user's requested designated physical pickup
objects and throwing. It retains document reading, radio and full authoring.

The relevant existing boundaries are:

- `level_document.hpp`, `level_codec.cpp` and shared world validation own strict
  authored data. `PrototypeLevel` remains immutable throughout a run.
- `AuthoredInteraction::update` chooses one displayed-eye target with 2 m reach,
  deterministic ties, obstruction and sampled release-before-press controls.
- `PhysicsWorld` already owns Jolt, static proxies, kinematic doors/actors and
  one `CharacterVirtual`. `Engine::tick` advances the world, player, actors and
  doors on bounded 1/60 s steps. It currently has no dynamic prop ownership, and
  its virtual player lacks a body visible to independently moving boxes.
- `OpaqueBoxFrame` and the fenced changing-geometry path already serve doors.
  They support yaw only; 192 boxes are fully consumed by 32 doors x 6 boxes.
  `prepareSceneAssets` currently prepares their material only if doors exist.
- `CaptionFont`, `TextResources` and `CueCoordinator` already provide Russian
  glyphs, caption layout, active cue time, cancellation and silent playback.
- `EditorDocument` owns concrete commands, 128-entry history and durable-link
  repair. The editor previews definitions and launches a separate saved-file
  game process; it does not construct gameplay physics.

## Goals / Non-Goals

**Goals:** keep the physical body authoritative; preserve current player,
actor, door and resource-lifetime guarantees; reuse the changing box renderer
and text/audio owners; make every required record authorable and testable.

**Non-Goals:** a generic rigid-body scene/component system, moving imported
models, editable mass/shape/material libraries, a general inventory, a second
physics world or simulation thread, hand animation, collision-triggered story
logic, session menus or checkpoint storage. The limited contact policy is
intentional: throwing moves boxes; it does not create combat or movable actors.

## Decisions

### 1. Separate concrete definitions; one current format transition

Add `household: {boxes, documents, radios}` to v10. Each record has a durable
ID unique in its typed collection; references stay typed strings. The exact
bounds/fields live in the household and persistence deltas. Boxes use a fixed
0.30 m cube and 1 kg mass; authored center/yaw initializes a dynamic body at
zero velocity. Size/mass are displayed read-only in the editor. Clear airborne
starts are valid. Documents are fixed thin panels with title and ordered pages.
Radios link an existing noncolliding `apartment-radio` prop and an exclusively
owned spatial looping ambience source. This avoids changing static-prop
identity, collision or model-loading contracts.

Use project-owned records and existing validation helpers, with concrete
household validation in `near_laugh_world`. Initial box overlap is checked
against static geometry/proxies, boxes, all entries and initial doors/actors;
touching support within tolerance is valid. No new terrain/world boundary or
requirement to place boxes on a floor is inferred. Safe bad references remain
repairable in the editor. Text scalar bounds belong to world validation; actual
glyph/layout fit additionally belongs to resource preflight.

Alternative considered: make every static prop optionally dynamic. That would
change baked imported geometry, proxy ownership and a much larger authoring
profile without serving the initial box requirement. Authored placement slots
are also omitted because release now delegates placement to actual physics.

### 2. One physical owner and one bounded holding relationship

`PhysicsWorld` owns up to 16 dynamic box bodies, indexed through a stable
durable-ID resolution established at startup. Jolt types stay private. A small
concrete household runtime owner holds the selected box identity, reader
document/page, radio enables, command result and feedback duration. Physical
pose, velocities and sleep state are read from physics, not mirrored as a
second independently mutable transform store.

Bodies remain `Dynamic` while held. Use one force/torque-limited Jolt SixDOF
motor constraint between the selected body and the fixed world, with a target
pose derived from the fixed-step accepted player eye and look, never the
render-interpolated eye; no extra kinematic target body is needed. Retain the
acquired object-to-view orientation as the desired carry orientation. Keep the
sole held body awake during active simulation (restore normal sleeping on
release), including after stillness, target changes, obstruction and resume. Contact may prevent reaching it. Drop removes
the constraint without changing pose/velocities; throw removes it then applies
one forward impulse at the center of mass. Natural contact supplies tumbling.
There is no manual rotation, charged throw or fake camera-attached duplicate.

Initial tuning values (implementation constants, not authoring fields):

| Parameter | Starting value / limit |
| --- | --- |
| Hold target | 0.90 m forward and 0.15 m down from the view |
| Hold force / torque cap | 80 N / 4 N m per motor axis |
| Motor response | 5 Hz, critical damping |
| Hold break separation | 1.50 m from desired target |
| Throw impulse | 6 N s along accepted view direction |
| Body linear / angular speed cap | 12 m/s / 12 rad/s |
| Friction / restitution | 0.6 / 0.1 |
| Collision integration | LinearCast plus two fixed collision substeps |

Preserve existing gravity (-18 m/s squared). Damping and motor constants may be
tuned from the required contact/feel tests without adding editor parameters;
record final values with evidence. Changing shape, mass, caps or supported
contact guarantees requires reconciling the plan/specs, not silently dropping
tests. Jolt LinearCast uses the starting orientation for its translational
sweep, so it alone is not proof against spinning tunneling. The cube profile,
speed caps, fixed substeps and maximum-speed tests against a 0.02 m wall
and a minimum-valid 0.04 m door leaf bound this claim. Increase fixed collision resolution if those tests expose an unresolved
gap; do not add adaptive time ownership or a general CCD framework.

Acquisition never moves a body immediately. Recheck reach, occlusion and empty
hands before applying it. Build the constraint before committing held identity;
failure leaves the box free. Drop/throw removes the constraint first. Invalid
targets, excess separation or deliberate cursor release break the hold at the
actual pose without an impulse. Minimize/explicit suspension preserves it.
Box escape from an open level is physical loss of reach, not a request for an
inventory or automatic respawn. A corrupt/nonfinite physics result is diagnosed
and terminates the run safely rather than being hidden by teleportation.

### 3. Player, actor and door contact stay explicit

Add the virtual player's supported inner kinematic collision shape so thrown
boxes collide with a stationary player. Validate the inner body ID, including
Jolt's silent allocation-failure case. `CharacterVirtual` owns its lifetime;
avoid separately deleting it. Synchronize accepted standing/crouched shapes
and positions. Filter that inner representation out of visibility and existing
actor/door paths that already test an explicit player envelope, while keeping
it available to dynamic contacts. Test failed stance replacement transactionally.

Use contact settings to prevent box velocities driving the player while
allowing bounded controlled pushes into free boxes. Dynamic boxes are not valid
player ground, jump or stair surfaces. Reuse the existing actor-contact approach:
classify ground, prevent stair/stick probes from promoting a contact into
support, and resolve top contact toward clear lateral space without penetration.
Account for Jolt's downward character-weight impulse even when the public ground
state is unsupported. A player confined between boxes and walls can remain
trapped; no universal escape/relocation policy is promised. Acceptance tests
must cover that limit explicitly rather than claiming all arrangements safe.

Actors already find support only on terrain/structures. Include boxes in their
movement sweeps and retain waiting at the last supported accepted pose. Keep
actors kinematic: impacts do not displace or injure them. Door sweeps likewise
treat free/held boxes as blockers and stop without pushing/crushing, preserving
the required new-press restart policy. A box may bounce from an accepted leaf;
that contact does not request door motion. Preserve coherent protection of
current and displayed player/actor envelopes throughout each update.

Alternative considered: let physics shove doors/actors or let box momentum
propel the player. This changes existing movement/route/door contracts and is
outside the requested pickup/throw capability.

### 4. Input, fixed-boundary commands and visible state

| Mode | E | Right mouse | R | Left mouse | Escape |
| --- | --- | --- | --- | --- | --- |
| Empty-handed exploration | nearest concrete interaction | existing knock | existing lock | existing flashlight | release cursor |
| Carrying | drop | throw | consumed | existing flashlight | release cursor and drop |
| Reading | close | consumed | consumed | consumed | close only |

Reading uses A/D for previous/next page. It starts at page one and suppresses
movement, look, stance changes and world actions, while preserving current
stance. Physical gravity/contact still apply to the player. World/audio time
continues while reading; this is the planning default matching the existing
active-world policy, distinct from explicit suspension. Carrying occupies
world interaction: put a box down before reading or operating a door/radio.
Hints make that limitation and controls visible.

Keep one concrete pending physical command, not an event bus. After the batch's
fixed steps, the current displayed-eye query chooses at most one target. Accept
pickup/drop/throw into that single pending record; another conflicting edge is
consumed with feedback. Capture target identity/selection ray for pickup and
the accepted view direction for throw in that request. At the next fixed
boundary, recheck current reach/obstruction for that target and consume it
once before physics advancement. A zero-step batch cannot duplicate or apply
an impulse outside simulation. Suspension, close and mode/capture invalidation
discard unapplied requests. Drop outranks throw when carrying; empty-handed
R/E/knock priority and no-fallthrough remain unchanged. A failed recheck does
not select a replacement object.

Before each world step, derive the hold target from the accepted simulation
eye/look and apply the pending command. The displayed interpolated eye is used
only for selecting a new interaction; it never drives the motor. Equivalence
tests supply the same commands and simulation-eye/look sequence at each fixed
boundary, including changing look while holding. Advance one PhysicsWorld update (with its fixed
internal collision substeps), then existing player, actor and door coordination.
Capture the resulting authoritative poses before preparing the frame. The
next body update sees the installed accepted player/actor/door geometry. Do not
introduce an additional world update per body or per rendered frame.

All modes consume sampled button state, including zero-step, inactive, minimized
and recovered batches. Rearm held actions and movement across ownership changes
only after release, and reset accumulated look. A separate release obligation
created by cursor loss while the world remains active is processed at the next
fixed boundary; it is not a retained user throw. An already-required safety
release survives a subsequent suspension and recapture and must complete before
the next advancing physics step after restore. Suspension alone retains the
held body and targets without advancing them. Discard ordinary pending input
commands on suspension, but never cancel an existing safety-release obligation. Add neutral development pause
and mute controls for P06 validation, using the existing control entry pattern;
they do not add P12 menus or change ordinary Escape behavior.

### 5. Shared target selection and generated presentation

Add concrete free-box, readable-panel and radio candidates to the existing
nearest-target arbitration, retaining door/switch priority before new types
and stable IDs within each type. Test box rays against the full current oriented
cube. Physics queries exclude only the selected object's own surface and the
player's representation; they must retain unrelated earlier/equal blockers and
inside-origin rejection. Nonblocking documents and radios still take target
priority by their visible bounds, without receiving invented collision bodies.

Extend the existing changing opaque box request from yaw to a validated unit
quaternion (or equivalent full orthonormal rotation); adapt current door
producers by converting their yaw. Transform vertices and normals consistently.
Use accepted physical poses directly, with no box render interpolation ahead
of collision. Reuse the CPU-generated, fenced per-frame geometry for color and
shadow passes. Do not introduce imported mutable meshes or a render subsystem.

Budget the complete presentation: 192 door boxes + 16 physical boxes + 32
document panels + 8 radio indicators = 248 generated boxes. Test all together;
do not reduce the supported door count. Prepare the shared obstacle material
when any changing household geometry needs it, including door-free scenes.
Initial parcel/panel visuals use the existing generated material with tints;
they are representative markers, not final paper/cardboard art. Radio state has
a small tinted indicator and text feedback, so darkness/mute cannot hide it.
Only current authored previews run in the editor. Compatible resize/recovery
retains materials and font resources; mutable buffers wait for their frame
fences. Failed editor replacement retains the complete preceding scene with
its matching household geometry and a stale-preview indication.

### 6. Text and radio reuse existing owners

Add a resolved reader/feedback presentation alongside captions, with bounded
borrowed strings or prepared page layout. The runtime owns page choice and
feedback time; renderer/text code only lays out and draws. Reserve the lower
caption lanes and use a separate reading panel, title/page controls and hint
area. Validate all pages at 800x600 as well as their Unicode limits before Play;
no hidden truncation or per-frame font rebuild is permitted. Increase checked
text geometry capacity from the caption-only budget to cover simultaneous
document, controls, feedback and both caption lanes (initial aggregate bound
8192 vertices), and test the actual combined maximum. Editor preview reuses
the same pure layout and font; game rendering never imports ImGui.

Resolve each radio's exclusive source once; reject prop/source sharing with
another radio or actor. Start/cancel through `CueCoordinator`, override source
position from the immutable prop, and leave its authored source definition
untouched. Require a captioned ambience loop with autoplay disabled. Initial-on
state starts exactly once under household ownership. Ordinary audio autoplay
continues for unrelated sources. Resolve captions after household toggles so
off-state feedback cannot coexist with a stale radio caption for one frame.
Device-free playback and device loss retain logical on/off and caption behavior.

### 7. Authoring and downstream boundaries

Extend concrete EditorDocument object values, commands, IDs, properties, picking
and initial preview. Use the existing property-draft commit model, including
whole committed text/page edits. Renaming props/sources rewrites radio links in
the same history entry; deletion leaves broken links. Duplicates keep outgoing
links and new identity, exposing exclusive-ownership conflicts for repair.
Radio control versus prop selection must be explicit in the list/properties.
Use existing nearest structural/terrain placement, with box bottom at the hit;
placing on static furniture can use numeric initial coordinates. No editor
physics preview or runtime mutation of editor definitions is required.

Keep P06 one coherent change with staged verification of data, physics,
runtime/presentation, then authoring and integrated T3. Every stage remains in
its task checklist; finishing physical boxes alone does not complete P06.
P05 consumes concrete state later; P09 must design checkpoint reconstruction
for physical objects rather than serialize Jolt internals or carry constraints.
Do not add unused P05/P09 adapters now. Update affected current docs and the P06
roadmap acceptance during implementation, while preserving actual acceptance
status and the P06-before-P05 work order.

## Risks / Trade-offs

- Physical holding can jitter or release at obstructions. Mitigate with the
  fixed profile, bounded motors, break distance and manual close-wall tests;
  a mathematically stable test alone does not establish comfortable handling.
- CCD does not guarantee arbitrary rotational collision. Mitigate with explicit
  shape/speed limits and spinning thin-wall/door tests plus fixed substeps.
- The new player inner body changes query/contact ownership. Mitigate with
  stationary/crouched impact, self-query, stance failure, actor/door regression
  and independent lifetime review before acceptance.
- Boxes can obstruct navigation or become unreachable in an open level.
  Preserve physical results without teleportation; use a bounded neutral test
  room and document the unsupported-ground/confinement limits.
- Physical positions are numerically sensitive. Compare equivalent fixed-step
  runs on the same build with tolerances and exact command/identity counts;
  do not promise identical states across compilers or platforms.
- Reading continues world time. Keep captions visible and verify concurrent
  physics/audio, with explicit pause tested independently.
- Generated markers limit visual fidelity. Validate the supported behavior and
  readable UI now; different shapes/imported art require a later profile review.

## Migration Plan

1. Extend current v9 schema to v10 once, with required empty household arrays
   for exact legacy v2-v9 normalization. Preserve earlier strict shape checks,
   migrations, IDs/order and source-version notices. Preserve legacy fixtures.
2. Extend canonical serialization, new-interior defaults, runtime preflight,
   editor snapshots/history and packaged level preparation tools coherently.
   Explicitly migrate packaged current scenes to v10; opening user files does
   not write them. Save As is the way to retain a file for older executables.
3. Integrate the new owner and rendering/input changes behind empty collections;
   empty household scenes retain old behavior and require no household assets.
4. Add the neutral P06 scene, then complete the independent UI-authored scene.
   Update docs during implementation and synchronize delta specs only after
   implementation acceptance. No current main specs are changed by this proposal.
5. Rollback before acceptance uses the prior executable and preserved v9 files.
   There is no automatic v10 downgrade that silently strips household content.

## Validation

**Automated:** follow `docs/DEVELOPMENT.md`: configure/build Debug and run
affected tests, then integrated `ctest --preset debug --output-on-failure`.
Cover exact legacy decoding and canonical v10 round trips; invalid fields,
overlap and radio ownership; reader glyph/layout/controls and input release
gates; free/held/drop/throw identity; body/constraint/inner-player construction
failure; physics error propagation; rotated contacts and maximum-speed thin
walls/minimum-valid doors; player support/stance, actor waiting and door
restart policy; held-body wake after stillness; cursor-loss then suspension
with recapture; sleeping,
suspension and equivalent fixed-boundary commands. Compare same-build physical
runs initially within 1 mm position, 0.1 degree orientation and 0.01 m/s velocity;
retain diagnostics and adjust justified solver tolerances rather than demand
bit identity. Exact logical transitions and body/hold counts remain mandatory.
Run existing/new real ImGui UI tests via `ui_test_runner`, including household
commands, references, text editing and the saved-file process transaction.

**GPU:** run `ctest --preset vulkan-smoke --output-on-failure`, with household
runtime/editor paths covering no doors, maximum combined geometry, rotating
boxes and matching shadows, document/caption coexistence, resize/minimize,
out-of-date/suboptimal recovery, failed replacement and final teardown. Retain
readback images for box orientation/shadow agreement and 800x600, 1920x1080,
3840x2160 reader layout. Regenerate and validate any changed shader stages as
required by DEVELOPMENT; do not change the Vulkan feature baseline.

**Runtime verification and human acceptance:** the original desktop plan covered
the neutral physical interaction scene and an independent second scene authored
through the UI, save/reopen and ordinary Play. The independent authoring scenario
passed. On 2026-09-11 the user stopped further manual testing and requested
scripts/code checks. Use `scripts/check_household.ps1` for the remaining functional
verification: hold/drop/throw, wall/door/player/actor contacts, reading, radio/mute
and suspension/recovery. Its default run builds and tests without windows; GPU
readbacks remain a separate opt-in. Retain earlier desktop observations and the
coverage/evidence mapping in `validation.md`. This changes the verification
method, not the gameplay requirements. Do not resume manual execution or treat
automated success as subjective hold/throw feel or physical listening; those
remain explicitly unverified. On 2026-09-26 the user accepted closing P06/T3
using the retained evidence with these limitations. The decision is recorded
in `validation.md` separately from historical execution and does not establish
hands-on confirmation of the unverified qualities.

**Performance:** measure a separate Release build under the existing supported
1920x1080/60 Hz conditions, with no concurrent build or GPU work. Use a neutral
furnished scene and a matching zero-box baseline, then 16 awake boxes plus
representative actor/door/shadow activity. Record three runs per workload,
10 s warm-up plus 60 s capture, CPU/GPU timing and frame p50/p95/p99, including
hold/throw/settle costs. Retain raw data and hardware/configuration. Apply the
existing T1 gates (CPU/GPU p95 <=16.67 ms; frame p50/p95/p99 <=16.9/20/33.4 ms)
on that supported machine and preserve failed samples. Profile regressions
before introducing optimization complexity; unavailable hardware leaves this
acceptance open rather than passing by inference.
