# P03 implementation validation

Recorded on 2026-09-06. This change was implemented together with
`add-authored-scene-assets` (P02), with P03's door contract composed first.
The final canonical writer is v6. Exact v2/v3/v4 inputs normalize to empty
doors; the exact v5 reader preserves authored doors while P02 normalizes its
legacy static prop and materials. Runtime door state is never serialized.

## Implemented contracts

- `DoorDefinition` belongs to immutable world data. It retains the authored
  ID, hinge, closed yaw, dimensions, signed opening angle, angular speed,
  initial endpoints/lock, and closed-frame lock side. There are at most 32
  definitions. Shared helpers derive leaf transforms, corners, rays,
  overlap tests, terrain overlap, and generated presentation.
- `DoorController` owns run-local angle, intent, lock, and one short feedback
  state per door. E requests the opposite endpoint at an endpoint and reverses
  intent strictly between endpoints. R changes a closed stationary lock only
  from its authored side. RMB knocks without changing motion or lock.
  Results identify the door and result kind; feedback consumes them directly.
- `AuthoredInteraction` alone owns E/R/RMB release latches and ray arbitration.
  It consumes each sampled batch, including inactive/minimized batches.
  Priority is R, E, then RMB, with all simultaneous edges consumed and no
  fallback after refusal. Selection first finds the absolute minimum distance
  within the inclusive two-metre reach, then selects the stable door ID/type
  among candidates within 0.1 mm. A switch follows doors in a distance tie.
- `PhysicsWorld` owns zero-velocity kinematic leaf bodies. Each fixed step
  updates the player first, then accepts door motion in durable-ID order.
  Angular intervals are at most one degree. A zero-convex-radius midpoint
  query box contains both endpoint boxes and the intervening arc using a
  horizontal sagitta margin. A blocked interval is refined to at most 1 mm
  unresolved tip travel, with at most twelve refinements. Only verified clear
  motion is installed; a stopped request never retries automatically.
- Motion checks all solids, terrain triangles, all P02 prop collision boxes,
  other accepted leaves, and the previous/current player envelope. Standing,
  crouched, and changing stance use the larger height. The shared 2 cm contact
  padding is explicitly assigned to Jolt and included in motion/initial entry
  clearance: radius plus padding, height plus twice padding. Yaw sweep query
  margins enlarge horizontal axes only, preserving authored floor clearance.
- Visibility includes accepted leaves, rejects origins inside any leaf, and
  permits only the selected door's own target surface. Rendering, targeting,
  and collision use the same current leaf pose; the player keeps its existing
  interpolation. Conservative checks can stop at recently vacated space.
- `FrameRequest::opaque_boxes` borrows at most 192 backend-neutral boxes for
  the synchronous render call. Six boxes per door provide leaf, handles, bolt,
  and knock plates with the procedural obstacle material alias. This does not
  extend the existing 128-byte push constant block. Feedback lasts 0.3 seconds
  of simulation time, and new feedback replaces the previous transient.

## Automated coverage

The final combined debug build and all **281/281 debug CTest cases passed**
(37.34 seconds), including the final capacity/feedback, editor overflow, and
fresh saved-file preflight regressions. The parent integration pass ran:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Relevant cases are in `tests/core/test_doors.cpp`,
`test_physics.cpp`, `test_light_switch.cpp`, `test_scene_authoring.cpp`, and
`test_interior_level.cpp`.

Coverage includes:

- Authored bounds and duplicate IDs; both opening signs; initial transforms;
  32-door/192-box capacity; initial terrain/structure/prop/entry obstruction;
  an unselected entry; upper floors; raised capsule/contact skin; byte-stable
  round trips and unchanged source bytes through runtime motion.
- Initial and moving leaf visibility; self-surface targeting, inside origins,
  endpoint obstruction, and leaves behind a target; a thin obstruction missed
  by endpoint-only checks; a terrain mound and a second yawed prop box;
  maximum speed/dimensions and both 170-degree swing directions; deterministic
  meeting leaves independent of storage order; partial startup failure after
  multiple leaf bodies and successful repeated recovery.
- Standing/crouched obstruction without pushing; changing stance and refused
  standing beneath a closed leaf; diagonal motion and vacated-space safety;
  obstruction with no progress; endpoint-first retry; no automatic resume;
  motion reversal, bolt-side lock/refusal, and knock without pose changes.
- Held startup/missed/inactive actions; cursor transitions, simultaneous
  priority and unsupported nearest actions; inclusive reach and storage-order
  independence; stable minimum-distance tolerance; zero/one/several simulation
  steps, the 100 ms active-stall cap, minimized timer pause and held input,
  renderer outcomes, flashlight independence, and restart. Former switch
  tests now exercise the actual shared dispatcher, not a second input path.
- Distinct refusal/knock presentation; feedback replacement/expiration without
  changing collision; rejected overflow edits for model bounds and yawed proxy
  bounds with unchanged editor history/document/file; repairable missing model
  references; both standing apartment routes after actual door operations.

