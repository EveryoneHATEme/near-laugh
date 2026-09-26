# Validation record

P06 implementation, automated functional/Vulkan checks and the controlled
six-run Release performance comparison have passed. On 2026-09-11 the user
stopped further manual testing and requested scripts/code checks. On 2026-09-26
the user accepted closing the remaining P06/T3 item on this evidence with
subjective hold/throw feel and physical listening retained as unverified
limitations. This acceptance decision does not represent a new test run or
hands-on confirmation of those qualities; see the final decision below.

## Current acceptance status

| Check | Result | Evidence |
| --- | --- | --- |
| Integrated Debug / real-ImGui | 561/561 passed, including all 29 real-ImGui checks | `build/household-automated-20260911/result.json`, `debug-tests.xml` and stage logs in that directory |
| Vulkan runtime/editor | 14/14 passed | `build/household-vulkan-final.log` |
| Release performance | Three paired zero/16-box comparisons (six runs) passed all workload/timing gates | `build/household-t3-20260910/release-desktop-repeat/acceptance.json` |
| Runtime behavior | Automated physical contact/carry/input/state checks passed; earlier reader recovery, pickup/hold/throw and normal/muted radio observations retained | Automated coverage below; `build/household-t3-20260910/runtime/` |
| Independent UI-authored scene | Passed creation, save/reopen, ordinary Play, box/document/radio behavior, history, reference repair and changed-pose Save-and-Play | [Retained scene](evidence/second-room.level.json); captures in `build/household-t3-20260910/authoring/` |
| Human feel and listening | Unverified; accepted limitations, with no further manual testing planned | Neither earlier desktop authorization nor automated results establish subjective quality or physical listening |
| P06/T3 acceptance | Accepted with the recorded limitations on 2026-09-26; task 9.5 closed | User's decision after the remaining acceptance item and limitations were reported; see final decision below |

## Definitions and compatibility

- 2026-09-10: reviewed current main specs, the v9 codec/immutable world handoff,
  and the accepted P04/P10/P07 baseline recorded in ROADMAP. The selected
  capability list and single v9-to-v10 transition agree with this checkout.
- `openspec validate add-household-interactions --strict`: exit 0.
- `cmake --preset debug`: exit 0 after retrying outside the sandbox, which
  denied execution of the installed Ninja. Log: `build/household-configure.log`.
- Debug targets `engine_tests`, `near_laugh`, `level_editor` build successfully.
  Log: `build/household-section1-build.log` (exit 0).
- Independent read-only review identified a partial-footprint terrain corner
  case: a rotated box entirely below the surface could evade both corner probes
  and surface intersection. The new regression failed before the fix (exit 1,
  `build/household-terrain-regression-before.log`). Shared authored clearance now
  clips terrain triangles to the box footprint and compares their interpolated
  height with the box bottom, with 0.1 mm contact tolerance. Follow-up independent
  review found no further defect in that fix; affected tests remain the acceptance
  evidence.
- The initial codec test compared ordered JSON objects and failed solely because
  the v9 fixture's environment-light object keys have a different order from
  canonical output. The test now compares semantic object fields while retaining
  ordered arrays, source-byte preservation and byte-identical canonical re-save.
- Expanded regression execution exposed older tests' current-v9 expectations.
  These were updated for explicit v10 saving, retaining historical fixture
  versions and source preservation.
- Final affected regression run: **148/148 passed**, exit 0, including household
  definitions/codec, level/character definitions, door geometry/physics/input,
  interior/scene authoring, immutable world, audio persistence, editor commands,
  saved-file Play and lighting. Log: `build/household-section1-tests.log`.
  Command: `build/debug/tests/engine_tests.exe
  --gtest_filter=HouseholdCodec.*:HouseholdDefinitions.*:LevelDocument.*:CharacterDefinitions.*:Door*.*:InteriorLevel.*:SceneAuthoring.*:PrototypeLevel.*:AudioPersistence.*:EditorCharacters.*:EditorCommands.*:EditorPlay.*:InteriorLighting.*`.
- Section 1 is implemented and verified. `git diff --check` and strict OpenSpec
  validation passed at this stage. Integrated Debug/UI/GPU results follow below.

## Physics, presentation and runtime iteration

- Debug `engine_tests`, `near_laugh`, `level_editor` build: exit 0,
  `build/household-runtime-tests-build.log`.
