# T4 validation record

Status: implementation, no-window verification and the injected runtime GPU
scenario pass. Independent authoring, integrated GPU regressions, performance
and visual/human acceptance are in progress or remain open. No T4 acceptance
claimed.

## Baseline

Initial working tree, 2026-09-27:

```text
 M openspec/changes/add-narrative-state-and-sequences/proposal.md
?? openspec/changes/add-narrative-state-and-sequences/design.md
?? openspec/changes/add-narrative-state-and-sequences/specs/
?? openspec/changes/add-narrative-state-and-sequences/tasks.md
```

These planning changes predate implementation and must be preserved.
`openspec validate add-narrative-state-and-sequences --strict`: exit 0.
Independent read-only baseline review: no blocking drift. P06/P07 are archived
and synchronized; all ten current packaged scenes are v10. Existing owner APIs
and fixed/active clocks match the design. `Engine::tick` currently hands
character audio off before ordinary interactions; task 5.2 deliberately moves
this after accepted interactions and narrative cancellation preflight.
`CharacterController::cancel` and `CueCoordinator::cancel` still require new
owner-side expected-instance checks. Existing P06/P07 acceptance limitations
(latency, listening and subjective feel) are not reclassified as T4 evidence.
Baseline statements above describe the starting tree, before implementation.

## Fixtures and expected results

- Ordinary neutral scene (`resources/levels/narrative-t4.level.json`): crossing a region
  enables a light, plays a captioned one-shot, delays, runs an actor route and
  sets a completion fact. Initially-on radio switching off cancels the run.
  Normal completion sets the fact once; later region entries do not replay it.
  Cancellation retains accepted state changes, cancels only the matching cue
  or route instance, and prevents all remaining steps and the completion fact.
- Door variation: endpoint request followed by actual endpoint wait; an
  obstructed door stays stopped until another explicit request. Refusal fails
  the event and prevents subsequent steps.
- Interaction/busy variations: accepted box/document actions trigger once;
  refused/queued/safety actions do not. Busy source/actor work remains foreign,
  is retried at most once per boundary, and cannot be canceled by the waiter.
- Independent variation: to be authored through semantic editor controls in a
  disposable scene, with different region/link arrangement, step order and
  cancellation. Preserve the saved result and scenario transcript. No script
  may manufacture its narrative content. Repair a deliberate broken link,
  save/reopen and undo/redo to the saved baseline before ordinary Play.

## New capability scenario coverage

The check column assigns a reproducible verification scope. The evidence ledger
below records actual results; unit coverage does not close integrated or human
acceptance of the same scenario.

| Capability / scenario | Assigned check |
| --- | --- |
| narrative-progression / Player starts inside a region | Reducer occupancy initialization versus scene-entry tests (4.1) |
| Definition is outside the supported profile | Device-free profile/ID/bounds tests and Save/Play preflight (2.1, 2.2, 6.4) |
| Region is crossed within one frame | Ordered multi-step crossing and thin-region tests (4.1, 5.1) |
| Requested action is refused | Accepted-outcome owner tests and integrated interaction tests (3.5, 5.1) |
| Door movement has only been requested | Endpoint predicate tests against moving/stationary owner snapshots (3.2, 4.1) |
| Cancellation and continuation coincide | Frozen snapshot cancellation-preflight tests, including pending sound marker (4.4, 5.2) |
| Definitions are reordered | ID-order conflict/contention tests with permuted storage (4.4) |
| Event changes another event's condition | Next-boundary causal effects and bounded cycle tests (4.4) |
| Busy event is inspected | Snapshot/trace reasons, target identity, no wait spam (4.5) |
| Process starts again | Initial facts/history reset and unchanged authored bytes (4.2, 5.4, 8.2) |
| authored-sequences / Sequence waits for an actual door endpoint | Door command versus endpoint wait integration (3.2, 4.3, 8.1) |
| Invalid sequence is authored | Strict per-kind/reference/duration/self-reference tests (2.1–2.3) |
| Once event is revisited | Once completion/cancellation/failure and repeated entry tests (4.2, 8.2) |
| Repeat condition remains true | Post-terminal false observation and fresh rising-edge tests (4.2) |
| Another route is already active | Busy/already-active ownership tests (3.3, 4.3) |
| Owned action is replaced | Snapshot ownership-loss failures and replacement preservation (3.3, 3.4, 4.3) |
| Door path clears after obstruction | Physical obstruction and explicit retry tests (3.2, 4.3, 8.1) |
| Completed action is reused within a boundary | Cue and route reuse in both event-ID orders (4.4) |
| Run is canceled during playback | Matching cue/caption stop, retained light, no later steps (3.4, 4.4, 8.2) |
| Old run is canceled after replacement | Expected-instance cancellation for both concrete owners (3.3, 3.4, 4.4) |
| Delay spans a long active frame | Injected active-time deadline and no-backdating tests (4.3, 5.3) |
| Output is muted or unavailable | Logical cue/caption ordering across output modes (3.4, 8.2) |
| Cancellation occurs during the delay | Ordinary radio cancellation with route never started (8.1, 8.2) |
| Independent variation is played | Retained semantic-authored scene plus separately authorized saved-file Play (10.2, 10.3) |

