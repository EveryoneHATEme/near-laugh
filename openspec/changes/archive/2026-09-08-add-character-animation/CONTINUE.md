# Continuation checkpoint — 2026-09-08

## Current continuation — indexed revision accepted

The user explicitly authorized indexed rendering and parallel subagents on
2026-09-08. Design decision 3 and tasks 5.1–5.4 were revised before code changes.
The indexed follow-up and accepted handoff are now finished: **23/23 tasks**.
Use [indexed-validation.md](indexed-validation.md) for final evidence and
[handoff.md](handoff.md) for P07b's exact dependencies and remaining responsibilities.

Character presentation now retains one deformed source vertex per instance,
shared immutable uint32 indices/materials per distinct asset, and checked
first-index/count/signed vertex offsets. CharacterResources owns the immutable
index buffer and two existing fenced vertex slots; color and every point-shadow
face use indexed draws through their shared draw method. CPU skinning, material
and shader layout, triangle order, eight lights/four casters and quality limits
remain unchanged. The four-character changing payload drops from 6,597,120 to
1,367,360 bytes; shared immutable indices occupy 164,928 bytes.

Three subagents handled CPU implementation/tests, Vulkan smoke extensions and
independent code/evidence review; root integrated resources and ran validation.
Debug/Release configured and rebuilt. **366/366 Debug tests passed (57.12 s)**
and **10/10 Vulkan smoke tests passed (44.42 s)**. The separate capture run
retains 27 images in `evidence/captures-indexed`; 25/26 baseline images match
exactly and the remaining image changes one foot pixel (max channel delta 11).
Its cause is undetermined; both images were inspected without a visible pose,
material or silhouette difference. The additional remapped mixed-asset image
matches four-shared geometry exactly. Index failure/rollback, empty selections,
recovery, slot reuse and teardown are covered. Debug check4 and Release check0/1/4
all passed; their CSV/logs and lossless views are in `evidence/timings-indexed/views`.

All nine ordinary-desktop Release samples from
`scripts/measure_character_animation.ps1 -OutputDirectory build/character-timings-indexed`
pass every unchanged gate. Four-character GPU p95 is **12.315–12.470 ms**;
CPU p95 **2.234–2.400 ms**; frame p50/p95/p99 is
**16.700–16.720 / 17.466–17.517 / 17.863–17.898 ms**. All GPU sample fields are
available and all sample rows are submitted. No builds/tests/project GPU work
overlapped these samples. Hardware/driver/power, scene/camera, timing path and
thresholds match the baseline. P07a is accepted; this does not establish full T2
or predict P07b route/physics/audio performance.

`python -B build/retain-character-indexed-timings.py` already retained verified
lossless data under `evidence/timings-indexed`. Do not rerun into that destination.
It verified all nine original CSV/gzip hashes, unchanged frozen benchmark inputs
and that all original evidence files remain unchanged. Source/package shader
hashes are recorded now; the older retention did not record shader hashes.
The ALL_COMMANDS acquire-wait limitation remains explicit; ordinary
non-instrumented cadence and display scanout were not separately measured.

Architecture/rendering docs, validation, performance follow-up and handoff are
updated. The actual edited source/tests and `git diff` were reviewed; whitespace
checks (including untracked text) and strict OpenSpec validation passed. No shaders
changed. Initial sandbox Ninja/hardware denials were resolved using approved
execution; relevant desktop validation was performed. No commit, spec sync or
archive has been made, and the preexisting user changes remain. The next action
is the separate `$openspec-archive-change add-character-animation` workflow,
then P07b task 1.1 rebasing on the archived main specs. Earlier failures and
checkpoints below are historical evidence, not current acceptance status.

## Pre-indexed continuation (superseded by the accepted revision above)

The next apply session completed independent read-only code and timing reviews
of `performance-followup.md`. The bounded indexed proposal has no identified
behavioral-spec conflict; concrete offsets, ownership and regression coverage
are now recorded in that file's technical-review section. All nine retained
CSV hashes and current recorded input hashes match. `validation.md` now states
the instrumented acquire-semaphore scheduling limitation explicitly. No code,
design decision, tasks, benchmark inputs or acceptance gates changed, and no
new builds/GPU runs were needed for this documentation-only review. An explicit
user confirmation for revising design decision 3 and implementing the proposal
was requested; do not treat technical review itself as that confirmation.