- Earlier non-UI affected run: **203/205 passed**, exit 1,
  `build/household-runtime-tests.log`, with filter
  `Household*.*:EditorHousehold.*:CaptionFont.*:Physics*.*:Player*.*:Door*.*:ActorPhysics.*:PrototypeScene.*:Frame.*:EditorCommands.*:EditorDocument.*:EditorPlay.*`.
  All 13 editor household history/reference tests, seven text tests, five
  presentation tests, and existing player/door/actor regression suites passed.
- Those two failures are resolved: box-top landing projects downward motion
  through collision and bounds blocked downward velocity to accepted vertical
  displacement, with no position rewrite or box support. The radio expectation
  now respects muted output. Follow-up independent review accepted the landing
  correction; the affected run below passed.
- Earlier landing fixtures failed authored spawn validation before constructing
  physics. They now start on a supported ledge and walk off it. Landing checks
  measure capsule versus fully oriented box/static geometry after both updates
  (1 mm numerical tolerance); quaternion equivalence uses angular error 0.1 deg.
- Independent source review checked Jolt inner-body/constraint construction and
  teardown, transactional stance, CCD/substeps, quaternion transforms, shared
  fenced color/shadow mesh, and runtime pending-command/suspension/frame ownership.
  No critical source finding remains from those reviews. GLFW delivery, GPU and
  manual interaction are not established by this evidence.

## Integrated authoring and gameplay checks

- Debug build of `engine_tests`, `near_laugh`, `level_editor`, `vulkan_smoke`
  and `household_measure`: exit 0, `build/household-preflight-build.log`.
- Affected runtime/physics/text/geometry/editor regression run: **227/227**,
  exit 0, `build/household-authoring-tests.log`. Includes isolated, tilted,
  stacked and confined landing, 16-box stress, physical hold, runtime input,
  saved-file Play and household picking/history/reference cases.
- Real-ImGui execution delegated to `ui_test_runner`: **29/29**, exit 0,
  `build/household-editor-ui-tests.log`, including five household UI scenarios
  and existing editor/character UI regressions. This is deterministic UI
  evidence, not desktop visual acceptance.
- First full Debug CTest: **558/559**, exit 1,
  `build/household-debug-ctest.log`. Sole failure was the source-boundary test's
  missing exact JSON-fixture allowlist entry for `test_household_codec.cpp`.
  The production codec boundary remains unchanged; targeted boundary rerun
  passed 1/1, exit 0, `build/household-boundary-check.log`.
- Expanded full Debug CTest: **560/561**, exit 1,
  `build/household-integrated-ctest.log`. The new fresh-run test incorrectly
  created two simultaneous Jolt runtimes and toggled a radio while carrying.
  It now verifies independent later authoring while the first run is alive,
  drops before radio use, destroys that run, then checks fresh state.
  Targeted fresh-run plus CPU smoke-fixture preflight: **2/2**, exit 0,
  `build/household-fresh-run-check.log`.
- Final full Debug CTest, delegated to `ui_test_runner`: **561/561 passed**,
  exit 0, `build/household-final-ctest.log`. No failed, blocked or unrun tests.
  Includes the real-ImGui regressions and CPU-only GPU-fixture preflight.
  Final `git diff --check` and strict OpenSpec validation: exit 0;
  `build/household-final-diff-check.log`,
  `build/household-openspec-validation.log`.
- Seven original packaged levels migrated to v10 with empty household arrays;
  three existing generators reproduce the same semantics. Logs:
  `build/household-level-migration.log`,
  `build/household-level-regeneration.log`. Affected migrated-level checks:
  **120/120**, exit 0, `build/household-migrated-level-tests.log`.
- Current neutral 4/0/16-box fixtures include identical furnishing, radio,
  readable pages, door/shadow workload and out/back actor routes. Ordinary
  definition/asset/text preparation and 180-step CPU checks pass. No filename
  selects game behavior. Historical v9 compatibility fixture remains unchanged.

## GPU and performance preparation history

- Dedicated `vulkan_household_interactions` and
  `editor_household_interactions` CTest entries are built. CPU-only
  `household_smoke_fixtures` validates door-free and 248-box authored content
  without creating a window. GPU results are recorded in the next section.
