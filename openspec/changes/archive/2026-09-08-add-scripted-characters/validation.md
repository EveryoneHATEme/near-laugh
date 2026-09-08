# P07b implementation and validation

## Resumed implementation review (2026-09-08)

This section supersedes the historical implementation/handoff status below.
Route, audio, Engine and initial editor compatibility are implemented. The
approved one-second cue and support-derived slope offset remain unchanged.
All nine Release samples pass the unchanged P07a timing gates. Physical listening
and hardware output latency remain unavailable. On 2026-09-08 the user answered
"yes" to finalizing tasks 5.3 and 5.7 with those observations explicitly
unverified. P07b is accepted with that limitation and all 25 tasks are checked.
This approval does not establish human listening or measured hardware latency.
The [handoff](handoff.md) is finalized; T2 remains pending P07c authoring acceptance.
On 2026-09-08 the user requested archive followed by sync. The change was moved
to `openspec/changes/archive/2026-09-08-add-scripted-characters`; all 103 files,
including `.openspec.yaml` and retained evidence, kept their hashes during the
move. The subsequent merge added 11 requirements and modified six across seven
main specs, preserving unrelated requirements and scenarios. Every archived
delta was compared with the result; `openspec validate --specs --strict` passed
all 28 main specs. Documentation links were adjusted for the archive location.
Only specs and documentation changed during archiving; runtime checks were not
repeated. No commit was made.

### Functional verification

- Debug configuration and builds passed for `engine_tests`, `near_laugh`,
  `scripted_characters`, `level_editor`, `vulkan_smoke` and the new opt-in
  `scripted_character_measure`. Release configuration and the measurement build
  also passed. New files and touched C++ ranges were formatted with Clang Format.
- The complete deterministic selection passed 408/409 in 71.20 s: 398 unit,
  six boundary and five process checks. Its only failure was a new switch test
  incorrectly expecting a door-result optional from a successful switch toggle;
  the light changed correctly. After correcting that assertion, the switch and
  contact-timing rerun passed 2/2. All 409 checks therefore have passing evidence.
  Final editor review added an orphan-reference diagnostic-anchor regression:
  the affected definition/editor selection passed 12/12 in 2.35 s. The resulting
  410 checks have passing evidence across the full run and focused corrections;
  this is not a claim that a single clean 410-test run was performed.
- The complete Vulkan selection passed 11/12 in 56.44 s. The character smoke
  could not overwrite previously retained captures. Giving every invocation a
  fresh capture directory fixed that repeatability defect; its rerun passed in
  24.77 s. All 12 Vulkan cases have passing evidence through teardown. Existing
  overlay-layer API-version warnings remain visible; no error-severity Vulkan
  message was accepted. After the final editor correction, the editor document
  and scripted-character Vulkan cases passed again, 2/2 in 6.25 s.
- The actor smoke composes missing-selected-mannequin, third-actor physics and
  character-index-upload failures with fresh successful startup afterward. It
  exercises one/four routes, accepted placement, door release, final marker,
  explicit cancel/restart/pause, inactive input, minimize/restore and recovery.
  Editor smoke verifies frozen initial palettes, unrelated edit/save/undo,
  coherent stale-preview retention and no child on failed selected Play preflight.
  Orphan routes now retain a red viewport anchor at their first surviving mark,
  or the default entry if none survives; malformed model/route references remain
  visible without rewriting the opened file.
- The added distance-drive regression preserves the displayed blend source and
  saved phase through interruption. Audible offline, mute, silent and simulated
  device-loss route runs retain identical contact and cue-instance identities;
  nonmuted offline runs contain PCM energy. Effective source gains follow accepted
  actor feet even when authored source coordinates deliberately differ.
- Prepared levels, two WAVs, caption and provenance regenerate byte-identically
  (six files). Strict `openspec validate add-scripted-characters --strict` and
  `git diff --check` pass. No shader source or SPIR-V changed.

The full runs and their focused corrections are retained in
[evidence/logs](evidence/logs). Historical failing route runs remain distinguishable
from passing evidence; the new measurement-lane assertion initially omitted
the allowed 2 cm arrival tolerance at both ends, then passed after correction.

### Captures, contact timing and practical limits

