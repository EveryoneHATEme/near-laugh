# P07a → P07b handoff — accepted indexed presentation

This records the delivered `add-character-animation` contracts for
`add-scripted-characters`. **P07a is accepted: all nine indexed Release runs pass
the unchanged gates, including four characters, after functional and visual
validation.** The original [validation record](validation.md) retains the failed
expanded baseline; [indexed validation](indexed-validation.md) records the final
commands, observations, hardware, timings and limits. Archiving/sync remains a
separate workflow. P07b must still rebase on the resulting main specs, and P07a
does not establish complete T2.

## Prepared model and measured metadata

The selected model is `test-mannequin`, with exactly `idle`, `walk` and
`interact`. The committed derivative and included CC0 notice work independently
of `build/p07-assets`; source archives are regeneration inputs only. The
[preparation manifest](../../../../resources/characters/manifest.json) records
source and derivative identities, while the
[asset workflow and calibration](../../../../resources/characters/README.md)
explain the measured values and retained independent pose references.

| P07b input | Delivered value |
| --- | --- |
| Feet origin / forward | `(0, 0, 0)` / `+Z` |
| Scale / upright proxy | Scale 1; capsule radius 0.25 m, full height 1.85 m |
| Conservative preview bounds | `(-1.05, -0.10, -0.55)` to `(1.05, 1.90, 0.80)` m |
| Walk cycle distance | 1.3061969054512663 m |
| Left/right heel contacts | Normalized phases 0 and 0.5 |
| Interaction marker | Left-hand maximum forward reach, phase 0.36666666666666664 |
| Supported route speeds | 0.25–1.5 m/s; alternating contacts at least 0.435399 s apart |

The generated `CharacterCatalogEntry` exposes these values without mesh decoding
or backend types. Its full capsule height includes both hemispheres; P07b must
convert it appropriately when constructing the private physics shape. Metadata
is derived from the selected source motion and is not collision acceptance.
The minimum contact spacing exceeds P07b's 0.20 s requirement and its planned
0.15 s footstep duration cap.

The source soles dip **2.654 cm below the authored feet plane**. P07b must inspect
this on floors and stairs and retain the observed limitation. The proxy excludes
animated limbs and does not establish hand/arm clearance at an interaction mark.
There is no turn clip; standing idle plus placement yaw remains P07b's planned
turn behavior, with visible foot pivoting to be assessed in its route fixture.
Superhero retargeting, final art and new material features are not delivered.

## CPU animation and presentation handoff

`near_laugh_animation` owns bounded CPU decoding, local TR sampling, global pose
evaluation and deformation. Static and animated decoding share one compiled
cgltf owner while retaining separate accepted profiles. The animation interface
uses project scalar/container types and does not depend on Vulkan, physics or
audio. Independent playback owners share `shared_ptr<const CharacterAsset>`.

Local poses use asset node order, including constant skeleton ancestors. A
`CharacterPose` contains column-major model-relative global matrices in skin
joint order. `CharacterPoseFrame` borrows those globals synchronously and supplies
a render-instance handle, the loaded asset's skeleton identity, world position
and yaw in degrees. Do not premultiply inverse binds or world placement into
the palette: deformation applies both once. The renderer retains immutable
selected assets, validates the complete zero-to-four instance/pose set and never
advances a clip or owns actor/route identities.

`CharacterPlayback` supports looping idle/walk, clamped interact with one-shot
completion, explicit pause/seek/restart and captured 0.15 s transitions, including
interruption. Its current viewer-oriented commands start a newly selected clip
at time zero; `seek` samples directly and ends any transition. `advance` uses
the same elapsed value for clip and blend clocks.

P07b task 3.3 must therefore connect accepted-distance walking to the CPU pose
boundary explicitly. Calling `selectClip("walk")` and then `seek(saved_time)`
does not provide its specified blend back to the saved walking phase. P07b must
support a target sample driven by accepted travel while blend time advances by
fixed steps, preserving the saved phase through idle/blockage and interrupted
transitions. This is the concrete playback integration required by its existing
design; it is not a route capability already established by the P07a viewer.
P07b also owns contact crossings, interaction-marker holds/retries and cue
instances; inspection seek and one-shot completion do not generate those events.

## Rendering, shadows and replacement

