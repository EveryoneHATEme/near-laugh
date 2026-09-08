# P07a validation — accepted after indexed revision, 2026-09-08

The indexed follow-up was authorized and implemented on 2026-09-08. **All nine
fresh Release runs pass every unchanged gate**, including four characters.
Its 366/366 Debug tests, 10/10 Vulkan checks, readback comparison, timings and
limits are recorded in [indexed-validation.md](indexed-validation.md).
The [accepted handoff](handoff.md) is ready for the separate archive workflow.
This is not full T2 acceptance.

The sections below retain the original expanded implementation evidence.
All three original four-character runs failed GPU/frame targets and remain
preserved. Their failure motivated the indexed revision; it is not overwritten
or retroactively accepted by the new result.

## Build and deterministic behavior

Debug configuration and full game/editor/viewer/test build succeeded with
Clang 23.1.0 targeting x86_64-pc-windows-msvc. Full `ctest --preset debug
--output-on-failure` passed **363/363 tests in 52.99 s**;
[retained log](evidence/debug-tests.log). Includes 14 CPU animation tests, six
presentation/surface-format tests, public/dependency boundaries and copied-resource
preflight without local source assets.

Continuation ran the complete `ctest --preset vulkan-smoke --output-on-failure
--output-log build/character-vulkan-suite-continuation.log`: **10/10 passed in
43.92 s**, including game, editor, partial construction, injected validation
error detection and character teardown; [retained log](evidence/vulkan-suite.log).
No shaders changed, so shader regeneration was not applicable.

The final viewer was rebuilt in Debug and Release after review fixes below.
`ctest --preset debug -R 'character_.*resources' --output-on-failure` again passed
both resource-layout tests (0.45 s). Debug `--check 4` and Release `--check 0/1/4`
completed 40 frames through explicit swapchain recovery. GPU timestamps were
available for every submitted frame; the two recovery frames submit no work.
Check-only readback after recovery is separate from performance sampling.

## Documentation and review

Reviewed the implementation diff and new animation/presentation/viewer code,
with independent static review of decoding, playback, resource lifetime and
measurement methodology. The review fixes to measured playback count and
fixture placement are described below. Architecture, rendering, development
and asset workflow documentation now describe the delivered profile and the
P07b handoff, including its outstanding accepted-distance playback integration.
`git diff --check` and `openspec validate add-character-animation --strict`
passed after these documentation updates. Preexisting changes to ROADMAP and
other active proposals were preserved. No main specs were synced or archived.

## Asset preparation

[Preparation evidence](../../../../resources/characters/evidence/preparation-verification.json)
records two byte-identical complete regenerations and 128 unchanged source files.
The prepared GLB is 959160 bytes; maximum bind displacement is 6.3152e-7 m against
the 1e-4 m limit. [Calibration and source reference](../../../../resources/characters/README.md)
document the measured walk/contact/interaction values and the source sole dip
of 2.654 cm below the authored feet plane. No retargeted body has been accepted.

## Vulkan and visual observations

The new `vulkan_character_animation` test passed with validation through final
teardown. [26 readbacks](evidence/captures/captures.json) retain exact RGB after
verified PPM-to-PNG conversion. [Readback results](evidence/captures/readbacks.txt)
cover clips, interruption, resize/minimize/restore, advertised alternate attachment
format 50 → 44 → 50, allocation/upload failure, independent instances and transactional
editor replacement. Frozen pose images and static/door ambient/flashlight controls
were byte-identical after the relevant operations. Off-screen mannequin vertices
were outside the view; changed receiver pixels 118595 (idle) / 126416 (interact)
therefore demonstrate actual geometry shadows. The two poses' shadows differ.

The [independent visual review](evidence/visual-review.md) inspected all 26
readbacks, eight desktop screenshots and the Python mesh reference. Bind
proportions agree; walking alternates the leading leg and lifts the swing foot;
interaction extends the left hand. Orange body and purple joints stay distinct
under different yaws, with coherent curved-surface shading. No visible skin
spike, collapsed limb, detached section or inverted lighting patch was observed.
The arm/leg silhouettes change in visible and off-screen shadows.

