# Indexed character validation — P07a accepted, 2026-09-08

The user authorized the bounded indexed revision after the expanded baseline
failed its four-character gates. **All nine indexed Release runs pass every
unchanged gate; P07a is accepted with the appearance and measurement limits
below.** Design decision 3 and tasks 5.1–5.4 were updated before implementation.
The original [failed runs](evidence/timings/summary.json) and their
[validation record](validation.md) remain retained. This is not full T2 acceptance.

## Implementation and functional evidence

The renderer now uploads 8,546 source vertices per mannequin instead of 41,232
expanded vertices. Four instances write 1,367,360 bytes per frame instead of
6,597,120 bytes, a 79.2734% reduction. They share 164,928 immutable index bytes.
CPU skinning, source triangle order, both primitive materials, vertex layout,
shaders, eight lights/four casters, shadow resolution/filtering and borrowed
pose ownership remain as in the baseline. The resource owner initializes
coherent index memory once and retains it through presentation recovery.

CPU tests reconstruct submitted triangles and compare independent positions,
normals, UVs and material colors. Coverage includes reordered poses, four shared
instances, distinct assets with the same skeleton identity, full capacity,
invalid ranges and empty selection. Vulkan smoke additionally reverses a cloned
asset's vertex storage and remaps its indices: alternating original/remapped
assets must reproduce the four-shared image across six frame-slot iterations.
Three index buffer/memory/initialization failure hooks join the previous vertex
and material hooks; editor rollback retains its entire prior image for all seven
construction failures. Index allocation, upload, retention and teardown are checked.

Commands completed:

```powershell
cmake --preset debug
cmake --build --preset debug --target near_laugh level_editor character_animation_viewer -j 4
cmake --build --preset debug -j 4
ctest --preset debug --output-on-failure --output-log build/character-indexed-debug-tests.log
ctest --preset vulkan-smoke --output-on-failure --output-log build/character-indexed-vulkan-suite.log
.\build\debug\bin\character_animation_smoke.exe build/character-captures-indexed
python -B scripts/retain_character_captures.py build/character-captures-indexed openspec/changes/add-character-animation/evidence/captures-indexed
cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/p10-release --target character_animation_viewer -j 4
.\build\debug\bin\character_animation_viewer.exe --check 4 build/character-indexed-debug-4.csv
.\build\p10-release\bin\character_animation_viewer.exe --check 0 build/character-indexed-release-0.csv
.\build\p10-release\bin\character_animation_viewer.exe --check 1 build/character-indexed-release-1.csv
.\build\p10-release\bin\character_animation_viewer.exe --check 4 build/character-indexed-release-4.csv
```

Debug configuration and full build passed. The [deterministic/process/boundary
suite](evidence/character-indexed-debug-tests.log) passed **366/366 in 57.12 s**,
including eight presentation and 14 animation tests. The [full Vulkan
suite](evidence/character-indexed-vulkan-suite.log) passed **10/10 in 44.42 s**.
The separate [capture run](evidence/character-indexed-captures.log) passed with
validation through teardown. All four viewer checks completed 40 frames through
recovery, with 38 submitted frames each and complete GPU timestamps; the other
two frames perform recovery without submission. Their [CSV/logs and lossless
views](evidence/timings-indexed/views/checks.json) are retained. Shader regeneration
was unnecessary because no shader changed.
The installed Ninja and desktop GPU runs required the approved execution mode;
initial sandbox configuration/hardware-query denials were retried successfully.

## Readback comparison and visual review

[27 indexed readbacks](evidence/captures-indexed/captures.json) retain exact RGB
after verified lossless conversion. [Baseline comparison](evidence/captures-indexed/baseline-comparison.json)
found **25 of 26 original views exactly identical**. Only `interact-25.png`
differs: one pixel at (495, 353), on the foot, changes from RGB (208, 153, 51)
to (219, 161, 54). Maximum channel difference is 11/255; whole-image mean is
0.0000141461 byte values per channel. The cause of this isolated pixel change
has not been established; the images are not claimed to be wholly identical.

Direct inspection of both interaction images shows the same stance, extended
arm, orange body/purple joints, curved-surface lighting and cast silhouette.
The indexed four-character fullscreen view shows all four bodies inside Lena's
room with their existing placements, depth occlusion and visible wall/floor
shadows. No new mesh spike, displaced limb, material mismatch or silhouette
change was observed. Independent subagent inspection confirmed the changed pair,
mixed-asset equality and fullscreen four-character visibility. The 25 exact
comparisons cover bind, the remaining clip
samples, interrupted blends, frozen recovery, static/door/flashlight controls,
off-screen shadows and editor retention. Existing source sole dip and coarse
shadow/contact limitations remain documented in the original review.

The new `four-mixed-equivalent.png` matches `four-independent.png` exactly.
Off-screen changed receiver counts remain 118595 for idle and 126416 for
interact. Readbacks wait for GPU completion; the viewer's ordinary submitted
check/measurement frames exercise the normal fence-protected slot path too.

## Release comparison conditions