Additional acceptance cases: zero/one/multiple fixed-step batches, initial true
conditions, guarded/active trigger consumption, elapsed bounds, actual box
ownership, overflow failure, last-256 trace retention, reading, owed cursor
release, pause/minimize input suppression, startup failure and teardown are
assigned to tasks 3–5 and the T4 runner in 8.2. GPU recovery and coherent
presentation are assigned to 8.3/10.1. All modified-capability regression
requirements remain in the task checklist.

## Evidence ledger

| Evidence class | State | Required retained evidence |
| --- | --- | --- |
| Definition/codec/reducer/owner unit tests | Passed, 81/81 | `build/narrative-focused-tests-3.log`, exit 0 |
| Integrated Debug/automation/Python checks | Passed after fixes; two explicit skips | Final CTest: 702 passed, 0 failed, 2 skipped, exit 0; pinned Python suite 120/120 and protocol 29/29 passed |
| A: no-window editor/semantic tests | Passed after ID fix | EditorNarrative 11/11, four corrected legacy regressions, SemanticEditorUi.Narrative 3/3, editor narrative preflight exit 0; `build/narrative-ui-checks/final-targeted-results.json` |
| B: SDK to production host to real editor | Two failed attempts retained; fixes under verification | Native fixture allowlist and fact-ID Tab commit defects found; disposable sessions closed |
| C: runner-visible four MCP tools | Visible; execution not run | Runner discovered all four tool names; no desktop session started. SDK is not proof of four-tool execution |
| Vulkan game/editor smoke | Runtime T4 passed; full suite pending | 83 injected checks and 16 snapshots, zero Vulkan errors through teardown; `build/narrative-runtime-gpu-2.*` |
| Independent UI-authored scene | Not run | Saved fixture, history/repair scenario and independent inspections |
| Visual picking/placement/ordinary Play | Not run | Separately authorized delegated run and screenshots/observations |
| Human listening | Unverified | Human observations of cue order, cancellation and clarity in both scenes |
| Release performance | Debug workload passed; first Release attempt interrupted | Partial first run retained in `build/narrative-authorized-release-measure-1/`; no timing acceptance. Three complete paired runs remain required |
| Independent lifetime/timing/compatibility review | Resolved and runtime GPU rechecked | Reviewed snapshot classification, instance ownership, v11 compatibility, editor typed links and clocks. Fixed P04 priority, unmet-condition trace detail and injected-clock selection/anchor |

Retained build logs: `build/narrative-world-build-confirm.log`,
`build/narrative-runtime-build-2.log`, and
`build/narrative-integrated-build-2.log`, all exit 0. Focused command:
`build/debug/tests/engine_tests.exe --gtest_filter=Narrative*.*:AcceptedInteractions.*:LightSwitchController.*:DoorGameplay.*:DoorInteraction.*:HouseholdRuntime.*:CharacterRoutes.*:CueCoordinator.*`.
Earlier failures are retained in `build/narrative-focused-tests.log` and
`build/narrative-focused-tests-2.log`: two tests incorrectly overlapped Jolt
owners, and one codec test expected semantic validation during safe structural
decoding. Corrected the tests to exercise the actual ownership and validation
boundaries, preserving refusal/repair assertions; the third run passed.