The first terrain sweep fixture was rejected by the existing 50-degree terrain
slope rule. Reducing its isolated grid sample to 0.25 m on the 0.5 m grid kept
the intended intervening blocker and made the fixture valid. The corrected
door-world/door-physics/packaged-traversal targeted run passed all 15 cases.

`git diff --check` passed for the door runtime, physics, interaction/input,
frame boundary, and owned tests. `openspec validate add-interactive-doors
--strict` passed after the design was aligned with the final result fields
and contact-padding behavior.

## Packaged route and GPU acceptance

The room door is `lena-room`, hinge `(-1.12, 3.02, 3.07)`, width 1.26 m,
closed yaw -90 degrees, open angle -90 degrees, and positive-Z lock side.
The initial 1 cm residual jamb gap caused the conservative sweep to stop
before the full endpoint. The final approximately 4 cm jamb gap permits the
full swing without weakening collision acceptance. Both standing route tests
pass through the open doorway near Z=3.5 and around the kitchen chair via
Z=-1.9; no jumping, crouching, or movement-policy change is required.

The switch plate is at `(-0.721, 4.3, 4.05)`, yaw -90 degrees, on the post
at `(-0.65, 3.7, 4.05)`. Its view from the room is blocked by the closed door.

All **7/7 Vulkan smoke cases passed** in 21.80 seconds on AMD Radeon(TM)
Graphics with validation enabled:

```sh
ctest --preset vulkan-smoke --output-on-failure
```

This includes changing door buffers across both frame slots, empty/present
streams, resize/minimize/recovery, partial construction failures, editor
initial open/locked previews and terrain retention. Actual recorded editor
mesh draws precede its ImGui pass. No unexpected error-severity validation
messages occurred; the deliberate validation-error case failed as required.

After the full debug run, desktop inspection found two stale summary labels
in the editor (one chair and v4 save). They now display actual prop/door counts
and level_format_version. The final full build and all 10 affected EditorUi
tests passed again. No other behavior changed after the 281-case run.

## Desktop observations and remaining acceptance

The real game window was driven through Win32 keyboard/mouse input. The
original packaged level and temporary copies with additional clear inspection
entries were used; original authored files were never saved by these runs.
Screenshots are local inspection artifacts under build/:

- p02-p03-door-closed/locked/refusal/knock/open.png: room-side R moves the bolt;
  locked E retains the closed leaf with a distinct refusal tint; RMB shows a
  short knock plate without opening; unlocking then E opens the leaf.
- p03-outside-lock-refusal.png and p03-moving-reversal.png: the corridor side
  cannot operate the bolt; a second E during opening returns the leaf closed.
  p03-outside-walk-through.png records walking through the open doorway.
- p03-player-blocked-open.png and p03-player-blocked-close.png: a standing
  player inside the respective swing arc stops either direction without being
  moved or seeing through the leaf. p03-no-auto-resume.png retains the stopped
  pose after stepping back; two explicit endpoint requests then permit the
  clear full opening (p03-cleared-retry.png).
- p03-switch-occluded.png: a locked leaf refuses the action aimed toward the
  switch. p03-switch-initial-on.png / p03-switch-open-off.png show that the
  reachable plate toggles its light with the leaf already open.
  p03-switch-off-flashlight.png shows the independent flashlight afterward.
  Open from the apartment start before standing at (-2.3, 3, 4.05): that latter
  switch-view point lies in the intervening swing and correctly blocks opening.
- p03-editor-door-properties.png: the flat object list exposes the door ID,
  hinge, geometry, speed, side and initial-state fields, with hinge/arc guides.
- Restarts restored authored closed/unlocked doors and light state. Repeated
  runs with authored-open inspection copies started open without prior locks.

At the earlier automated/desktop audit, task 7.3 remained open: the full manual matrix was not completed. In particular,
the single-fixed-step vacated-space stop is covered deterministically but was
not visually resolved; the outside unlock refusal was not separately exercised
with an initially locked copy. Complete walking routes from both starts are
verified by deterministic physics/player/door tests. Desktop input exercised
the doorway, kitchen and upward stairs separately, but did not produce a
reliable full end-to-end route record (large OS mouse moves affected turns).
These limits do not replace or weaken the specified acceptance criteria.

## Handoff

P03 passed strict OpenSpec validation, its ten capability deltas were merged
into the main specs and verified, and it was archived before the P02 sync.
P02 then extended the shared requirements to the final v6 profile. This order
preserves P03's reviewed baseline without rewriting it around its successor.

## User acceptance on 2026-09-06

After the remaining manual checks were explicitly listed in the conversation,
the user confirmed that they had checked the changes, were satisfied, and
requested closure of P02 and P03. This closes manual acceptance task 7.3.
This is user-reported acceptance; no new agent-observed screenshots or
per-scenario measurements are claimed. The earlier audit limitations above
remain a historical record of the agent-run checks.