- Runtime readback preparation covers full physical orientation, maximum
  geometry, separate and combined reader/caption layers at three resolutions,
  reader state/recovery, owed release through minimization, initial-state
  reconstruction and exact construction/update failure diagnostics. Independent
  review found possible lost wake events and false-positive pixel/error checks;
  corrections repeat wake events until the wait returns, isolate one box's
  orientation, require text draw/output, and match each failure's diagnostic.
  Reviewed smoke rebuild: exit 0, `build/household-reviewed-smoke-build.log`.
  Independent re-review closed all four findings before GPU execution.
- Editor readback preparation covers authored box/radio/page edits and exact
  undo pixels, retained whole-scene resources after replacement failures,
  fonts across resize/attachment recovery, door-free/max scenes and teardown.
- Release `household_measure` build: exit 0,
  `build/household-release-configure.log`, `build/household-release-build.log`.
  Debug/Release executables and `scripts/measure_household.ps1` are ready.
  The script's AST parse passed before desktop measurements. Sidecar CSV
  records actual post-step awake/rest/action counts, and invalid physical
  workloads fail before timing acceptance. Six paired runs preserve raw
  timings, machine/configuration and all T1 gate results.
- The [manual scenario](manual-scenario.md) defines both the neutral interaction
  run and an independent scene created through the editor UI. `ui_driver`
  preflight confirmed existing `build/p01-desktop.ps1` and
  `build/p07-authoring-desktop.ps1` Win32 input/capture helpers are available.
  This preflight involved no live desktop interaction. The subsequent user
  authorization is recorded below; human feel/listening acceptance is separate.

## Authorized desktop acceptance run

- 2026-09-10: user explicitly authorized the current desktop for Vulkan,
  visual interaction/authoring and fullscreen measurements ("да, разрешаю").
  Runs were serialized with one desktop owner. The full Vulkan preset was
  delegated to `ui_test_runner`; `ui_driver` prepared the manual scenario
  before taking desktop ownership after that test run.
- Initial full Vulkan preset: **12/14 passed**, exit 1,
  `build/household-vulkan-acceptance.log`. All existing runtime/editor/character
  paths passed. Runtime household stopped at a 1920x1061 client area requested
  as 1920x1080; Win32 default tracking limits included window decoration.
  `Window::setSize` now applies temporary explicit GLFW sizing limits and
  immediately restores normal resizing. Targeted runtime rerun: **1/1 passed**,
  exit 0, `build/household-runtime-vulkan-rerun.log` (19.10 s), including
  800x600, 1920x1080 and 3840x2160 captures, suspension/recovery and failure
  teardown. Captures: `build/debug/bin/build/household-runtime-captures/392659821495800`.
- Editor initial failure was an overly broad reader comparison: the glyph
  region matched exactly after undo, while three parent resize-grip pixels
  differed with window focus. The smoke now intersects the child clip with
  actual panel content bounds; edited glyph differences and exact undo are
  still required. No production editor text change was needed.
- `ui_driver` inspected runtime readbacks and retained lossless PNG copies in
  `build/household-t3-20260910/readbacks`. Upright/tilted box views show coherent
  orientation-dependent shadows; reader and caption layers fit without visible
  clipping or overlap. Cyrillic Ё/ё and `идёт` are visible. Repeated W/r lines
  are the authored maximum-capacity stress strings, not replacement glyphs.
  The distant maximum-scene view does not establish contact or human feel.
- Final full Vulkan preset after both fixes: **14/14 passed**, exit 0,
  `build/household-vulkan-final.log`. Both household paths, all existing game,
  editor, lighting, audio and character scenarios passed; expected injected
  validation-error detection also passed. Normal scenarios require no
  error-severity validation messages through final resource destruction.
- Live neutral runtime visual execution is delegated to `ui_driver`, with
  exclusive desktop ownership and evidence beneath
  `build/household-t3-20260910/runtime`. Its ordinary executable launches from
  an unrelated working directory using the absolute selected level.
- Ordinary Debug startup from `build/household-t3-20260910/runtime` succeeded
  with the absolute neutral level and executable-adjacent resources. The driver
  verified the exact owned executable and captured authored table, radio,
  letter, boxes and room. An initial untargeted E press correctly caused no
  pickup; the driver was directed to approach and centre a reachable box before
  testing its action. No game defect is inferred from that initial miss.