Tasks 4.2–4.5 are now checked: **18/19**. The complete Vulkan
suite passed 10/10 (43.92 s), visual review covers all retained images and
the independent reference, and docs/diff/strict OpenSpec review passed.
See updated `validation.md`, `evidence/visual-review.md` and `handoff.md`.

Before performance sampling, review fixed the viewer's one-character mode
advancing four playback owners. It now creates only the measured count.
`--check` additionally saves a sibling PPM after recovery. That exposed the
original rear row intersecting Lena's room wall; final camera/placements were
corrected and actual readback confirms four visible bodies inside the room.
Debug/Release viewer rebuilt; final Debug check4 and Release check0/1/4 passed,
as did both copied-resource process tests. No animation/renderer core changes
were made during continuation.

The nine-run Release script finished on the ordinary desktop:
`scripts/measure_character_animation.ps1 -OutputDirectory build/character-timings-final`.
**All six 0/1-character runs pass; all three four-character runs fail GPU and
all frame-time gates.** Four-character GPU p95 is 16.923–17.013 ms; frame p50
is 33.332–33.342 ms, p95 34.047–34.122 ms and p99 34.324–34.455 ms. CPU p95
passes at 5.620–5.948 ms. P07a is NOT accepted and task 4.6 stays unchecked.

`python -B build/retain-character-timings.py` already retained verified lossless
CSV.gz, logs, summaries, input hashes, final check CSVs and exact-RGB PNG views
under `evidence/timings`. Do not rerun it into the existing destination. All
performance samples have complete GPU data and zero non-submitted sample rows.
Earlier checks in `evidence/measurement-checks`
use the rejected placements and are labelled diagnostic in validation.

The recorded next investigation is indexed character buffers/draws, preserving
CPU skinning and all material/light/pose/lifetime contracts. This would change
design decision 3's explicit index expansion; implementation awaits review,
following the apply skill's design-issue pause. The current scopes show a large
shadow cost increase, material CPU deformation/upload and sustained FIFO pacing,
but do not prove a shader-stage bottleneck or a guaranteed 60 Hz fix.
See `validation.md`, `handoff.md` and the reviewable `performance-followup.md`
before resuming; preserve every failed run
and all thresholds. Only mark 4.6 when acceptance is actually justified.
No commit, spec sync or archive has been made; preexisting user changes remain.
The three inspected generated bytecode files and empty `scripts/__pycache__`
were removed using verified literal workspace paths. Final diff whitespace and
strict OpenSpec validation passed after recording the results.

## Older checkpoint (retained implementation context)

The user requested `$openspec-apply-change add-character-animation` and explicitly
authorized parallel subagents. Work paused at the user's context-reset checkpoint,
not because of a design blocker. **14/19 tasks are checked; final acceptance is pending.**
Read this file, `validation.md`, `tasks.md`, and the normal OpenSpec context before
continuing. No commit or archive has been made. Keep the current working tree.

## Immediate next work

1. Run the complete `ctest --preset vulkan-smoke --output-on-failure`; the new
   character smoke passed, but the whole existing game/editor suite has not yet
   been rerun with this change. The last CPU edits were stricter decoding and
   finite normal normalization, so refresh character GPU evidence if it differs.
2. Finish visual observations in `validation.md` (task 4.2). There are 26 actual
   readback PNGs in `evidence/captures` and real keyboard-control screenshots in
   `evidence/viewer-controls`. Representative images have already been inspected.
   Compare remaining loop/transition images and the independent Python mesh
   reference at `resources/characters/evidence/pose-reference.png`.
3. Rebuild the final Release viewer (last build predates final CPU/UI fixes), then
   run `--check 4 <fresh.csv>` to verify timing/query recovery before measuring.
   Run `scripts/measure_character_animation.ps1` on the ordinary desktop, with no
   simultaneous builds/GPU work. It runs 0/1/4 characters, three samples each,
   10 s warm-up + 60 s samples = about 10.5 minutes total. **No Release timings
   have been collected yet.** Preserve raw CSV (lossless gzip okay), logs,
   summary and all missed targets; do not claim acceptance if budgets fail.