Hardware was queried again: AMD Ryzen 5 4600H (6 cores/12 threads), AMD Radeon(TM)
Graphics driver 31.0.21921.1000, 16,505,966,592 bytes RAM, 1920x1080/60 Hz and
Windows balanced power scheme, matching the baseline. D32 retains 24 shadow
layers per slot. Debug uses Khronos validation; Release logs confirm validation
disabled and FIFO. Existing external OW_OVERLAY/OW_OBS_HOOK API 1.2 warnings
remain separate from error-severity validation results.

The same Clang 23.1.0/Ninja Release tree, packaged capacity scene, camera,
placements, initial doors, disabled flashlight and animation mix are used.
The instrumentation still waits for image acquisition at ALL_COMMANDS; its
cadence describes the instrumented path, not display scanout or an independent
non-instrumented frame-rate measurement. CPU deformation now includes source
vertex conversion instead of index expansion; all timing scope boundaries stay
unchanged. There is no P07b route physics/audio or full T2 claim.

```powershell
.\scripts\measure_character_animation.ps1 -OutputDirectory build/character-timings-indexed
python -B build/retain-character-indexed-timings.py
```

The batch uses three runs for each 0/1/4-character setup, each with 10 s warm-up
and 60 s sampling, on the ordinary interactive desktop with no concurrent builds,
tests or other project GPU work. CPU/GPU p95 limits remain 16.67 ms and frame
p50/p95/p99 limits remain 16.9/20/33.4 ms in every run. All nine runs completed
without interruption, discarded runs or missing GPU data. Every post-warm-up
row was submitted at 1920x1080; each run retains 3590–3601 sample rows.

## Indexed Release results and acceptance

[Raw CSV.gz/logs and unrounded summaries](evidence/timings-indexed/summary.json)
retain all warm-up and sample rows. [Retention hashes](evidence/timings-indexed/retention.json)
verify every lossless CSV round trip. [Input hashes](evidence/timings-indexed/input-hashes.json)
record the executable, renderer sources, measurement sources/scripts, shaders,
Release cache and actual packaged scene/asset/shader bytes. The [baseline audit](evidence/timings-indexed/baseline-audit.json)
verifies all nine original CSV/gzip hashes and that every original evidence file
remains unchanged. Viewer source, scene, asset and both measurement scripts match
their original hashes. Original shader hashes were not recorded by that older
retention helper; no shader was edited in this continuation, and the new record
includes their hashes and verifies source/package equality.

All values below are milliseconds, nearest-rank sample percentiles in [10,70)
seconds, rounded to three decimals. Every individual gate passes.

| Characters / run | CPU p95 | Deform p95 | Upload p95 | GPU p95 | Shadows p95 | Frame p50 / p95 / p99 |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 0 / 1 | 0.726 | 0.000 | 0.000 | 9.521 | 1.783 | 16.672 / 17.342 / 17.576 |
| 0 / 2 | 0.717 | 0.000 | 0.000 | 9.927 | 1.832 | 16.675 / 17.344 / 17.613 |
| 0 / 3 | 0.725 | 0.000 | 0.000 | 9.544 | 1.789 | 16.670 / 17.384 / 17.656 |
| 1 / 1 | 1.222 | 0.496 | 0.052 | 10.125 | 2.038 | 16.686 / 17.374 / 17.655 |
| 1 / 2 | 1.239 | 0.519 | 0.054 | 10.107 | 2.090 | 16.689 / 17.307 / 17.544 |
| 1 / 3 | 1.184 | 0.485 | 0.055 | 10.218 | 2.099 | 16.694 / 17.325 / 17.669 |
| 4 / 1 | 2.400 | 1.373 | 0.244 | 12.366 | 3.505 | 16.700 / 17.517 / 17.898 |
| 4 / 2 | 2.283 | 1.314 | 0.247 | 12.470 | 3.562 | 16.704 / 17.466 / 17.863 |
| 4 / 3 | 2.234 | 1.260 | 0.239 | 12.315 | 3.455 | 16.720 / 17.501 / 17.867 |

For four characters, the median of the three run p95 values changes as follows:

| Scope | Expanded baseline | Indexed revision |
| --- | ---: | ---: |
| Active CPU | 5.819 | 2.283 |
| Deformation/conversion | 3.203 | 1.314 |
| Mapped upload | 2.131 | 0.244 |
| Whole GPU | 16.969 | 12.366 |
| GPU shadows | 8.025 | 3.505 |

These compare run percentiles rather than paired frame costs. Four-character
frame medians change from 33.332–33.342 ms to 16.700–16.720 ms under the same
instrumentation. The observed result supports the indexed revision and meets
P07a's stated capacity without reducing characters, lights or shadow quality.
It does not identify a single shader-stage cause or promise that P07b's added
physics/audio will fit without fresh route measurements.

The implementation diff and actual new files were reviewed, including an
independent CPU/resource/lifetime review. Architecture/rendering documentation,
the performance follow-up and P07b handoff now describe indexed presentation.
Strict OpenSpec and final whitespace checks are recorded at completion in
[CONTINUE.md](CONTINUE.md). The handoff retains the source sole dip, limited
shadow/contact precision and P07b's separate distance-driven playback work.