- Live screenshots confirmed the authored document opens and navigates through
  page 3/3, with readable Russian prose and controls, then closes. Evidence:
  `build/household-t3-20260910/runtime/pickup-attempt.png`,
  `reader-page2.png`, `reader-page3.png`, `radioaim.png`. The filename of an
  attempted pickup is not proof of pickup: the visible target was the document.
  Carry/radio and suspension cases continue as separate bounded checks.
- Live table-box pickup and throw were confirmed with actual target/carrying
  feedback and visible body motion: `runtime/boxaim-3d.png`,
  `runtime/boxhold.png`, `runtime/boxthrow.png` under the T3 run directory.
  Radio had not yet been targeted (the last attempted view faced away from it),
  so its live result remains unverified. No off-target attempt is counted as a
  successful or failed radio toggle.
- Independent read-only review accepted measurement step ordering, matching
  profiles, real awake/action gates, paired Release sampling and query-drained
  raw-file preservation. Updated Debug/Release measurement binaries built
  successfully after the window fix; logs:
  `build/household-measure-final-debug-build.log`,
  `build/household-measure-final-release-build.log`.
- Debug and Release functional measurement checks both passed baseline and
  capacity workloads (four runs, exit 0), delegated to `ui_test_runner`.
  Driver logs: `build/household-measure-debug-check.log`,
  `build/household-measure-release-check.log`. Raw workload/frame data,
  readbacks and hardware/configuration are retained beneath
  `build/household-t3-20260910/debug-check` and `release-check`.
  These checks established the actual 16-awake/action/rest/actor/door gates;
  their timing rows are not used for performance acceptance.
- Six paired Release timing runs completed with exclusive desktop/GPU ownership:
  `powershell -NoProfile -File scripts/measure_household.ps1 -OutputDirectory
  build/household-t3-20260910/release-timings`, driver exit 1, log
  `build/household-measure-release-timings.log`. All six executable exits and
  physical workload gates passed, including all sixteen awake bodies during
  capacity sampling. Raw timing/workload CSVs, hashes, hardware/configuration,
  per-run summaries and failed `acceptance.json` remain in that fresh directory.
  No performance pass is claimed: every run failed all three interval gates.
  Across runs, worst p50/p95/p99 intervals were 31.8906/46.5817/47.1483 ms.
  CPU p95 stayed at or below 2.0742 ms and GPU p95 at or below 6.20448 ms.
  Acquisition wait accounts for nearly all delay (p50 30.49–30.78 ms and
  p95 45.24–45.51 ms), including the zero-box baseline. This localizes the
  measured delay to presentation pacing but does not establish its cause;
  foreground/execution conditions and earlier P10 evidence were checked next.
- A bounded review of the accepted P10 measurements reproduced the same
  acquisition-delay pattern in its failed wrapper run. Its archived direct
  invocation and ordinary-desktop batch passed with the same binary/settings;
  the underlying Windows/driver cause was not established. The direct P06
  baseline diagnostic and full repeat below preserve all six failed runs.
- Independent UI authoring could not start because the current desktop became
  locked. `ui_driver` identified foreground HWND 3608398, PID 18484,
  `C:\Windows\SystemApps\Microsoft.LockApp_cw5n1h2txyewy\LockApp.exe`.
  The responding editor PID 4740 (exact Debug executable, HWND 52039196) could
  not gain focus; the helper refused input. No second scene was authored or
  saved, and no lock-screen interaction was attempted. Desktop authorization
  remained valid; UI work and the controlled performance retest resumed after
  the user unlocked Windows. The later lock observation does not prove
  that the earlier six timing runs were affected by the lock screen.
- The user resumed the authorized desktop run later on 2026-09-10. UI input
  succeeded, and the driver saved a new scene through the editor at
  `build/household-t3-20260910/authoring/second-room.level.json`.
  Read-only inspection confirms v10, two separately positioned boxes, the
  Russian title `Вторая комната`, two distinct Russian/Latin pages including
  Ё/ё, and an initially-off radio linked to an apartment-radio prop without
  collision proxies and a captioned, looping spatial ambience source.
  Reopen and ordinary Play were subsequently verified below. This saved-file
  milestone alone did not close task 9.3.