4. Finish docs/diff review, strict OpenSpec validation, and the P07a handoff for
   P07b. Tasks 4.2–4.6 remain unchecked. Follow the apply skill; suggest the
   separate archive workflow once actually finished.

## Implemented code and ownership

- `scripts/prepare_character_animation.py`: deterministic standard-library GLB
  preparation, source/ZIP/license hash verification, scale/quaternion conversion,
  clip selection, calibration, independent CPU reference rendering, catalog/header.
- `resources/characters`: committed derivative, included CC0 notice, hashes,
  catalog, calibration samples/reference PNG and preparation verification record.
- `src/core/animation/character_asset.cpp`: bounded private cgltf animated GLB
  decoder, 16 MiB file/32 MiB parse allocation budget, JSON depth guard, checked
  accessor ranges, hierarchy/bind/geometry/material/channel validation.
- `character_animation.hpp/.cpp`: project scalar/container types, local TR and
  global joint poses, shortest-arc sampling, CPU skinning, separate placement,
  loop/clamp/pause/seek and captured 0.15 s transitions. Blend endpoints preserve
  the captured pose exactly; elapsed accumulation handles floating-point batching.
- `character_catalog.hpp` is GENERATED; change the preparation script and
  regenerate if modifying it, preserving manifest/header hashes.
- `near_laugh_animation` is CPU-only. `near_laugh_cgltf` changed from INTERFACE
  to STATIC, compiling existing cgltf_impl.cpp once for both loaders. Runtime,
  editor and renderer link animation privately; static importer profile unchanged.
- `CharacterPoseFrame` in `src/core/frame.hpp`: instance handle, skeleton tag,
  borrowed global joint matrices, position/yaw. RendererResources.characters
  selects immutable shared assets. No routes/actions/backend types in frame data.
- `character_presentation.*` validates complete selection/palettes, material/index
  ranges and native supplied asset safety. `character_resources.*` owns independent
  mapped coherent vertex buffers for two fenced slots and shared primitive textures.
  Deform once before acquire, upload after fence, reuse color/all point-shadow faces.
- `EditorRenderer::replaceDocument(level, instances)` builds a complete candidate
  scene/light/character set and retains old resources if candidate allocation fails.
- `alternate_surface_format` test hook selects another advertised surface format
  during recovery and logs `swapchain.surface_format.alternate.selected`.
- `src/character_animation_viewer_main.cpp`: explicit executable, no runtime
  Engine/physics/audio linkage. Interactive controls below; explicit measure/check
  and headless preflight modes. Normal filenames never activate the viewer.
- FrameTimingSample/CSV now add `character_deformation_ms` and
  `character_upload_ms`; existing lighting summarizer handles these optional fields.

## Commands and evidence

```powershell
cmake --preset debug
cmake --build --preset debug -j 4
ctest --preset debug --output-on-failure
ctest --preset vulkan-smoke --output-on-failure
.\build\debug\bin\character_animation_smoke.exe build/new-character-captures
python scripts/retain_character_captures.py build/new-character-captures <fresh-output>
.\scripts\check_character_viewer.ps1 -OutputDirectory build/new-viewer-controls
cmake --preset debug -B build/p10-release -DCMAKE_BUILD_TYPE=Release
cmake --build build/p10-release --target character_animation_viewer -j 4
.\build\p10-release\bin\character_animation_viewer.exe --check 4 build/new-character-check.csv
.\scripts\measure_character_animation.ps1 -OutputDirectory build/new-character-timings
openspec validate add-character-animation --strict
```

The existing Release tree uses a WinGet Ninja outside the sandbox. Its configure
and build required escalation; scoped approvals were granted in this session.
GUI tools also ran with desktop escalation. Do not bypass those restrictions.
All output directories/CSV paths must be new. No performance run may overlap
builds, other GPU tests or viewers. Poll long running commands while maintaining
user updates (no long blocking wait).