Pinned Python results: `build/narrative-editor-python-venv.log` and
`build/narrative-editor-protocol.log`. The system-Python attempt failed because
that interpreter lacks MCP; it is not a product failure or passing evidence.
Current no-window SDK doctor: `build/narrative-ui-checks/doctor.json` and `.jsonl`,
exit 0, with the narrative fixture registered. No GPU or visual claims follow.
Historical scene retention is recorded in `build/narrative-originals-check.json`;
the ten originals under `tests/fixtures/levels/v10/` match their pre-change bytes.

Review follow-ups: automatic IDs now skip unresolved typed narrative links,
including links to existing owner records; focused Add and door-Duplicate
regressions passed after rebuilding. The semantic scenario now retains the editor-saved
variation byte-for-byte beside its transcript before host temporary cleanup,
with a SHA-256 in the report. Its syntax check passed; desktop attempts are
recorded below.

First full CTest: `build/narrative-ui-checks/ctest-results.json` and
`build/narrative-ui-checks/ctest-debug.log`, 426.846 seconds. Four failures were
remaining v10 save assertions / removal of the new empty narrative field before
historical content comparison. The source boundary check rejected the new JSON
development evidence writer; its exact private implementation/target exceptions
now include only the two narrative evidence executables and the codec migration
test. Runtime/public boundaries remain enforced. The corrected standalone
boundary check passed (`build/narrative-boundary-check.log`, exit 0). The final
integrated recheck below passed after rebuilding. The two skipped tests
require Windows symlink privileges and are retained as skipped, not passed.

Final prepared binaries: all affected Debug targets built with exit 0 in
`build/narrative-final-debug-build-3.log`. Release `narrative_measure` built with
`cmake --build build/p10-release --target narrative_measure -j4`, exit 0,
40.624 seconds, `build/narrative-final-release-build.log`. Existing Vulkan
aggregate-initializer warnings remain. The earlier development report compile
failure (read-only access to `CueCoordinator::playback`) was fixed with a const
accessor; the final build contains that fix.

No-window native checks passed: all five T4 fixture/resource variants in
`build/narrative-final-preflight/report.json`, and identical measurement profiles
in `build/narrative-measure-final-preflight/` with command output in
`build/narrative-measure-final-preflight.log`. Both returned exit 0.

Independent clock review found that switching from ordinary to injected time
was allowed and a first injected zero sample included a wall-clock gap. Engine
now fixes its mode on the first tick, rejects either switch before changing
state, and anchors injected samples to the constructor's last accepted audio
timestamp. Actual-diff re-review found the issue resolved; the GPU runner adds
first-zero and both switching-direction assertions, now passed in runtime GPU
run 2. Measurement bookkeeping is excluded from active CPU timing but remains
part of frame intervals, as recorded in the development instructions.

Final `git diff --check` and strict OpenSpec validation pass, exit 0,
in `build/narrative-final-diff-check.log` and
`build/narrative-openspec-validation.log`. Final full Debug CTest passed against
these fresh binaries: 702 passed, 0 failed, 2 skipped of 704 tests, exit 0,
437.211 seconds. Evidence: `build/narrative-ui-checks/final-ctest-results.json`,
`final-ctest-debug.log` and `final-summary.json`. Both narrative native/editor
preflight registrations and the source boundary check passed. Skipped tests:
`EditorFilePolicyCodecDeathTest.DanglingSiblingTemporaryLinkIsNeverFollowed`
and `RestrictedEditorFiles.SymlinksInTheOwnedRootAreRejected`.
These were the pre-authorization results; desktop results follow below.