[Twelve lossless Vulkan readbacks](evidence/captures/captures.json) retain
initial/walking/door-blocked/interacting/completed/caption views for one and four
actors. PNG retention verified exact stored RGB and recorded SHA-256 hashes.
These are real Engine poses, with accelerated simulation for functional checks;
they are not output-latency measurements. Final-action captures use a close
inspection camera beyond the doorway so the jamb cannot hide the gesture.

Reviewed observations: the actor visibly climbs the three risers and waits at
the closed leaf; after opening it reaches and faces the final mark. Character
silhouettes and projected shadows change together, while the door shadow follows
the accepted opening. The close marker view shows the left-hand reach and the
Russian caption `Манекен: Тихий щелчок.`. The four-actor initial/walking views show
independent placement and the deliberate opposing wait. The source mannequin's
2.654 cm sole dip, standing yaw pivot and capsule-versus-limb limits persist.
No foot IK, animated limb collision or automatic deadlock avoidance is claimed.

[The contact trace](evidence/contact-timing.csv) records 11 contacts at 1.5 m/s
over five simulated seconds in six-step/100 ms batches. Delay from the fixed
step that crosses a contact to batch handoff ranges from 0 to 83.3333 ms.
The first stereo PCM frame exceeding amplitude 0.00001 occurs 0.375 ms after
each handoff, giving a maximum combined software delay of 83.7083 ms. Contact
time differs from the ideal calibrated distance schedule by 0.142–14.743 ms,
within one 60 Hz step without accumulating drift. This measures the real offline
mixer, with no hardware output buffering or display scanout.

Physical headphone/speaker listening, device-loopback latency, and simultaneous
display/audio capture are unavailable in this agent session. Player/static/prop
blockage, actor swaps, cancellation and release are covered by deterministic
behavior tests; this record does not describe those automated cases as a human
playthrough. The user accepted P07b with those observations unavailable on
2026-09-08; the limitation remains in the accepted record.

### Measurement setup and retained interruptions

The opt-in measurement composes actual PhysicsWorld, player, routes, door state,
animation and CueCoordinator with the P07a renderer. It retains the packaged
capacity level's geometry, all entries, eight lights/four D32 casters, initial
doors, disabled flashlight, fixed camera eye (-3.15,5.65,5.85) toward
(-3.15,3.8,2.7), and fullscreen 1920x1080/60 Hz FIFO. Visual-only P07a placements
are replaced by validated short lanes at X=-4.5/-2.2, with Z=2.95–3.70 and
1.60–2.35, Y=3. Every measured actor repeatedly traverses out/back legs and
performs Interact. The program rejects insufficient accepted travel/completions.

The same zero-actor scene supplies the light-only comparison. Audio uses the
real 48 kHz offline mixer instead of hardware callbacks; caption overlays are
excluded from performance presentation to preserve P07a's color workload.
New per-frame scopes record route decisions, actor collision, pose/contact work,
world/player/doors and audio, alongside deformation/upload, GPU frame/shadows
and presentation waits. Ordinary runtime callers supply no timing storage.

Three short Debug and three Release zero/one/four checks passed at 1920x1080,
including timing-query recovery and measurement-view readback. Initial checks
lost fullscreen availability before their first submitted frame; draining and
restoring queued startup focus notifications before timing corrected that case.
The first long zero-actor sample completed and passed all gates. The second was
interrupted at 32.767 s when its framebuffer became unavailable; its partial
CSV/log is retained and excluded from valid samples. Two replacement baselines
and all six actor samples then completed in fresh output directories. No
concurrent builds/tests or other project GPU work ran during performance sampling.

[performance.md](performance.md) records all nine runs, per-scope costs, hardware
and retained CSV hashes. Every CPU/GPU p95 and frame p50/p95/p99 gate passes.
The largest four-actor values are CPU p95 2.5013 ms, GPU p95 12.65376 ms and
frame p95 17.7163 ms. Each actor traversed at least 15.0667 m and completed at
least 20 legs. No performance optimization or gate reduction was needed.

### Commands used in this continuation

The following commands produced the retained results; log redirection is omitted
for readability. Full-suite failures and successful focused corrections are
retained separately in [evidence/logs](evidence/logs).