`RendererResources.characters` selects shared immutable assets by render handle.
One CPU deformation per submitted instance feeds the existing color pipeline
and all enabled point-light shadow faces. The implementation retains both
primitive colors, finite normalized normals and off-screen shadow silhouettes.
No collision capsule substitutes for rendered triangles. The existing eight
point-light/four-caster, ambient and independent flashlight limits remain.

Changing character buffers are independent of static and door geometry. The
two frame slots each own mapped coherent storage, rewritten only after their
fence. Zero selected characters require no character resources. Borrowed frame
data does not survive the render call; immutable assets and character GPU
resources survive swapchain and attachment-format recovery. `Rendered`,
`Skipped` and `Recovered` do not mutate caller playback state.

`EditorRenderer::replaceDocument(level, instances)` builds a complete candidate
scene/lighting/character set and retains the prior set on failure. P07b's editor
composition must keep the old matching pose selection until replacement succeeds
and report a stale preview after failure. P07a provides that rendering boundary;
v9 persistence, initial actor definitions and selected character/audio Play
preflight remain P07b work. The current level baseline remains v8.

## P07b dependency review

The existing P07b proposal, design, tasks and delta specs were reviewed against
the delivered implementation. Their model identity, capacity, scale/proxy,
calibrated phases, in-place movement, idle/yaw turning, supplied-pose rendering,
shadow coherence and v8 starting baseline agree with P07a. No P07b planning
files were edited during this review.

| P07b work | Dependency and remaining responsibility |
| --- | --- |
| 1.1–1.4 definitions/preflight | Consume the generated catalog after P07a acceptance/archive; add v9 references, support and initial-overlap validation. |
| 2.2–2.5 collision | Use the upright catalog proxy; implement accepted movement, player/door obstruction and query ownership independently of rendered triangles. |
| 3.3 travel/animation | Retain accepted-distance phase with a separately advancing blend clock as described above; verify blockage/resume behavior. |
| 4.1–4.2 actor audio | Use calibrated phase crossings and P04 cue arbitration; prepare and validate actual short sound/caption assets. |
| 4.3–4.5 composition/editor | Own accepted placement, clocks and resource/pose lifetimes; add v9 retention and selected Play preflight. |
| 5.2–5.4 acceptance | Recheck animated/door shadow coordination and measure moving routes with physics/audio under P07a comparison conditions. |

P07a measures fixed placements with independent animation in the packaged
eight-light/four-caster interior, compared with the same light-only scene.
Each zero/one/four setup requires three Release runs with 10 s warm-up and 60 s
sampling at 1920×1080/60 Hz. Retain raw active CPU, deformation/upload, GPU whole
frame/shadows, frame and separate wait costs. Existing gates are CPU/GPU p95
≤16.67 ms and frame p50/p95/p99 ≤16.9/20/33.4 ms in every run. These measurements
do not include P07b route physics or audio and do not replace its measurements.
Any failed or unavailable measurement remains explicit in `validation.md`;
do not reduce capacity or infer acceptance from passing functional tests.

All nine original expanded comparisons are retained in
[the timing summary](evidence/timings/summary.json). Zero/one-character runs
pass every gate. Four-character GPU p95 is 16.923–17.013 ms; frame
p50/p95/p99 is 33.332–33.342 / 34.047–34.122 / 34.324–34.455 ms. The CPU p95
gate passes at 5.620–5.948 ms. That measured capacity failure remains part of
the record; it motivated the indexed follow-up rather than being overridden by
functional tests.

The user authorized the indexed revision of design decision 3 on 2026-09-08.
The implementation now shares immutable indices per distinct asset and uploads
one deformed source vertex per instance into each fenced slot. Its renewed
functional/visual checks and same-condition timing comparison are recorded in
[indexed-validation.md](indexed-validation.md). The original expanded failure
evidence above remains retained. All nine indexed comparisons pass. Four-character
GPU p95 is 12.315–12.470 ms; CPU p95 is 2.234–2.400 ms; frame p50/p95/p99 is
16.700–16.720 / 17.466–17.517 / 17.863–17.898 ms. The camera, scene, animation
mix, measurement path and capacity/quality limits stayed unchanged. The timing
path's ALL_COMMANDS acquire wait remains an explicit measurement limitation.
This accepts the P07a fixture; P07b still needs its own route/physics/audio timings.

P07a now meets its acceptance conditions and is ready for archive/sync in the
separate archive workflow, followed by rebasing P07b task 1.1 on the resulting
main specs. Full T2 remains dependent on P07b runtime and P07c authoring acceptance.