Idle/walk starts and phases 0.999999 (filenames ending `-99`) have visually close
stances and shadows. The interrupted blend has no bind reset; the immediate
interruption readback is exactly unchanged, and the middle/frozen-recovery
images match. These are stationary sample comparisons, not a continuous-motion
recording; there is no separate PNG at the completed 0.15 s blend endpoint.
Endpoint/time-batching guarantees have separate CPU behavioral coverage.
Absolute scale comes from calibration, not a ruler in the perspective images.
Coarse shadow edges, merged fine hand details and limited contact precision
remain visible limitations, together with the preserved source sole dip.

The real desktop keyboard script completed with code 0 and 377 frames. Retained
[viewer screenshots/logs](evidence/viewer-controls/viewer.log) show paused four
figures at interact 0.70 s, restart to walk 0.00 s, and resumed one-instance side view
at interact 1.49 s. Paused seek snapshot hashes match; see the visual review.
The viewer has no audio/gameplay coordinator; its explicit entry is independent
of ordinary game level filenames.

## Hardware and measurement conditions

AMD Ryzen 5 4600H, 6 cores/12 threads; AMD Radeon(TM) Graphics,
driver 31.0.21921.1000; 16,505,966,592 bytes RAM; 1920x1080 at 60 Hz; Windows balanced
power scheme. Vulkan uses D32 point shadows on this device. External OW_OVERLAY
and OW_OBS_HOOK layers emit API 1.2 warnings; successful validation runs report
no error-severity messages.

The final Release viewer uses the Clang/Ninja `build/p10-release` tree, configured
with `cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release` and
built with `cmake --build build/p10-release --target character_animation_viewer
-j 4`. Measurement logs must confirm validation disabled and FIFO presentation.

Before measurement, static review found that the one-character mode advanced
four playback owners. It now creates/advances exactly the measured count;
interactive one/four switching still retains four independent states. Actual
readback also exposed a rear row intersecting the room wall; all four placements
were moved inside Lena's room and the fixed camera raised for visibility.
Earlier diagnostic checks under `evidence/measurement-checks` show the rejected
placement and are not accepted measurement views. Windows desktop copies of the
exclusive fullscreen window were blank and are not used as image evidence.

The accepted fixture keeps the packaged eight-light/four-caster capacity scene,
initial doors, disabled flashlight and fixed camera identical across 0/1/4.
Eye is `(-3.15, 5.65, 5.85)`, looking at `(-3.15, 3.8, 2.7)`; feet are at Y=3.
The single mannequin is at X/Z `(-3.15, 3.5)`. Four use X `-4.2/-2.1` and
Z `3.5/2.0`, with the fourth yawed 35 degrees. All four bodies are visible in
the final readback; ordinary body/prop depth occlusion remains enabled.
Retained final views show [zero](evidence/timings/views/character-benchmark-final-release-0.png),
[one](evidence/timings/views/character-benchmark-final-release-1.png) and
[four](evidence/timings/views/character-benchmark-view-final.png) under that same
camera. [Check records](evidence/timings/views/checks.json) preserve exact RGB
hashes and GPU-query availability through recovery; accompanying CSV/logs
include the final Debug validation run.

One character plays idle. Four play idle, walk, interact and a fourth changing
clip every two seconds. The third holds its interaction endpoint after its
one-shot completes. This is a mixed animation workload, not four identical
walking characters; it includes no route physics/audio. The reported CPU
deformation scope includes validation/skinning/index expansion; upload measures
the mapped copy, while active CPU also includes sampling, recording and loop
work. GPU whole-frame/shadow and frame interval remain separate from fence,
acquire and present waits.

`scripts/measure_character_animation.ps1 -OutputDirectory
build/character-timings-final` completed three runs for each count, each
with 10 s warm-up and 60 s sampling on the ordinary interactive desktop, with
no concurrent builds or GPU tests. There were no interrupted or discarded
performance runs. Frame intervals are successive CPU frame-completion intervals,
not display scanout latency. All nine runs have complete GPU measurements,
1920x1080 submitted frames and no non-submitted post-warm-up rows.

## Release results — expanded baseline

[Raw CSV.gz files and per-run logs](evidence/timings/summary.json) retain every
warm-up and sample row. [Retention hashes](evidence/timings/retention.json)
verify lossless gzip against each original CSV; [input hashes](evidence/timings/input-hashes.json)
identify the measured executable, viewer source, level, asset and scripts.
The original uncompressed data and initial summary remain in
`build/character-timings-final`. Percentiles use nearest rank on rows with
elapsed time in `[10, 70)` seconds. Values below are milliseconds, rounded to
three decimals; the JSON contains the unrounded values and every gate result.

