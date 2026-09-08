# P07b continuation handoff

Updated 2026-09-08 after resuming the implementation. This file supersedes the
earlier 10/25-task handoff. Implementation and available automated/performance
verification are finished; **25/25 tasks are checked**. No project measurement
process remains active. The change was archived on 2026-09-08 and its seven
delta specs were then synced to main specs in the user's requested order.
No commit was made.

## Active request and approvals

- User request: `$openspec-apply-change add-scripted-characters — read CONTINUE.md
  in the change directory and resume`. Schema: `spec-driven`, local OpenSpec
  root. Apply skill: [.agents/skills/openspec-apply-change/SKILL.md](../../../../.agents/skills/openspec-apply-change/SKILL.md).
- Workspace: `D:\programming\near-laugh`, PowerShell, branch `main`.
  Original HEAD: `c868e967dc469ef208b3046bc7305f3d59fc02fa`.
  Existing and new changes remain uncommitted. Do not overwrite them or rerun
  ignored migration helpers. No subagents unless explicitly requested.
- Both earlier decisions are approved; **do not ask again**:
  1. Interact has one second of PCM/caption, at most 0.25 seconds of effect
     followed by silence. Full duration controls arbitration and completion.
  2. [The slope correction](slope-decision.md) keeps authored/render/audio feet
     on accepted ground, with a private support-derived capsule offset used
     by shared validation and runtime. Keep 50-degree slopes and 0.02 m arrival.
- AGENTS.md and VISION/ARCHITECTURE/GAMEPLAY/RENDERING/DEVELOPMENT were reviewed.
  Narrative horror remains the scope; no general engine or combat systems.

## Acceptance completed

On 2026-09-08 the user answered **"yes"** to finalizing tasks **5.3** and **5.7**
with physical listening and hardware latency explicitly unverified. Both tasks
are now checked and [handoff.md](handoff.md) is finalized. Do not request that
approval again. No human walk/listen session or physical output/display latency
measurement occurred. Automated tests and Vulkan readbacks cover routes,
obstruction/release, cancellation, shadows, captions and accepted source positions.
Offline PCM onset/drift is measured.

P07b is accepted with those explicit evidence limits; approval is not evidence
that unavailable observations happened. P07c's dependency review is done.
Archive and main-spec sync are finished; T2 stays open for P07c.

## Delivered implementation

- Strict v9 actors/marks/routes, catalog/reference/source-ownership checks,
  exact v2-v8 compatibility, canonical save and unchanged files on Open.
  Legacy v8 fixtures and deterministic preparation paths are retained.
- Privately owned actor capsules; once-per-step world -> player -> actors in
  durable-ID order -> doors. Swept support/steps/slopes, protected envelopes,
  initial/body allocation cleanup, no standing on actors, actor obstruction
  of E/R/knock including switches.
- Concrete start/cancel/route state, accepted-distance walk phase and contacts,
  turn/arrival/final marker behavior, foreground Busy retry and action-owned
  cancellation. Shared immutable assets and independent playback/palettes.
- Engine composition, ordinary authored initial routes, neutral one/four
  fixtures, and explicit `scripted_characters` controls: F5 restart, F6 cancel,
  P joint suspension, M mute. Cursor release keeps time; minimized waits drop it.
- Editor frozen initial poses, character persistence through edits/history,
  selected Play preflight before child creation, coherent failed replacement.
  Invalid routes retain visible red anchors at a surviving mark/default entry.
- Opt-in `scripted_character_measure` composes actual runtime subsystems with
  P07a lighting/camera and collision-valid moving lanes. CSV scopes separate
  route, collision, pose/contacts, world/player/doors, audio, deformation/upload.
  No hardware device callbacks are included in its offline audio workload.
- Current documentation, source provenance and P07c contracts are updated.
  No shaders changed and no speculative performance optimization was needed.

## Verification and retained evidence

Read the superseding top section of [validation.md](validation.md),
[performance.md](performance.md) and [handoff.md](handoff.md). Older sections of
validation.md are historical and deliberately preserve failures/decisions.

- Debug builds passed for engine_tests, near_laugh, scripted_characters,
  scripted_character_measure, level_editor, vulkan_smoke and the affected
  interior_lighting_visual. Release measurement configuration/build passed.
- Full deterministic selection was 408/409; one new test incorrectly expected
  a door result from a successful switch toggle. Corrected focused rerun 2/2.
  The final editor diagnostic regression and affected selection passed 12/12.
  **410 current checks have passing evidence across full/focused runs**; no
  single clean 410-test run is claimed.
- Full Vulkan selection was 11/12; rerunning captures collided with previous
  output names. Fresh per-invocation directories fixed it, runtime rerun 1/1.
  Final editor Vulkan checks passed 2/2. **All 12 cases have passing evidence**
  through teardown, with no accepted error-severity validation message.
- Prepared levels/audio/caption/provenance: six files regenerate byte-identically.
  Changed C++ was formatted. Strict OpenSpec validation and git diff --check pass;
  diff and relevant documentation were reviewed.
- Twelve verified lossless route/shadow/caption PNGs and contact CSV are in
  [evidence](evidence). Offline contact handoff plus onset max 83.7083 ms;
  schedule drift stays within one 60 Hz step. Physical listening and device/
  display latency are explicitly unavailable.
- **All nine valid Release runs pass every unchanged P07a gate**: three each
  for 0/1/4 actors, 10-second warmup plus 60-second samples, fullscreen
  1920x1080/60 Hz FIFO, validation off, eight lights/four D32 casters.
  Worst four-actor CPU p95 2.5013 ms, GPU p95 12.65376 ms, frame p95 17.7163 ms.
  All actors repeatedly traversed at least 15.0667 m and completed 20 legs.
- Raw CSVs are retained as exact gzip copies with SHA-256 manifest, summaries,
  logs and measurement PNGs under evidence/timings. Hardware matches P07a:
  Ryzen 5 4600H / Radeon Graphics / approximately 16 GB RAM / Balanced power.
- An interrupted second baseline at 32.767 s and startup-focus failures remain
  retained and excluded. Final samples came from:
  `build/scripted-character-release-timings/scripted-0-1.csv`;
  two baselines in `build/scripted-character-release-baseline-resumed`;
  six actor runs in `build/scripted-character-release-actors`.
  The manifest maps retained filenames to these sources. No sampling remains.

## Subsequent work

This change is archived at
`openspec/changes/archive/2026-09-08-add-scripted-characters`. Its 11 added and
six modified requirements were synced across all seven main specs, with every
delta compared afterward. Strict main-spec validation passed 28/28.
Implementation, acceptance and archive are finished. Do not repeat passed
builds/tests/timing runs without new changes or a specific concern. Preserve
the accepted evidence limits and T2's P07c requirement. P07c authoring remains
the next implementation stage when requested. Do not commit unless requested.