Performance protocol: events disabled versus the supported-capacity workload;
verify actual execution, retain warm-up and raw CPU/GPU/frame p50/p95/p99.
Use the DEVELOPMENT.md T1 gates unchanged (CPU/GPU p95 <=16.67 ms, frame
p50/p95/p99 <=16.9/20/33.4 ms for every run). Do not run competing builds or
GPU work while measuring.

Unavailable, blocked, failed and unknown checks are distinct from passes.
Only retained evidence or an explicit user acceptance decision can close an
acceptance limitation; no such decision has been made.

## Prepared desktop acceptance sequence

Authorized run follow-up: first SDK attempt stopped before any scenario action
because the native session allowlist omitted `narrative-t4` while the host
manifest included it. Retained `build/narrative-authorized-sdk.jsonl`,
`.summary.json`, and `-editor.stderr.log` distinguish startup failure from
mutation failure. The child exited; no saved variation was produced. The native
allowlist is corrected and a real no-window SessionController T4 start/select
regression passed (1/1, `build/narrative-allowlist-tests.log`), after fresh
automation build. A new scenario attempt uses a fresh transcript; the failed
session is not replayed.

Second SDK attempt (`build/narrative-authorized-sdk-2.*`) reached real edits,
then failed request 20 step 1: editing `narrative-id` to `semantic-done` with
Tab reported success, but independent applied inspection retained `fact-1`.
Effects were partial in the disposable document; no saved variation exists.
Close succeeded and cleanup was released; source bytes were preserved and
Vulkan teardown reported zero errors. The failed assertion remains intact.
Tab wrapped to the fact's only input instead of deactivating it because ImGui
keyboard navigation was disabled. Production and matching in-memory editor
contexts now enable keyboard navigation, with a regression covering combined
edit/Tab, deferred edit/separate Tab, independent applied IDs and undo history.
Affected targets rebuilt successfully in
`build/narrative-navigation-integrated-build.log`. External overlay API-version
warnings are retained separately from validation errors.

First authorized runtime GPU run: `build/narrative-runtime-gpu-1.log` / `.json`,
CTest exit 8 in 23.246 seconds. Report
`build/debug/bin/build/narrative-runtime-checks/281441309415500/report.json`
retains 73 passing checks, 14 snapshots and zero Vulkan errors through cleanup.
Normal/muted sequences, cancellation stages, owned shutdown, door/refusal/busy,
same-boundary sound cancellation, minimize/restore and accepted interactions
passed. The startup-failure fixture incorrectly requested a lazy frame-upload
failure while only constructing Engine; no exception was expected at that stage
by the implementation. The test now uses the existing constructor-stage
`world_mesh_upload` failure and exact diagnostic. Its acceptance assertion is
unchanged. Region rearm and P04 arbitration were not reached in that failed run.

Explicit runtime GPU rerun passed after the constructor-hook correction:
`build/narrative-runtime-gpu-2.log` / `.json`, exit 0 in 26.116 seconds.
`build/debug/bin/build/narrative-runtime-checks/281756346531500/report.json`
contains 83 passing checks, zero failed checks and 16 snapshots. Both partial
startup failures, recovery, region rearm, both clock-switch refusals and final
P04 arbitration were reached. Vulkan errors remained zero through destruction;
the owned process exited and GUI ownership was released. Silent/muted logical
checks do not establish physical listening.
The fixture supplies an ordinary `RuntimeConfig::level_path`; Engine creates
progression from the loaded level definitions without filename dispatch.
The normal wall-clock Engine construction/tick and all five authored variants
were exercised. This verifies task 8.1's saved definitions/runtime boundary;
interactive external-editor Play and the independent variation's visible
behavior remain separate in task 10.3.

Post-navigation no-window batch: 40/41 automation tests passed, exit 1 in
277.406 seconds (`build/narrative-navigation-tests-results.json` and
`build/narrative-navigation-tests-automation.log`). Both new narrative session
regressions passed. `RealVectorColorComboAndCheckboxUseEngineInput` failed with
the client decoder's partial-JSON-line timeout; its exact request/effects are
unknown because the harness had no per-request diagnostics. Independent review
found the test client rendered a complete UI frame between every 4096-byte
read, creating its own pipe backpressure. The test-only client now drains
fragments before rendering, expires incomplete responses using the unchanged
deadline, and records bounded request/byte diagnostics. A real large-response
regression was added; production protocol deadlines remain unchanged. The
updated test target built successfully (`build/narrative-receive-build.log`).
Live B/C were held during this correction, and the failed test process exited.