| Characters / run | CPU p95 | Deform p95 | Upload p95 | GPU p95 | Shadows p95 | Frame p50 / p95 / p99 | Gates |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 0 / 1 | 0.713 | 0.000 | 0.000 | 9.507 | 1.776 | 16.660 / 17.363 / 17.651 | All pass |
| 0 / 2 | 0.691 | 0.000 | 0.000 | 9.541 | 1.834 | 16.670 / 17.375 / 17.679 | All pass |
| 0 / 3 | 0.715 | 0.000 | 0.000 | 9.482 | 1.772 | 16.668 / 17.369 / 17.650 | All pass |
| 1 / 1 | 1.665 | 0.773 | 0.239 | 11.441 | 3.280 | 16.698 / 17.398 / 17.724 | All pass |
| 1 / 2 | 1.605 | 0.737 | 0.251 | 11.286 | 3.188 | 16.682 / 17.396 / 17.703 | All pass |
| 1 / 3 | 1.682 | 0.767 | 0.240 | 11.369 | 3.239 | 16.695 / 17.384 / 17.648 | All pass |
| 4 / 1 | 5.819 | 3.203 | 2.113 | 16.969 | 8.048 | 33.342 / 34.122 / 34.455 | CPU pass; GPU and all frame gates FAIL |
| 4 / 2 | 5.620 | 2.963 | 2.131 | 16.923 | 7.964 | 33.336 / 34.047 / 34.324 | CPU pass; GPU and all frame gates FAIL |
| 4 / 3 | 5.948 | 3.378 | 2.167 | 17.013 | 8.025 | 33.332 / 34.065 / 34.335 | CPU pass; GPU and all frame gates FAIL |

The unchanged targets are CPU/GPU p95 ≤16.67 ms and frame p50/p95/p99
≤16.9/20/33.4 ms **in every run**. Four-character frame cadence is approximately
30 FPS; the small GPU p95 miss does not make the end-to-end failure acceptable.
No character/light capacity or shadow quality was reduced, and historical T1
acceptance is not redefined by this P07a result.

Using the median of each setup's three p95 values, zero → one adds about
0.952 ms active CPU, 1.862 ms whole GPU and 1.463 ms shadows. Zero → four adds
5.106 ms active CPU, 7.462 ms whole GPU and 6.249 ms shadows. These differences
compare setup percentiles, not paired frame costs. The four-character deformation
and mapped upload scopes are material CPU work; the shadow passes account for
most of the GPU increase. The timestamps do not isolate vertex versus fragment
or memory-bandwidth costs and do not prove which optimization will recover 60 Hz.

All sampled four-character frame intervals exceed 25 ms, while only about
9.0–10.7% of their GPU samples exceed 16.67 ms. Acquire p50 rises to
28.44–28.65 ms from the baseline's 15.98–16.07 ms; four-character fence p95
stays ≤0.0271 ms and present p95 ≤0.3361 ms. This is sustained FIFO/presentation
pacing under the additional load. These scopes do not isolate the exact
scheduling cause, so the occasional GPU threshold crossing alone does not
explain each doubled frame interval.

A subsequent read-only timing audit verified all nine retained CSV hashes and
the current recorded input hashes. It also identified a measurement limitation:
`Renderer::renderFrame` waits on the acquired-image semaphore at `ALL_COMMANDS`
when timing is enabled, versus `COLOR_ATTACHMENT_OUTPUT` otherwise. This keeps
presentation-image availability outside the GPU timestamps but delays shadow
work in the instrumented path too. The measured approximately 30 Hz cadence
therefore describes this measurement mode; it does not establish the ordinary
non-instrumented renderer's cadence. The failed gates remain valid for the
specified measurement path. Keep this path unchanged for indexed comparisons;
no fresh performance samples were collected during the audit.

This baseline expanded 8,546 source vertices into 41,232 uploaded vertices
per character and reused that stream in color and 24 shadow faces. Its failure
led to the reviewed, subsequently authorized revision of design decision 3:
retain source vertices per fenced slot and immutable shared indices, keeping
CPU skinning, materials, lighting and pose ownership. The fresh implementation,
visual/lifetime checks and identical Release comparison conditions are recorded
in [indexed-validation.md](indexed-validation.md); this baseline remains intact.