- The reopened editor initially appeared not to restart Play. Its unchanged
  `Game exited with code 0` label belonged to the child the driver had closed,
  rather than proving a new child had exited. Leaving persistent viewport
  navigation with Escape restored UI interaction and produced a fresh child
  (PID 19744); its observed command line named the saved second-room file and
  `--entry default`. No process-launch source fix was needed. The runtime helper
  also now sets its thread DPI context before input/capture coordinates;
  [Microsoft documents the context-dependent API coordinate behavior](https://learn.microsoft.com/en-us/windows/win32/hidpi/high-dpi-improvements-for-desktop-applications).
  Earlier floor/sky-only captures were inconclusive; the stationary check below
  subsequently established camera direction and object visibility.
- The subsequent input diagnostic reported editor DPI 96, matching requested
  and actual cursor coordinates, full-desktop clipping and all tested buttons
  released. A DPI mismatch was therefore not established as the cause of these
  captures. The finite floor's far edge had been mistaken for a horizon;
  horizontal views omitted the nearby floor objects below the frame. The fresh,
  stationary crouched look-down check below resolved object visibility.
- That fresh stationary check confirmed both boxes and the document in the
  saved scene (`authoring/visibility-crouch-down.png`, child PID 21612).
  The decisive input held Ctrl and applied a positive-Y look-down movement.
  No rendering defect was found. A later approach overshot the document, so
  it did not establish reader activation. Runtime actions and the remaining
  history/reference-repair/fresh-pose checks for this scene stay open.
- The next stationary second-room run confirmed the explicit document hint,
  E opening and D advancing to page 2: `authoring/final-doc-aim.png`,
  `final-reader1.png`, `final-reader2.png`. The helper then sent Escape while
  holding Ctrl for crouch, invoking Windows Start; this was an operator shortcut
  collision, not evidence of a game input defect. Subsequent checks use E to
  close the reader or release Ctrl before Escape. Literal helper inputs and
  capture geometry are retained in `build/household-runtime-actions.jsonl`.
- On the resumed desktop, the direct Release baseline diagnostic passed its
  workload and all T1 gates (native exit 0): interval p50/p95/p99
  16.6701/17.3880/17.7637 ms, CPU p95 1.8052 ms, GPU p95 10.09072 ms,
  acquisition p95 16.1977 ms. Raw CSVs are in
  `build/household-t3-20260910/direct-baseline`; driver log:
  `build/household-measure-direct-baseline.log`. Hash preflight confirmed the
  exact executable and both scenes match the first six-run series.
- A matching Start-Process/Hidden baseline control on the same resumed desktop
  also passed timing gates: interval p50/p95/p99 16.6761/17.3819/17.6316 ms,
  CPU p95 1.2799 ms, GPU p95 5.25928 ms. Its diagnostic wrapper did not retain
  a usable native exit code, which remains unavailable rather than inferred
  from timing output. Evidence is in `wrapper-baseline` beside the direct run.
  This comparison does not support blaming the invocation method or changing
  renderer scheduling. The full repeat of the original, unmodified six-run
  script below supplies the performance acceptance evidence.
- The original six-run script completed on the resumed desktop with exit 0:
  `build/household-measure-desktop-repeat.log`. All six native exits were 0,
  every physical workload passed and `release-desktop-repeat/acceptance.json`
  records six samples with no failed or unavailable gates. Each run contains
  3597–3601 post-warmup samples through at least 69.98 s, all submitted at
  1920x1080. Worst CPU/GPU p95 across the series was 2.3365/11.57372 ms;
  worst frame p50/p95/p99 was 16.6749/17.4027/17.8782 ms. All T1 thresholds
  passed. Configuration/hardware, hashes, raw frame/workload CSVs and per-run
  summaries are retained in that directory. The executable hash is unchanged
  from the failed series; no renderer, scene, script, driver or power-setting
  change was made to obtain this repeat. Earlier failed data stays retained.
  The timing regression was not reproducible on the resumed desktop; the
  lower-level OS/driver mechanism remains undetermined. Task 9.4 is verified.
- The ordinary Play run of the UI-authored second room also confirmed box
  pickup, hold, E drop and right-mouse throw. Captures under `authoring/` are
  `box1-hint-final2.png`, `box1-hold-final.png`, `box1-drop-final.png` and
  `box2-throw-final.png`; the throw capture includes the accepted-action feedback
  and airborne body. Together with the two reader pages, these establish the
  second scene's box/document behavior. Radio and editor history/reference
  repair/fresh-pose checks remain open; no human feel or listening is inferred.
- Second-room radio on/off visual transitions passed. The explicit
  interaction hint is retained in `authoring/radio-hint-final.png`;
  `radio-on-final.png` shows the on indicator and radio caption together.
  `radio-off-final.png` shows off feedback with no active radio caption.
  Review of `radio-muted-on-final.png` and `radio-muted-off-final.png` found no
  active hint/feedback/caption or visible state transition. The input receipt
  shows crouch was released before these attempts, moving the target below
  the reticle. These two captures do not verify muted radio actions, despite
  the driver's initial pass report; that check remains open. Physical
  audibility/listening is also unverified.
- The first editor-history attempt committed box-1 yaw 45 and duplicated it
  as selected box-3 at X -0.8, then deleted the copy. Its subsequent Undo and
  document-edit claims were rejected on screenshot review: the captures showed
  the unchanged Open Level modal, not restored objects or edited text. The
  receipt identified a File-menu click at X 20 where Edit required X 60.
  Those history results remain unverified until the corrected UI sequence;
  no application defect is inferred from this input error.
- The user identified four editor instances during this UI run. Process times
  and launch receipts confirmed that the temporary desktop helper had launched
  a fresh editor for each runtime case while overwriting its tracked PID.
  UI automation was paused; three older owned instances were gracefully closed
  and current PID 6876, including its unsaved scene, was preserved. A subsequent
  process check confirmed exactly one responsive editor. The helper now reuses
  a sole matching editor, refuses ambiguous multiple instances, validates the
  tracked executable and records PID in each receipt. Its PowerShell syntax
  check passed. This was test-automation process ownership, not a native editor
  child-process defect. The accepted Release series finished before these four
  launches (acceptance artifact 20:25:05 UTC; earliest editor 20:26:56 UTC), so
  its measurement conditions are unaffected by this later cleanup incident.
- Corrected box/document history subsequently passed in the single retained
  editor. `authoring/history-doc-redo-final.png` shows the committed replacement
  text in Properties and Readable preview; `history-doc-undo-final.png` shows
  the original first page restored. `history-box-undo-delete-final.png` restores
  selected box-3, `history-box-redo-delete-final.png` removes it again, and
  `history-box-two-original-final.png` shows the two original boxes with box-1
  selected at yaw 45 after undoing duplication. These images were reviewed
  directly; the earlier misnamed/no-op history captures are not acceptance.
- Radio reference authoring also passed in that editor. Renaming the prop and
  source to `receiver-prop` / `receiver-source` updated radio-1's selectors
  atomically (`authoring/radio-link-source-ref.png`). Duplicating the source
  created `source-1`; deleting the original produced the unresolved-source
  diagnostic (`radio-link-broken.png`). Selecting the duplicate restored valid
  state (`radio-link-repaired.png`); undoing repair restored the error
  (`radio-link-undo-broken.png`). Undoing deletion and duplication left the
  original renamed source and valid links (`radio-link-final-valid.png`).
  Root directly reviewed these state/diagnostic transitions.
- Changed-pose Save-and-Play passed (`authoring/saveplay-prompt.png` and
  `saveplay-fresh-pose.png`). The fresh child PID 19716 loaded the saved second
  room and presented the changed first box with its pickup hint. Read-only
  inspection confirms box-1 yaw 45, two boxes, the restored original two pages,
  and valid renamed radio links. The actual UI-authored file is retained
  byte-for-byte at [evidence/second-room.level.json](evidence/second-room.level.json),
  SHA256 `8E0EC6EDBE7B05A80D4603932D774721C999CC7AF2FF152206185D2434C94107`.
  No JSON/script-generated scene substituted for UI authoring. The child and
  editor closed gracefully; root verified zero remaining editor/runtime
  instances before the next scenario. Task 9.3 is verified.
- The next ordinary neutral-scene run (PID 21668, no editor running) verified
  normal and muted radio transitions while keeping the target under the reticle.
  Evidence in `runtime/`: `primary-radio-aim.png`, `primary-radio-on.png`,
  `primary-radio-off.png`, `primary-radio-muted-on-state.png` and
  `primary-radio-muted-off-state.png`. Root reviewed the muted pair: on hint,
  indicator and caption are visible together; off changes the hint/feedback
  and clears the active caption. This closes the earlier inconclusive muted
  attempt. Listening and physical audio output remain unverified.
- Reading recovery passed in that same runtime. `runtime/paused-d.png` retains
  page 2/3 after D while paused; `page3.png` shows that a fresh D after resume
  advances normally, and A returns to page 2 (`page2-return.png`). Native
  minimize/restore retains page/text (`minimize-restore.png`); resize to a
  1304x841 client also retains a readable page 2 (`resize-reader.png`). Root
  reviewed the page-state and resized-text captures. No separately moving
  world object is visible in these reader views, so these images establish
  reading/input recovery rather than independently proving world-time freeze.

## Automated verification selected on 2026-09-11

The user explicitly requested scripts/code checks and stopped manual testing.
The visual agent was stopped; root verified no remaining editor/runtime process.
This replaced the remaining manual functional execution in task 9.2, while
preserving the gameplay requirements and leaving acceptance in 9.5 open at
that time. The later acceptance decision is recorded separately below.
No further window launches, screenshot interaction or desktop input were used
for this run. The last floor-box desktop attempt had not established wall
contact or carry pause/minimize recovery and is not counted as that evidence.

`scripts/check_household.ps1` configures/builds Debug and runs the complete
non-GPU CTest preset by default. It retains separate logs, JUnit and a JSON
summary, refuses an existing output directory and propagates native failure
codes. Vulkan readbacks are an explicit opt-in; it contains no desktop input
automation. The UI test runner executed:

```powershell
powershell -NoProfile -File scripts/check_household.ps1 -OutputDirectory build/household-automated-20260911
```

Configure, build and CTest each exited 0. All **561/561** tests passed, with zero
failed, skipped or not-run tests: 549 unit (including 29 real-ImGui), six boundary,
five process and one CPU household-fixture check. CTest took 100.95 s. Artifacts:
`build/household-automated-20260911/{configure.log,build.log,debug-tests.log,debug-tests.xml,result.json}`;
driver log: `build/household-automated-driver.log`. No interactive application
process remained afterward. Vulkan and performance were not repeated because
this continuation changed only tests, the verification script and documentation.

Root reviewed three strengthened existing regressions: a held box makes finite
progress while lagging its target after one step, an obstructed door closes on
the next action and opens on the following one, and carrying refuses interaction
with an actually reachable door while empty hands can lock/unlock/open it.

The following existing/strengthened tests provide functional evidence for the
remaining scenario; names are searchable in the linked sources:

| Behavior | Automated evidence |
| --- | --- |
| Hold follows physically, safety release does not relocate | [Physics tests](../../../../tests/core/test_household_physics.cpp): `StillHoldFollowsNewTargetAndInvalidOrDistantTargetsRelease` |
| Hold/throw against walls; fast spinning contacts | Physics: `BlockedHoldAndNearWallThrowNeverRelocateBox`, `MaximumSpeedSpinningCubeCannotCrossThinWallOrDoor` |
| Player contact without propulsion or box support | Physics: `MovingBoxHitsStationaryStandingAndCrouchedPlayerWithoutPropulsion`, `ControlledMotionPushesFreeBoxWithoutUsingItAsAStair`, `IsolatedTiltedAndStackedLandingsDoNotCreateSupport` |
| Actor waits/resumes; free/held boxes stop doors until a new action | Physics: `ActorWaitsAtBoxThenResumesAndCannotBeMovedByAnImpact`, `DoorStopsBeforeFreeOrHeldBoxAndRequiresNewPress` |
| One fixed-step command; drop outranks throw; occupied hands | [Runtime tests](../../../../tests/core/test_household_runtime.cpp): `PickupAndThrowWaitForOneBoundaryAndKeepAcceptedDirection`, `DropOutranksThrowAndPendingCommandsRefuseCompetition`, `CarryingKeepsFlashlightAndExcludesTargetedWorldActions` |
| Reader controls, active world and suspended state | Runtime: `ReadingRetainsStanceAndBlocksInheritedActionsAndEscape`, `ReaderPagesAndFeedbackSurviveSuspensionWithoutReplay`, `ReadingAllowsConcurrentCharacterBoxAndRadioActivity` |
| Radio/mute/caption lifecycle and fresh-run reset | Runtime: `RadioOnOffRestartUsesOneOwnedSourceAndCurrentCaptions`, `FreshRunAndLaterAuthoringOwnIndependentState` |
| Suspension cancels pending action but preserves owed safety release | Runtime: `SuspensionCancelsRequestWhileSafetyReleaseSurvivesRecapture` |
| Real window/Engine recovery, geometry/text and lifetime | Earlier passing `vulkan_household_interactions` in [runtime smoke](../../../../tests/core/runtime_household_smoke.hpp); earlier desktop reader recovery described above |

Headless tests exercise logical input/state and real simulation; they do not
prove physical keyboard delivery or visible recovery by themselves. Earlier GPU
and desktop evidence retains that separate scope. Subjective hold/throw feel and
physical audio listening are not inferred from any automated result.

The wrapper's failure path was also checked using a copy in a fresh temporary
root without CMake presets. Configure failed with native exit 1; the script
returned 1, recorded `passed: false` and ran no build/tests. Evidence:
`build/household-check-failure-6efd85dae82a4bc2a7610d74708185a9/`.

Final review: the strengthened test assertions and script were inspected;
PowerShell parsing, new-file whitespace/conflict checks, `git diff --check` and
`openspec validate add-household-interactions --strict` passed. Retained logs:
`build/household-automated-diff-check.log` and
`build/household-automated-openspec-validation.log`.

## Final acceptance decision — 2026-09-26

After being told that 40/41 tasks were closed, the retained automated/GPU/
performance checks had passed, and task 9.5 remained open for T3 acceptance
with subjective feel and listening unverified, the user requested resolution
of the remaining P06 item. P06/T3 is accepted on the retained evidence with
those limitations explicitly accepted. No further manual run is planned.

A read-only independent review confirmed the retained 2026-09-10/11 evidence:
the Debug JSON/JUnit records show 561 tests with no failures, skips or disabled
tests, including 29 real-ImGui checks; the final Vulkan log shows 14/14 passing;
the Release acceptance, run summaries and raw timing/workload CSVs retain all
six successful runs and their passing gates. The tracked second-room scene
matches the retained UI-authored file, and the referenced authoring/Play
captures remain present. This review checked records and artifact integrity;
it did not repeat execution or provide new visual acceptance.

The status table and earlier coverage mapping consolidate tasks 8.1–9.4.
Earlier failed measurement attempts and inconclusive manual observations stay
in this record; the controlled passing Release repeat is the accepted timing
series. The following limits remain part of this acceptance:

- Subjective hold/throw comfort and physical audio listening remain unverified.
- Automated checks do not establish subjective quality or physical input
  delivery; their scope remains separate from the retained desktop observations.
- Acceptance changes no gameplay requirements, supported physical profile or
  timing gates, and does not turn earlier inconclusive observations into passes.

Task 9.5 is closed and `tasks.md` now records 41/41 completed tasks. The user
decision, together with the consolidated evidence and accepted limitations,
establishes T3 acceptance; artifact readiness or passing tests alone does not.
This closure changes documentation only and does not repeat builds, tests,
GPU runs or measurements. At acceptance, main-spec synchronization and archival
were still pending; their subsequent completion is recorded below.

## Archive handoff — 2026-09-26

All 12 P06 delta capabilities were synchronized into main specs: the new
`household-interactions` capability and 11 existing capabilities, with 21
requirements added and 13 modified. Existing Purpose sections, unrelated
requirements and prior scenarios were preserved. The main persistence spec
now describes v10 with the accepted v2-v9 compatibility contract.

A comparison of every delta requirement against its merged main spec found
nothing left to apply. Independent review found no semantic or formatting
issue in the merge. Strict validation passed for P06 and all 30 main specs;
logs are retained under `build/p06-archive-20260926/`.

The complete change, including `.openspec.yaml` and its authored evidence, was
moved to `openspec/changes/archive/2026-09-26-add-household-interactions/`.
All 41 tasks are closed. The accepted subjective feel/listening limitations
remain unchanged; no additional runtime, GPU or performance run was made for
this documentation-only handoff. P05 is next for detailed planning against
the synchronized main specs; no dependent implementation was started.