Viewer: F5 clip; P pause; A/D seek ±0.1 s and pause; Space restart; M one/four;
E fixed view; R flashlight; Escape close. Interactive window is 1280x720;
measure/check use fullscreen 1920x1080/60 Hz and the packaged eight-light/four-caster
`interior-lighting-capacity.level.json`. One/four benchmark placements use the
same Lena-room view. Before accepting timing evidence, inspect the measurement
view if needed to ensure representative placement/visibility.

## Verified state at checkpoint

- Full Debug build succeeded for game/editor/viewer/tests.
- **363/363 deterministic/process/boundary tests passed**, 52.99 s. The command
  completed after the user interrupted the tool call. Log copied to
  `evidence/debug-tests.log`; original `build/character-debug-tests.log`.
- CPU CharacterAnimation: **14/14 passed**, including tiny two-joint known skin,
  world yaw/normals, absent channels, actual mannequin samples, malformed inputs,
  file/geometry/key/parse caps, metadata, pause/seek/interruption and batching.
- CharacterPresentation + SurfaceFormat: **6/6 passed**.
- Copied-resource preflight: both tests pass from another cwd; isolated copy has
  only executable + prepared characters, no source archives/build/p07-assets.
- New Vulkan character smoke passed under validation through teardown. Captures:
  `build/character-captures-final` -> 26 lossless verified PNGs already retained
  in `evidence/captures`. Includes moving off-screen shadows (118595 idle /
  126416 interact changed receiver pixels), static/door ambient/flashlight controls,
  exact frozen recovery, alternate format50->44->50, zero/four, five injected
  allocation/upload failures, editor retained candidate and final teardown.
- Actual desktop keyboard script succeeded with code0 and 377 rendered frames;
  `build/character-viewer-controls-accepted` copied to `evidence/viewer-controls`.
  Inspected four-paused (interact0.70s, four figures), walk-restarted (0.00s paused),
  one-resumed (interact1.49s playing side view). Seek/pause pair hashes compared.
- Earlier failed checks retained under build: transition endpoint renormalization
  fixed; smoke fixture missing door material fixed; viewer caption newline (font
  rejects controls) removed; desktop script fixed native handle/exit code and
  waits after M. No unresolved functional failure known.
- Early strict OpenSpec validation passed; rerun after final records/diff.
- No live ctest, engine_tests, viewer, character smoke, Ninja or Clang processes
  were found at checkpoint. Subagents all finished; their work is in shared files.

## Hardware, limits, and preexisting work

AMD Ryzen5 4600H (6 cores/12 threads), AMD Radeon(TM) Graphics,
driver31.0.21921.1000, 16,505,966,592 bytes RAM, 1920x1080/60 Hz,
Windows balanced power scheme. Installed Honor virtual display also enumerated.
Clang23.1.0, x86_64-pc-windows-msvc, D:/LLVM. Vulkan warnings from external
OW_OVERLAY/OW_OBS_HOOK layers advertise1.2; no Vulkan error-severity messages in
successful runs. Do not conflate warnings with failure or suppress validation.

Source GLB hash69591853d817488edaa8fd9bf8fc1d821eaeaf789f8627b3cd23b41c4ed67997;
derivative35bc29b259644b784b1bb3fe97e880ac0d7f51d4f5309965ebcfc47d2ac800c4,
959160 bytes. Two complete preparations byte-identical;128 source files unchanged.
Bind geometry delta <=6.32e-7m. Forward+Z, capsule radius0.25m/full height1.85m,
walk cycle1.3061969054512663m, contacts0/0.5, interaction LEFT-hand phase0.366666667.
P07b speed1.5m/s gives0.435399s contact spacing. **Source soles dip2.654cm below
feet plane**; preserved and prominently documented, not corrected or hidden.

Initial user changes that must remain untouched: docs/ROADMAP.md,
add-narrative-state-and-sequences/proposal.md, add-scripted-characters/proposal.md,
untracked add-scripted-characters design/specs/tasks, add-character-authoring and
the add-character-animation planning directory itself. These were already dirty
at turn start. Implementation edits are additive and uncommitted. Do not reset
or clean the repository or build/p07-assets. `scripts/__pycache__` was generated
by capture export; remove only our generated bytecode after inspecting paths,
using native PowerShell and workspace-contained targets (no broad cleanup).