```powershell
cmake --preset debug
cmake --build --preset debug --target engine_tests near_laugh scripted_characters scripted_character_measure level_editor vulkan_smoke -j 4
ctest --preset debug --output-on-failure
ctest --preset debug -R '^(ActorPhysics.ActorObstructsSwitchWithoutBecomingAnInteractionTarget|CharacterRoutes.ContactHandoffMeasuresBatchDelayAndOfflineOnsetWithoutDrift)$' --output-on-failure
ctest --preset vulkan-smoke --output-on-failure
ctest --preset vulkan-smoke -R '^vulkan_scripted_characters$' --output-on-failure
cmake --build --preset debug --target engine_tests level_editor interior_lighting_visual -j 4
ctest --preset debug -R '^(CharacterDefinitions\.|CharacterContent.Editor|EditorOverlay)' --output-on-failure
ctest --preset vulkan-smoke -R '^(editor_scripted_characters|editor_vulkan_smoke)$' --output-on-failure
python -B scripts/prepare_scripted_characters.py
python -B scripts/prepare_scripted_character_audio.py
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-measure-debug-desktop-restored -Check -DebugBuild
cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/p10-release --target scripted_character_measure -j 4
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-measure-release-check -Check
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-character-release-timings
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-character-release-baseline-resumed -Counts 0 -Repeats 2
.\scripts\measure_scripted_characters.ps1 -OutputDirectory build/scripted-character-release-actors -Counts 1,4
openspec validate add-scripted-characters --strict
git diff --check
```

The first long measurement command stopped on its second baseline. Its first
sample and the eight later samples form the nine valid runs. The manifest maps
their retained names to original paths. Preparation was checked by comparing
SHA-256 hashes before and after regeneration. No shader validation was needed
because shader sources and binaries are unchanged.

## Prerequisite review (2026-09-08)

P07a is archived at `../2026-09-08-add-character-animation`.
Its main character-animation specification and indexed-validation record agree
with this change: four selected instances, independent CPU poses, shared
immutable indices, and fenced changing vertex buffers. All nine recorded
indexed Release runs passed P07a's gates; P07b requires fresh moving-route
measurements. The baseline level codec is v8, including P10 lights and P04 audio.
The planning artifacts already reference that archived handoff and need no
requirement changes before implementation.

The generated test-mannequin catalog supplies a 0.25 m radius / 1.85 m high
capsule, +Z forward, 1.3061969054512663 m walk cycle, contacts at phases 0/0.5,
and interaction phase 0.36666666666666664. At 1.5 m/s, contacts are 0.435399 s
apart. Retain the measured 2.654 cm sole dip and capsule/limb limitations during
floor/stair acceptance. Catalog metadata does not establish route acceptance.

## Implemented data foundation

Tasks 1.1–1.5 are implemented. The document and immutable world handoff retain
bounded character records, metadata validation and endpoint placement checks.
Standing clearance uses the existing conservative world bounds/support helpers
with catalog capsule dimensions. Endpoint validation does not certify route
traversability. Character physics, actions, runtime/editor presentation and
selected content preflight are not implemented yet; no movement acceptance is
claimed. P07c/T2 remain pending.

Current saves emit strict canonical v9. Exact v2–v8 input shapes remain readable
without modifying their source, and normalize to empty character arrays. Original
prototype, P04 audio and capacity-lighting v8 bytes are retained in
`tests/fixtures/levels`. All five packaged levels preserve every previous
semantic value, adding only v9 and empty character collections.

## Commands and results

Configuration and builds used the existing Clang/Ninja Debug preset. Sandbox
execution initially refused the installed Ninja executable; approved desktop
execution succeeded. An initial attempt to build `editor_vulkan_smoke` found no
such build target; that is a CTest name, and its executable target is `level_editor`.

```powershell
cmake --preset debug
cmake --build --preset debug --target engine_tests near_laugh level_editor -j 4
ctest --preset debug --output-on-failure
ctest --preset debug -R source_boundary_check --output-on-failure
cmake --build --preset debug --target vulkan_smoke level_editor -j 4
ctest --preset vulkan-smoke -R '^(vulkan_smoke|vulkan_interior_apartment|editor_vulkan_smoke)$' --output-on-failure
openspec validate add-scripted-characters --strict
git diff --check
```