Independent review also found that keyboard navigation requires explicitly
releasing panel focus when RMB captures the viewport. The production bridge
now clears focus and already-queued ImGui navigation requests at that handoff.
Real-ImGui regressions cover committing the draft once, same-frame Space,
held camera keys without document edits, and subsequent Tab editing. Native
GLFW capture and visual navigation remain separate acceptance. The affected
engine/editor binaries rebuilt (`build/narrative-focus-build.log`), and all
21 EditorUiInteraction tests passed in 2.677 seconds, exit 0, including both
new focus-handoff regressions (`build/narrative-focus-tests.log` / `.json`).

Superseded on 2026-10-07: global keyboard navigation made every focused panel
capture the keyboard and let Space/Enter/arrows drive widgets, so it was
removed together with the private ImGui navigation resets. Draft text inputs
now detect ImGui's Tab wrap to a pane's sole, still-active input one frame
after Tab and release focus, which commits the draft; other Tab results keep
ordinary input-to-input tabbing. The viewport handoff only releases panel
focus. Real-ImGui regressions cover sole-input Tab commit (in-memory editor
and automation session), Tab to the next input in a multi-field pane, and held
camera keys during navigation; a mutation run confirmed both sole-input tests
fail without the release. Debug build and `ctest --preset debug` passed
(711 tests). Live B/C editor runs were not repeated for this correction.

Debug measurement workload check passed for disabled and capacity profiles:
`build/narrative-authorized-debug-measure-1/`, both child exits 0, retained
workload reports and raw CSV. The capacity profile executed 64 events with
32 steps each; this is functional evidence only. External overlay warnings
remain distinct from validation errors.

First Release attempt was interrupted before sampling acceptance. Retained
`build/narrative-authorized-release-measure-1/disabled-1.csv` contains 389
submitted 1920x1080 frames followed by a zero-extent/non-submitted frame at
6.573 seconds; workload report records interruption by close/resize/suspension.
The first child exited 1 and the driver stopped, preserving all partial data.
No timing pass is claimed and no thresholds were changed. Subsequent paired
measurements require an uninterrupted desktop interval.

The root instruction in `AGENTS.md:193` requires explicit authorization for
this desktop run. The user explicitly authorized GPU tests, the editor SDK
scenario and fullscreen Release measurements on the current desktop on
2026-09-27. Prepared binaries and fixtures are ready; the ordered runs below
must not overlap GUI sessions or performance
sampling:

1. Delegated semantic narrative scenario with fresh transcript/output paths;
   retain the byte-identical second scene, report and cleanup. Separately
   exercise the four runner-visible MCP tools and retain C evidence.
2. Integrated `ctest --preset vulkan-smoke --output-on-failure`, including
   narrative game/editor modes. Inspect failures without replaying unknown
   semantic mutations. Captured region frames are appearance evidence only.
3. Debug functional measurement check, then three paired Release runs with no
   other builds/tests/GPU work. Retain failed/unavailable timing gates unchanged.
4. Separate ordinary saved-file Play and region picking/placement check in an
   environment with a usable desktop executor; then human listening in both
   scenes. Root tool discovery currently exposes no desktop input/screenshot
   executor for that visual scenario. Semantic automation does not support
   viewport picking/placement or game launch and cannot substitute for it.

Visual expectations: selected region has twelve coherent wire edges; moving it
and undo restores its original bounds; upper-floor placement preserves extents
and a nearer wall prevents placement behind it. Normal saved-file Play should
show the accepted light change, captioned cue, delay, actor route and completion;
radio-off cancellation should retain the light and stop only matching owned
work. Reading continues active progression; pause/minimize freezes it and
recovery preserves identities. Obstruction requires the existing concrete door
or actor policy. Screenshots cannot verify cue audibility or clarity.