The first focused run passed 60/61 checks and caught an old editor source-version
expectation, corrected from 8 to 9. The full deterministic run then passed all
364 unit tests, including nine new character-definition cases, and all five
process tests. Five of six boundary tests passed; the JSON boundary check needed
an explicit test-only allowance for malformed level fixture generation, matching
the existing animated fixture allowance. Its rerun passed, retaining the private
production codec boundary. Logs: `build/scripted-characters-deterministic.log`.

All three selected Vulkan tests passed with validation: ordinary prototype,
apartment entry, and editor document/recovery smoke. They exercise migrated
existing scenes, not moving characters. Log: `build/scripted-characters-vulkan.log`.
No shaders changed. Strict OpenSpec and whitespace checks passed.

Regenerated `level_characters_v9.py`, `level_lighting_v8.py`,
`prepare_audio_level.py` and `prepare_interior_lighting.py` sequentially. SHA-256
comparison of all five packaged levels before/after was identical. JSON comparison
against `git show HEAD:resources/levels/<name>.level.json`, after removing empty
characters and restoring the version field to 8, matched every prior value.
Opening/explicit-save migration and character-bearing canonical round trips are
also covered by the deterministic tests. Reviewed the implementation diff and
new files; architecture/development descriptions identify delivered data support
and pending movement work.

## Approved timing resolution (2026-09-08)

P07b's Character sound ownership and markers requirement caps the essential
captioned interaction cue at 0.25 seconds. The accepted
`openspec/specs/game-text-presentation/spec.md` requires each caption segment to
last at least one second and stay within the selected clip. The existing
`readCaptions` preflight enforces both constraints. No cue can satisfy both.

The user approved this resolution: keep the audible interaction effect at most 0.25 seconds,
package it in a one-second cue with trailing silence, and retain a one-second
caption. Preserve the existing caption minimum. The full one-second cue governs
foreground occupancy and logical completion. Approval is already given; do not
request it again after resetting the conversation context.

The resumed session reconciled the proposal, design, scripted-character sound
requirement and task 4.1 with that approval. Strict validation passed. Audio
assets and content checks remain unimplemented; the timing approval needs no
further confirmation.

## Resumed physics implementation (2026-09-08)

Tasks 2.1 and 2.2 are implemented, bringing progress to 7/25. Engine and other
fixed-step callers now advance the shared world explicitly before player
movement. Participant movement no longer calls PhysicsSystem::Update.
The existing player/physics/door/fixed-step selection passed all 72 tests,
including zero-step and multiple-step batches. Raw log:
`build/scripted-characters-shared-step.log`.

PhysicsWorld privately owns the selected catalog capsules and releases them
before world/library teardown. Tests cover zero/four actor poses, stationary
retention, and failure after every actor creation followed by a fresh world,
repeated twice. Task 2.3 is partial: continuous casts, grounded landing and
up/forward/down stair checks pass thin-wall, ordinary stair ascent/descent,
over-height step and unsupported-gap cases. Player/actor envelope checks and
door envelope protection are present but task 2.4's ordering/coordination and
acceptance coverage are not delivered. Task 2.5 is still unimplemented.

The new slope regression exposes a design issue: on a validated 49-degree
terrain slope, the nonpenetrating upright capsule settles 0.131058 m above the
authored ground anchor, exceeding the required 0.02 m arrival tolerance.
Separating initial slope contact permits traversal but does not solve the
anchor mismatch. The failing requirement test remains enabled, and task 2.3
is unchecked. See [the concrete proposed revision](slope-decision.md). Its
support-derived collision offset is proposed only; it has not been approved or
implemented. This issue is distinct from the resolved sound timing decision
and from the still-pending player non-support integration.

### Resumed validation commands and outcomes

```powershell
cmake --preset debug
cmake --build --preset debug --target engine_tests near_laugh level_editor vulkan_smoke interior_lighting_visual -j 4
ctest --preset debug -R '^(Physics|Player|Door|FixedStep|AuthoredInteraction)' --output-on-failure
ctest --preset debug --output-on-failure
ctest --preset vulkan-smoke -R '^(vulkan_smoke|vulkan_interior_apartment|editor_vulkan_smoke)$' --output-on-failure
ctest --preset debug -R '^ActorPhysics\.' --output-on-failure
openspec validate add-scripted-characters --strict
git diff --check
```

Debug configuration and all affected target builds succeeded. One initial
test compilation used the wrong fixture enum `Step`; it was corrected to
the existing `WalkableStep` before execution. The full deterministic run
passed 380/381 checks: 369/370 unit tests, all six boundary checks and all five
process checks. Its only failure is the deliberately retained, unsatisfied
slope requirement described above. Raw log:
`build/scripted-characters-resume-deterministic.log`.

After adding an explicit validated destination mark to that regression and
rebuilding, the actor suite again passed five of six cases with the same
0.131058 m slope-arrival failure. No runtime behavior changed after the full
run. Log: `build/scripted-characters-resume-actor.log`.

All three selected Vulkan checks passed through teardown; retained CTest log:
`build/scripted-characters-resume-vulkan.log`. These cover the existing empty-
actor packaged scenes, not moving actor presentation. No shaders changed.
Strict OpenSpec validation and whitespace checks passed. Reviewed the resumed
implementation, tests and documentation diff; earlier foundation changes
remain intact. Full route visuals/listening, hardware latency, moving actor
Vulkan acceptance and Release measurements remain unperformed.

## Latest implementation and context handoff (2026-09-08)

This section supersedes the earlier pending-approval/implementation status.
The user approved the slope correction with "yes, proceed". Shared initial
validation and runtime now distinguish ground feet from actual capsule offset;
the slope/arrival regression passes. Actor collision tasks 2.1-2.5 are checked,
bringing checkbox progress to 10/25. Route/controller/audio/Engine/editor work
has also been implemented substantially; its task completion review is pending.
See [CONTINUE.md](CONTINUE.md) for precise code state and next steps.

Fifteen ActorPhysics tests passed, including slopes and flat transitions,
stairs/over-height risers, gaps, player stance/non-support, actor ordering and
crossings, interaction obstruction and opening/closing doors. Route tests
subsequently exposed tiny stationary support rounding and an opening-away door
blocked by conservative sweep margins. Both implementation defects were fixed;
the door release regression passed after 1 mm actor clearance and bounded
subdivision that proves both swept half-intervals clear.

Debug engine_tests, near_laugh, scripted_characters, level_editor and vulkan_smoke
were built. The latest full deterministic run passed **398/400**; its failures
were a caption-font test assuming every catalog sound has captions and an
incidental source-order assertion for minimized input. Both were corrected and
their focused reruns passed. Retained full log:
`build/scripted-characters-integrated-deterministic.log`.

`build/scripted-characters-fixtures.log` retains a later 17/19 selection: only
two new packaged-fixture tests failed because the preparation script spelled
the solid kind `walkable-step` instead of `walkable_step`. After correcting and
regenerating, the packaged route and editor round-trip tests both passed (2/2,
3.50 s) in a direct rerun. Older route failure logs remain at
`build/scripted-characters-routes.log` and
`build/scripted-characters-door-release.log`; these are not passing-run records.

Desktop Vulkan command:

```powershell
ctest --preset vulkan-smoke -R '^(vulkan_scripted_characters|vulkan_audio_runtime|editor_vulkan_smoke)$' --output-on-failure
```

All three passed with validation through teardown: audio runtime 5.48 s,
scripted characters 24.52 s, existing editor smoke 4.98 s; total 35.10 s.
Log: `build/scripted-characters-runtime-smoke.log`. The new Engine test exercises
one/four actors, accepted render placements, operated door, interaction marker,
recovery, explicit controls and joint minimized suspension. Accelerated fixed
step sampling makes this functional coverage, not visual/audio latency evidence.

After that run, editor diagnostic markers and a new `editor_scripted_characters`
smoke were added. The latest editor build passed, but this new smoke has **not
been run**. It is the immediate next validation task in CONTINUE.md. No final
full deterministic/Vulkan run has been performed after all latest additions.
No shader changed. `git diff --check` passed at handoff; final formatting, strict
OpenSpec validation and complete diff review remain. Captures/listening/output
latency and Release route timings remain unperformed. No project/build/test
processes remained running when the user requested context preservation.
