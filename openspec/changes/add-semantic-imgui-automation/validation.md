# Implementation validation

## Baseline and prerequisites (2026-09-17)

- Checkout: `b784ec7cd16d8c8366fb9637af08548714597c15`, matching the
  planning baseline. Initial `git status --short` showed only this untracked
  change directory; no existing source edits were present.
- Read the selected proposal, design, both delta specs and tasks, the root
  instructions (no nested instructions found), and affected main requirements
  in development-toolchain, level-editor, level-persistence, platform-windowing,
  player-input and runtime-composition. Consulted the architecture/editor
  ownership, rendering boundary and development validation guidance.
- ImGui remains `v1.92.9b-docking`, fetched commit
  `b48d1afbe8ee8b238e2961dc363a949dd7304e23` (19291). Test Engine is pinned to
  `2628e39cc0ea3a0a612d5d039543c9d4e873c720`. No dependency upgrade is included.
- Existing Debug configuration: Ninja, Clang 23.1.0, GNU frontend, x64 MSVC
  ABI; Vulkan SDK 1.4.321.1. Project presets retain portable compiler names.
- Known out-of-scope discrepancy: main persistence specs describe v9;
  `src/core/world/level_document.hpp` defines format version 10 and the codec
  accepts the household extension. This change does not reconcile household
  specs or alter the file format. Normal editor file/Play and runtime input
  requirements remain in force; restrictions belong only to the test profile.
- `openspec validate add-semantic-imgui-automation --strict`: exit 0.

## Dependency license and reproduction

The operator confirmed eligibility for the **Free License, Article 1.1**, of
Dear ImGui Test Engine License v1.04 during this implementation session. No
particular eligibility criterion or business/revenue status is inferred.
This records the basis for this operator's integration, not eligibility of
other recipients. The pinned engine's license is not MIT. Distributions must
include its copyright notice and complete license (Article 2.2); capture being
disabled does not change that requirement.

The dependency-only probe was inspected and reproduced without a native window,
backend, GPU or physical input. It compiles all eight engine sources and the
four ImGui core sources with C++20, capture/ImPlot disabled and std::thread
coroutines enabled. Exact commands, each exit 0:

```powershell
cmake -S build/semantic-imgui-research/compatibility -B build/semantic-imgui-research/compatibility/out -G Ninja -DCMAKE_CXX_COMPILER=D:/LLVM/bin/clang++.exe -DCMAKE_BUILD_TYPE=Debug
cmake --build build/semantic-imgui-research/compatibility/out --clean-first -j 4
& ./build/semantic-imgui-research/compatibility/out/compatibility_probe.exe
```

Result: `PASS: ImGui 1.92.9b (19291), Test Engine create/start/3 frames/stop/destroy`.
Fresh local evidence: `build/semantic-imgui-research/compatibility/apply-configure.log`,
`apply-build.log`, and `apply-run.log`; no warning/error diagnostics. This does
not validate the persistent dispatcher, real editor integration or Vulkan.

## Initial isolated build

The OFF-by-default `NEAR_LAUGH_UI_AUTOMATION` option gates the full-SHA
Test Engine fetch and separate instrumented ImGui/UI/render/application targets.
The engine license is copied from the pinned source to
`bin/licenses/imgui-test-engine.txt` beside the optional executable; its SHA-256
matches the source (`8d01df5085b3d7c055999188bfc743354b00658be309819eb0415239820b1835`).

Commands (all exit 0):

```powershell
cmake --preset debug -DNEAR_LAUGH_UI_AUTOMATION=ON
cmake --build --preset debug --target level_editor level_editor_automation editor_automation_dependency_probe -j 4
ctest --test-dir build/debug -R '^editor_automation_dependency_probe$' --output-on-failure
```

Logs: `build/semantic-imgui-implementation/configure-on.log`,
`build-variants.log`, `dependency-test.log`. Integrated dependency lifecycle
test: 1/1 passed, no native window. Build: no warning/error diagnostics.
Fetched engine HEAD matches the requested full SHA. Ninja link records show
ordinary editor links only ordinary ImGui/UI/render libraries and automation
links only their instrumented counterparts, sharing the existing core libraries.
Audited 44 ordinary/instrumented ImGui/UI/render/application compile commands:
zero configuration mismatches. No runtime/public-header dependency was added.
This initial build established dependency isolation; later integration evidence
is recorded below.

## Deterministic integration evidence

- Python protocol/host/process/client: `python -B -m unittest discover -s
  tests/tools -p "test_editor_ui_*.py"`, 55 passed, exit 0. Log:
  `python-automation-tests.log`. Includes real Windows host-death cleanup of
  responsive and stalled owned children, unrelated-process survival, worker
  saturation, session-epoch races, UTF-8/framing/response limits and MCP lifecycle.
  A subsequent close/exit-status review added bounded final-exit evidence;
  that rerun passed 67/67, exit 0 (`python-close-tests.log`).
  EOF and refused-startup exit races then received regression coverage;
  that suite passed 72/72, exit 0, 12.495 seconds
  (`python-eof-tests.log`). Final startup ownership and simultaneous EOF
  regressions passed 79/79, exit 0, 20.778 seconds
  (`python-startup-eof-tests.log`), with the final source hashes retained in
  `python-host-frozen-hashes.json`.
- Existing automated UI routing could not start its assigned
  `gpt-5.3-codex-spark` model on this account. The operator explicitly approved
  a standard worker for in-memory UI/bridge/lifecycle tests, then separately
  authorized the prepared native editor/Vulkan checks and fixes/reruns.
- Integrated build: `cmake --build --preset debug -j4`, exit 0,
  `build-integrated.log`. Initial broad deterministic run:
  `ctest --preset debug --output-on-failure`, 615 passed, two symlink privilege
  skips, one failure among 618 tests, exit 8 (`ctest-integrated-agent.log`).
  The failure was the source-boundary JSON allowlist, corrected with a narrow
  private automation-protocol exception. Its targeted rerun passed, exit 0.
  Final integrated `cmake --build --preset debug -j4` passed
  (`build-final-integrated.log`). The final
  `ctest --preset debug --output-on-failure` selected 627 tests: **625 passed,
  two privilege skips, zero failures**, exit 0, 261.26 seconds. All 45 automation
  tests and the source boundary check passed. Logs:
  `ctest-final-integrated.log`, `ctest-final-integrated-details.log`.
- Focused in-memory automation run: `ctest --test-dir build/debug -L
  editor-automation --output-on-failure`, 43/43 passed, exit 0, 159.84 seconds
  (`ctest-window-refs.log`). Two later concrete projection/truncation tests
  passed in the focused 5/5 ApplicationSnapshot run
  (`ctest-snapshot-coverage.log`, exit 0, 3.90 seconds). This includes immutable projections, schema/codec,
  engine lifetime, real UI drafts and commits, menu/modal/disabled controls,
  vector and color components, combo/checkbox input, Cyrillic multiline input,
  rejected identifiers, ambiguity diagnostics and all fixture object kinds.
  The actual combo test exposed the pinned backend's missing open-status flag;
  the adapter now uses the copied combo state and real input. Its correction
  passed both a targeted rerun and this full suite.
- CTest routing was corrected to use the `editor-automation` label and a
  120-second per-test harness timeout. Real multi-operation widget scenarios
  exceeded 30 seconds; protocol operation/batch deadlines remain unchanged.
- Fresh option-OFF configuration: `build/semantic-imgui-option-off`, Clang
  Debug, Ninja, `NEAR_LAUGH_UI_AUTOMATION=OFF`, `BUILD_TESTING=OFF`. Configure,
  ordinary `level_editor` build (118 steps), and dependency audit each exited 0.
  All 268 compile commands, including 26 editor commands, are uninstrumented;
  no Test Engine fetch directory/cache/graph/link entry, automation executable
  or optional license artifact exists. Exact commands/evidence:
  `off-commands.log`, `off-configure.log`, `off-build.log`, `off-audit.log`.
  Ordinary dependency sources were reused read-only with separate binary dirs.

All log paths in this record are under `build/semantic-imgui-implementation/`
unless otherwise stated. Earlier failing intermediate runs remain as debugging
evidence; later passing runs supersede only the corrected checks they cover.

## File confinement review and approved save exception

Independent review found a destination ownership check/use race between a
write permit and ordinary atomic replacement. Disposable Windows sharing
probes confirmed that a deny-delete destination pin blocks replacement,
while permitting deletion also permits a competing replacement. The operator
explicitly approved automation-only exclusive, identity-checked output writes:
foreign destinations must remain untouched, while an I/O failure may leave
partial bytes in the owned temporary output. Ordinary editor saves remain
atomic. Design and delta requirements now record this exception.

The root-reparse fix pins a guard file during a temporarily deny-write-protected
setup. Final focused policy/document/codec checks: 43 passed, two symlink tests
skipped because this environment lacks link-creation privileges, exit 0.
Evidence: `build-policy-exclusive-final.log`, `policy-exclusive-tests.log`.
The separate capability probe confirmed symlinks remain unavailable even with
`CreateSymbolicLinkW` flag `0x2`: all four attempts returned error 1314, and the
current token lacks `SeCreateSymbolicLinkPrivilege` (not merely disabled).
No privileges/global settings were changed (`symlink-capability.log`). Those
two skipped checks remain an environmental validation gap:
`RestrictedEditorFiles.SymlinksInTheOwnedRootAreRejected` and
`EditorFilePolicyCodecDeathTest.DanglingSiblingTemporaryLinkIsNeverFollowed`.
Rerun them in a Windows environment with symlink creation available:

```powershell
ctest --test-dir build/debug -R 'SymlinksInTheOwnedRootAreRejected|DanglingSiblingTemporaryLinkIsNeverFollowed' --output-on-failure
```

A skipped result is not acceptance of task 3.2.

Tests cover actual junction substitution, foreign file insertion/replacement,
exclusive output identity, partial I/O failure and retry, denied pending
operations, and ordinary atomic saves. Ownership is recorded from the same
exclusive handle before writing; a failed output remains owned but unsaved.

The policy rejects pre-existing hard links. Windows still permits a same-user
process to create a new outside hard-link alias to an already-owned temporary
file after the final check. This is a file-confinement policy, not an OS security
boundary against an adversarial same-user process. Existing foreign files
cannot be substituted or overwritten by the approved save path.

## Native acceptance

The operator authorized a standard worker to open owned test windows, edit
copied fixtures through ImGui, resize/minimize/restore them, and test shutdown,
including fixes and reruns. Prepared external drivers use ordinary runtime
JSON tool calls and Win32 lifecycle calls only; no screenshot analysis or OS
keyboard/mouse injection is involved. The unfiltered ordinary Vulkan run failed 12/14 because the installed Overwolf
implicit layers could not resolve Vulkan loader entry points. A process-local
`VK_LOADER_LAYERS_DISABLE=*OW_OVERLAY*,*OW_OBS_HOOK*` filter was used for subsequent
runs and restored afterward. Khronos validation remained enabled. Layer-name
filters are documented by [LunarG](https://vulkan.lunarg.com/doc/view/1.4.335.0/windows/LoaderDebugging.html).
The filtered ordinary Vulkan suite passed 14/14, exit 0, 101.83 seconds;
92 context creations had validation enabled, zero unexpected error messages,
one deliberate injected validation error and 184 expected filter warnings.
Logs: `ctest-vulkan-filtered.log`, `ctest-vulkan-filtered-details.log`.
The final integrated `ctest --preset vulkan-smoke --output-on-failure` passed
**15/15**, exit 0, 110.56 seconds, including the physical-input isolation probe.
It created 93 Khronos-validation contexts, with zero unexpected errors,
one deliberate injected smoke-test error, and 186 expected overlay-disable
warnings. Logs: `ctest-vulkan-final.log`, `ctest-vulkan-final-details.log`.
The process-local filter was restored and no owned editor/host children remain.

The native callback isolation probe passed, exit 0, 3.80 seconds, and checked
zero Vulkan errors after actual application destruction. It verifies callback
input cannot reach ImGui, camera or document during idle/running batches,
session clipboard isolation, and owned-window close handling. Logs:
`ctest-native-input.log`, `ctest-native-input-details.log`. It does not certify
physical WM-message/gamepad hardware or visual appearance.

Actual MCP acceptance passed, including draft preservation, commit, Undo/Redo,
temporary Save As/reopen, generation invalidation, excluded/denied operations,
and a failing assertion that prevents its real Save suffix while preserving
prior edits. Output bytes and the source fixture hash were unchanged by the
failed batch. `real-editor-acceptance-01.*` and the later `real-editor-acceptance-02.*`:
exit 0, child exit 0, no forced kill, released cleanup, and zero final Vulkan
errors. The second run includes the absolute window-reference correction.

`real-editor-failures-01.*` passed missing/ambiguous/disabled/modal targets,
denied outside/traversal/ADS/Open/Play paths, timeout, cancellation, retained
identical retry and conflicting request-ID cases. Normal child exit and final
Vulkan count were both zero. `real-editor-lifecycle-03.*` passed resize,
minimize/restore and cancellation after a real edit was proven applied.
Minimized cleanup was explicitly unverified with unknown effects/stale state;
subsequent close reported the faulted session separately. The child ultimately
exited 0 with zero final Vulkan errors. Earlier lifecycle failures were harness
ordering assumptions and remain in their transcripts.

The native widget scenario passed section, scalar, combo, SliderFloat seek and
readable checks before exposing a real window-scroll reference bug. The engine
interpreted a repeated window name relative to its already-set reference. The
correction uses absolute numeric window identities for scope/focus/scroll;
its new regression and full 43-test suite passed. The native rerun
`real-editor-widgets-02.*` passed all these checks and page/item scrolling,
with normal child exit 0, released cleanup and final Vulkan count 0.

Independent review identified repeated cancellation extending cleanup, editor
File -> Exit bypassing cleanup, and host close acknowledging before child
teardown/validation exit. All three were corrected. Repeated/queued cancel and
expired-ID regressions passed. Cursor retry/pinning is intentional; an initially
incorrect expiration test was corrected to exceed the actual retention bounds,
and its rerun passed (`ctest-cursor-native-ready.log`, exit 0, 12.64 seconds).

`real-editor-construction-03.*` verified injected backend-construction failure:
expected child exit 2, no forced termination, final Vulkan count 0. Its cleanup
is conservatively `process_terminated` because the process returned nonzero.
`real-editor-file-exit-02.*` verified normal UI File -> Exit after frame samples,
with child exit 0, no force, released cleanup and final Vulkan count 0.

The first controller EOF run (`real-editor-disconnect-01.*`) stopped the owned
child within its bound but used forced termination and has no final Vulkan
marker. The host-death run (`real-editor-host-death-01.*`) verified Job Object
cleanup; killing the host removes diagnostic forwarding, so it does not prove
normal destructor order or final Vulkan count. The corrected `real-editor-disconnect-02.*` uses the shared bounded close path
and passed with natural child exit 0, no force, released cleanup and final
Vulkan count 0. Host-death remains a bounded process cleanup check by design.
The final `real-editor-construction-04.*` and `real-editor-file-exit-03.*`
reruns also passed on the added-control build, with expected natural exit 2/0,
no force and final Vulkan count 0.

Implementation progress is recorded in `tasks.md`. Unverified mandatory gates
remain unchecked. Visual acceptance: `not_run`.

## Workspace coverage inventory

The actual five editor UI files were inventoried after instrumentation. These
are call-site counts, not a claim that every runtime row is submitted at once.
The generic adapters retain normal UI handlers; metadata carries copied facts
and field bindings without mutation callbacks.

| Actual family | Sites | Evidence/status |
| --- | ---: | --- |
| Button / MenuItem | 42 / 8 | Real menu, disabled/modal and native workflow passed |
| Checkbox / Selectable | 12 / 28 | Actual selection, duplicate-owner disambiguation and checkbox input passed |
| BeginCombo / builtin Combo | 19 / 5 | Builtin real-input regression and native preview combo passed |
| InputText / multiline | 10 / 1 | Cyrillic, multiline, capacities, rejected ID, draft preservation and real commits passed |
| InputFloat / DragFloat / DragFloat3 | 4 / 6 / 5 | Ambient/vector input and native scalar property passed |
| SliderFloat | 1 | Actual character-preview seek passed |
| ColorEdit3(NoPicker) | 1 | Real component input and applied 8-bit rounding passed |
| CollapsingHeader | 3 | Actual expand/collapse passed |
| Window / modal / child | 9 / 4 / 1 | Scope topology/modal blocking and native semantic scrolling passed |
| Read-only text/diagnostics | 89 | Copied metadata and readable last-good preview regression passed |

Closed/clipped content is reported as incomplete coverage. Internal color
options remain `unregistered`; no right-click/picker action is advertised.
Viewport picking, placement, sculpting, navigation, gizmos, docking and OS
dialogs are excluded. Play is denied by both document policy and process-launch
boundary. Visual appearance and animated pixels are `not_run`.

## Representative size and timing

Debug hardware runs include startup, observation and actual actions; these are
client round trips, not an isolated rendering benchmark. The retained JSON
summaries contain every timing. P95 below uses the lower ranked sample.

| Scenario | Responses | Largest response | Median / P95 / maximum |
| --- | ---: | ---: | --- |
| acceptance-01 | 52 | 710,924 bytes | 515.5 / 2,031 / 2,250 ms |
| widgets-02 | 48 | 190,936 bytes | 407 / 875 / 1,594 ms |
| file-exit-02 | 18 | 208,240 bytes | 172 / 1,109 / 1,219 ms |

All were below the 1 MiB transport limit. Twelve fresh observations in
`real-editor-file-exit-02.summary.json` spanned 22 completed frames over 1,828 ms,
mean 83.09 ms per completed frame. This uses completed-frame counters and client
receipt times; it is not GPU timing or visual smoothness evidence. No speculative
performance changes were made. The final File Exit rerun sampled 22 frames
over 1,797 ms, mean 81.68 ms per completed frame. Visual checks remain `not_run`.

## Actual new-control extension evidence

After generic runtime/host fixes settled, the extension added exactly three
lines to the actual Document Summary: adjacent `zero-ambient` metadata and a
normal Button calling the existing `EditorDocument::setAmbient(0.F)`. It uses
the already permitted read-only document binding `ambient_intensity` and the
ordinary history/Undo path. No test-only workspace or mutation callback was
introduced.

`extension-control.diff` retains the narrow before/after diff;
`extension-before.json` and `extension-proof.json` retain SHA-256 hashes for all
23 generic automation, MCP/client/protocol, metadata-helper and optional-build
source files. They were identical after the control addition. The external
`tests/automation/real_editor_extension.py` supplies a new runtime batch to
discover/activate/assert and Undo the control.

`cmake --build --preset debug --target level_editor level_editor_automation -j4`
passed (`extension-build.log`). `real-editor-extension-01.*` passed discovery,
activation, typed assertions and Undo through actual MCP/editor calls, with
normal close, child exit 0, no force and final Vulkan count 0. The largest
response was 204,065 bytes; 11 round trips had median 500 ms and maximum
1,437 ms. All 23 source hashes were rechecked after native
execution and remain unchanged. The generated build fingerprint recompiles its
contract as expected; no generic source/handler/schema changed.


Exact final native commands below each exited 0 with the process-local layer
filter described above. The environment name records the authorization given
in this session. For reruns, use new transcript paths; existing evidence is
intentionally never overwritten.

```powershell
python -B tests/automation/real_editor_extension.py --environment user-authorized-windows-desktop --transcript build/semantic-imgui-implementation/real-editor-extension-01.jsonl
python -B tests/automation/real_editor_shutdown.py --environment user-authorized-windows-desktop --mode disconnect --transcript build/semantic-imgui-implementation/real-editor-disconnect-02.jsonl
python -B tests/automation/real_editor_shutdown.py --environment user-authorized-windows-desktop --mode construction --transcript build/semantic-imgui-implementation/real-editor-construction-04.jsonl
python -B tests/automation/real_editor_shutdown.py --environment user-authorized-windows-desktop --mode file-exit --transcript build/semantic-imgui-implementation/real-editor-file-exit-03.jsonl
```

## Final review and remaining gate

Reviewed the actual integrated changes and independent lifetime, input,
persistence and process-ownership findings. Corrections include absolute window
references, repeated-cancel deadlines, real File -> Exit cleanup, final-exit
verification, startup publication ownership and simultaneous EOF cleanup.
Affected deterministic/native checks were rerun after corrections. The final
extension source hashes remain unchanged. No unrelated format/spec migration,
new dependency version, screenshot claim or archive is included.

All implementation and available acceptance checks are finished. Task 3.2
remains unchecked solely for the two actual symlink checks blocked by Windows
privileges. Visual acceptance is `not_run`; it is separate from the semantic
acceptance gate. Normal controller EOF verifies destructor/Vulkan teardown;
forced host death verifies bounded Job Object cleanup and does not establish
final Vulkan diagnostics. The documented same-user hard-link limitation remains.

Final `git diff --check` and `openspec validate add-semantic-imgui-automation --strict` both exited 0 (`diff-check-final.log`, `openspec-final.log`). Previous-stage checklist: **35/36 verified**, with task 3.2 blocked as described above.

## Core-stage continuation, 2026-09-17–18

The continuation audited the existing working tree rather than interpreting its
checkboxes as evidence. The baseline automation executable passed 45/45 tests.
The audit found missing passive Test Engine facts, no real numeric drag gesture,
implicit engine visibility/focus recovery, loss of a known numeric draft when
the text was incomplete, and unsafe nested-row reuse for combined reorder/edit.
These are corrected by the implementation and design section 6.1; this stage
does not archive the change or alter the remaining symlink gate.

The user explicitly authorized a dedicated native test editor for this run.
The required `ui_test_runner` model could not start (platform rejected its
assigned model for this account); the user then explicitly authorized a worker
agent to execute the prepared tests. No screenshots or OS input are used.

Evidence directory: `build/semantic-ui-core-stage/`. Logs retain complete output
and real exit statuses. Early failed builds/runs are retained, not presented as
successful validation: `build.log`/`build-02.log` record corrected compile diagnostics, build-05 fixed
an obsolete API in new test setup; core-full-01 found menu-close, multiline-child
targeting and color-default test setup defects; core-corrections-01 isolated a
new test's temporary-JSON lifetime bug. Subsequent integrated results below are
the acceptance evidence.

Independent source review confirmed the pinned `ItemReadAsScalar` and
`ItemReadAsString` activate the field, Ctrl+A/C and Enter. Observation uses
read-only input buffers plus `ImGuiTestEngine_FindItemInfo` hook records instead,
with separate checks for current geometry/item flags and status timestamps.
The review also caught window key/scroll focus bypasses, helper evidence lost
on cancellation, implicit uncollapse in `SetRef`, missing compound key chords,
and exception cleanup. Each was corrected before final verification.

Functional coverage remains the actual editor's standard widget families and
the readable/diagnostic projections. Horizontal drag uses real Test Engine
button/movement/release for scalar/vector/RGB/slider components. Custom viewport
picking/placement/sculpting/navigation, gizmos, docking, OS dialogs and game
launch remain explicitly unsupported; HSV/hex/hidden color inputs publish an
unsupported reason instead of invented RGB components. Visual acceptance is
`not_run` and is separate from the functional/Vulkan results.

Final deterministic results for this continuation:

| Check | Result | Evidence under `build/semantic-ui-core-stage/` |
| --- | --- | --- |
| Debug configure, automation ON | exit 0 | `configure.log` |
| Full debug build, including ordinary and automation editor | exit 0 | `build-final.log` |
| `ctest --test-dir build/debug -L editor-automation --output-on-failure` | 61/61 passed, 270.31 s | `core-full-02.log` |
| Final affected assertion/timeout/vector validation tests | 3/3 passed, 60.21 s | `final-affected.log` |
| Python tool suite | 79/79 passed, 20.72 s | `tools-python-01.log` |
| Protocol suite after the final schema correction | 28/28 passed, 0.203 s | `final-protocol.log` |
| Ordinary CTest excluding automation and Vulkan labels, sequential | 580 passed, 2 skipped, 98.35 s | `ordinary-02.log` |

The two skips are the actual symlink cases already tracked by task 3.2;
Windows did not grant symlink creation. An initial parallel ordinary run
exposed shared temporary-file interference between existing StaticModelLoader
tests; the sequential run passed. No production change was made for that
unrelated test isolation issue. The last schema change adds the finite
`input_validation.status` assertion field. Error snapshots preserve the frame
of predicate evaluation, while `after` independently reports final cleanup.

The first native acceptance run correctly rejected an old scenario's attempt
to edit Ambient while its File menu remained open. Its prepared driver now
explicitly closes File before editing and places both menu opening and Save in
the `not_run` suffix. The runtime guard was retained. The failed transcript
(`native-acceptance-01.jsonl`) and exact pair are kept; teardown reported
child exit 0, released input and zero Vulkan validation errors.

The corrected native acceptance and widget scenarios passed:

- `native-acceptance-02.jsonl`: bounded immutable pagination; passive draft vs
  applied value; separate commit; Undo/Redo; Save As and reopen; generation
  invalidation; failed dirty assertion with actual `true`, and both following
  steps `not_run`. Maximum response was 748,021 bytes; 55 tool round trips had
  median 562 ms and maximum 2,359 ms on this run (includes scheduling/IPC).
- `native-widgets-01.jsonl`: actual section, selection, scalar edit and
  drag/release, combo, preview slider input and drag, scrolling and stable refs.
  Both editors closed normally (child exit 0, no forced termination), with
  released input and zero final Vulkan validation errors in their stderr logs.

Actual runtime drag evidence is retained as
`native-widgets-01-drag-7-pair.jsonl`, with the unmodified tool payloads also
extracted to `actual-drag-request.json` and `actual-drag-response.json`.
The request contains a real `drag` and an independent application assertion;
it supplies direction, fraction 0.2 and eight frames, without supplying a
result value. Applied object speed was 0.75 before the gesture. The response
reports both steps passed, observed speed `1.2400000095367432`, document
revision 3 to 4, no auxiliary actions, and `cleanup: released`. The slider
pair is separately retained as `native-widgets-01-drag-15-pair.jsonl`.

- `native-failures-01.jsonl` passed the prepared missing/ambiguous/stale,
  disabled/modal/unsupported, assertion, path-policy, timeout, cancellation,
  identical-retry and conflicting-request cases. Normal close released input,
  child exit was 0, and final Vulkan error count was 0.
- `native-lifecycle-02.jsonl` passed resize, minimize/restore and the intended
  cancellation while minimized. This last case returned `ui_unavailable`,
  `cleanup: unverified`, and `effects: unknown`; subsequent close reported
  `engine_error` with `cleanup: process_terminated`. It does **not** establish
  verified input release in the minimized state: close acknowledgement was
  missing. The independent host audit nevertheless recorded actual child
  exit 0 and `forced: false`; no forced kill occurred. Final Vulkan teardown
  error count was 0. The first lifecycle run is retained: its direct runtime packet
  omitted the now-required explicit scrolling policy after resize; only the
  prepared driver was corrected.
- `ctest --test-dir build/debug -R
  '^(editor_vulkan_smoke|editor_vulkan_construction_failure|editor_automation_input_isolation)$'
  --output-on-failure` passed 3/3, exit 0, 8.96 s (`vulkan-editor-01.log`).
  Native runs used the process-local
  `VK_LOADER_LAYERS_DISABLE=*OW_OVERLAY*,*OW_OBS_HOOK*` filter for the installed
  overlay hooks; Khronos validation remained enabled. The full output is in
  `vulkan-editor-01-full.log`: input isolation explicitly reports zero final
  errors; smoke/construction report no validation errors and only the expected
  overlay-filter warnings, without a numeric error counter.

Task 3.2 remains the only unchecked task because the two real symlink tests
remain unavailable. Standard widget behavior, custom readable/diagnostic
adapters and explicit unsupported custom gestures are recorded above; visual
acceptance remains `not_run`. The change is left open, not archived.

Final strict OpenSpec validation and `git diff --check` exited 0
(`openspec-final.log`, `diff-check-final.log`). Checklist: **42/43 verified**.

## Observation truthfulness review, 2026-09-18

Continued from the existing implementation and retained its pre-existing work.
Independent pinned-source review reconfirmed that `ItemReadAsScalar/String`
activate, select/copy and confirm the input; the passive implementation uses
current hook facts and read-only buffers instead. The review identified two
remaining observation errors: silent truncation of inactive text/labels and
deferred commit metadata on immediate preview/placement values.

The corrections add explicit label/input truncation, preserve actual
unavailable-value evidence in predicate failures, and add one-shot value-only
immediate-policy annotations. Actual editor placement-control tests accompany
the shared scalar/compound metadata tests. The ordinary handlers and the
generic data-only operation set are unchanged. Design section 6.1 documents
the contract correction and its reason; file-dialog buffer completion remains
distinct from the subsequent explicit Open/Save action.

The configured `ui_test_runner` could not start because its pinned model was
unavailable. The user explicitly authorized a worker for in-memory tests and
requested removal of that model requirement. `AGENTS.md` now allows that narrow
fallback without another confirmation, and `.codex/agents/ui_test_runner.toml`
omits both model overrides. The file parses as TOML. Omitted model settings use
the documented [Codex subagent resolution rules](https://learn.chatgpt.com/docs/agent-configuration/subagents).
No desktop authorization was inferred from this permission.

Evidence directory: `build/semantic-ui-review-20260918/`. Configure with
`cmake --preset debug -DNEAR_LAUGH_UI_AUTOMATION=ON` exited 0 (`configure.log`).
Both `cmake --build --preset debug --target level_editor level_editor_automation -j 4`
and `cmake --build --preset debug --target editor_automation_tests engine_tests -j 4`
exited 0 (`build-editors.log`, `build-tests.log`).
`python -B -m unittest discover -s tests/tools -p 'test_editor_ui_*.py'`
passed 80 tests, exit 0, 20.893 s (`python.log`).
The worker's `ctest --test-dir build/debug -L editor-automation --output-on-failure`
passed **67/67**, exit 0, 294.09 s (`core.log`). This includes six new tests for
truncation, one-shot/compound metadata and actual light/door placement controls,
alongside passive input/focus/history, multiple batches, ambiguity/stale refs,
disabled/modal guards, invalid values, timeout, cancellation and drag release.
The worker's `ctest --test-dir build/debug -R 'EditorUi|boundary|automation_dependency' --output-on-failure`
passed **35/35**, exit 0, 32.05 s (`ordinary-affected.log`): 20 unit/dependency
checks, four boundary checks and 11 semantic UI checks overlapping the first
run. These two CTest selections cover **91 unique passed tests**. No additional
repeat was needed. Strict OpenSpec validation and `git diff --check` also
exited 0 (`openspec.log`, `diff-check.log`). Checklist: **45/46 verified**;
the remaining unchecked item is the previously blocked task 3.2. No archive.

The newly retained `actual-request.json` and `actual-response.json` are the
unmodified tool payloads from the in-memory native pipe/dispatcher regression,
also stored as GoogleTest XML properties under `gtest/`. The packet enters
0.123 through a real widget whose test handler deliberately ignores application,
then asserts the actual document and supplies a third edit. The response has
`ok=false`, `failed_index=1`, `assertion_failed`, actual document value
`0.11999999731779099`, statuses `passed/failed/not_run`, `effects=partial`, and
`cleanup=released`. This expected protocol failure makes the regression pass;
it does not echo the requested value as document evidence.

This run opens no desktop editor. Native MCP/Vulkan and visual checks were not
rerun; previous native evidence remains historical evidence above. The current
corrections change semantic metadata/predicates, not rendering, resources,
window ownership or lifetime. Task 3.2's two real symlink checks remain blocked
by the previously recorded Windows privilege constraint. Viewport/gizmo,
docking, OS-dialog and other explicit v1 exclusions remain unsupported.


## Official SDK MCP continuation, 2026-09-18

The user explicitly replaced the handwritten MCP decision with the official
SDK. The host and runtime JSON client now use `mcp==2.2.0`; all 29 resolved
packages are pinned in `scripts/requirements-editor-ui.txt` for Windows x64 /
Python 3.12. The SDK owns initialize/discovery, method dispatch and cancellation;
the bounded byte adapter retains the existing 1 MiB / depth 32 / partial-line
limits. The existing inherited pipes, fixed executable/fixtures, build/session
handshake and editor-side semantics remain the only execution path.

Tool descriptions now explain batch execution, passive observation limits,
drafts versus applied state, partial/unknown results and explicit recovery.
Results contain optional local `diagnostic_path`; stderr is retained up to
16 MiB per child, and oversized tool evidence uses up to 32 one-MiB result
artifacts. There is no automatic mutation retry or exactly-once claim.

Independent transport review found and implementation corrected:

- Priority cancellation could precede its batch and be lost. The C++ channel
  retains at most 16 distinct cancellation IDs; duplicate IDs share capacity,
  completion clears them, and overflow closes the channel.
- SDK startup cancellation must close the acquired child, including readiness
  racing a suppressed response. Queued/running work receives cancellation
  tokens before EOF teardown so it cannot acquire late ownership.
- EOF cleanup and request cancellation use one absolute cleanup deadline,
  rather than stacking independent two-second waits.
- Failed diagnostic eviction remains accounted for and refuses new artifacts
  instead of silently exceeding its file limit.

Behavioral regressions cover these cases. Final independent review confirmed
both remaining race/retention corrections with no unresolved finding in its
assigned lifetime/bounds scope. No agent model/global Codex settings changed.

### Reproduction and results

All commands below run from the repository root. The fresh venv was created
with `python -m venv build/editor-ui-venv`, the official SDK installed there,
and the resolved closure recorded as exact pins. Reproduce installation with:

```powershell
& ./build/editor-ui-venv/Scripts/python.exe -m pip install --only-binary=:all: -r scripts/requirements-editor-ui.txt
& ./build/editor-ui-venv/Scripts/python.exe -m pip check
cmake --preset debug -DNEAR_LAUGH_UI_AUTOMATION=ON
cmake --build --preset debug --target level_editor level_editor_automation editor_automation_tests engine_tests -j 4
& ./build/editor-ui-venv/Scripts/python.exe -B -m unittest discover -s tests/tools -p "test_editor_ui_*.py"
ctest --test-dir build/debug -L editor-automation --output-on-failure
ctest --test-dir build/debug -R 'EditorUi|boundary|automation_dependency' --output-on-failure
& ./build/editor-ui-venv/Scripts/python.exe -B tests/automation/real_editor_sdk.py --environment authorized-test-desktop --disable-implicit-layers --transcript build/editor-ui-sdk-integration-clean.jsonl
```

Use a fresh transcript path when repeating the last command. The user's
explicit request for real-editor integration authorized only the dedicated
fixture sessions for this run. `ui_test_runner` executed the native/real editor
checks; no OS mouse/keyboard injection, screenshots or document-file edits
were used. Source fixture hashes remained unchanged.

| Check | Result | Evidence under `build/` |
| --- | --- | --- |
| SDK install / lock installation / `pip check` | exit 0; no broken requirements | `editor-ui-sdk-install.log`, `editor-ui-sdk-lock-install.log`, `editor-ui-sdk-pip-check.log` |
| Configure / four affected targets | exit 0 / exit 0 | `editor-ui-sdk-configure.log`, `editor-ui-sdk-build.log` |
| Python protocol/process/host/SDK tests | 87 passed, exit 0, 37.009 s | `editor-ui-sdk-python-final.log` |
| Native automation CTest | 68/68, exit 0, 312.05 s | `editor-ui-sdk-native.log` |
| Ordinary UI/boundary CTest | 35/35, exit 0, 33.11 s; 11 overlap native selection | `editor-ui-sdk-ordinary.log` |
| SDK -> production host -> real editor | all normal assertions passed; exit 0 | `editor-ui-sdk-integration-clean.jsonl`, `.summary.json` |
| Injected private protocol incompatibility | `version_mismatch`, passed | same transcript, protocol scenario |
| Injected owned-editor death | `disconnected`, unknown steps/effects, terminated cleanup; passed | same transcript, crash scenario |
| Normal Vulkan teardown | validation enabled, 0 errors | `editor-ui-sdk-integration-clean.normal.stderr.log` |

The first integration attempt is retained as a **failed** run in
`editor-ui-sdk-integration.*`. Semantic assertions passed, but normal close
correctly reported `engine_error`, child exit 3 and two Vulkan loader errors
from installed Overwolf implicit layers (`vkGetInstanceProcAddr` missing).
The repeat used `VK_LOADER_LAYERS_DISABLE=~implicit~` only in the SDK-launched
host/child environment. Explicit Khronos validation remained enabled and was
asserted by the driver; no global environment, registry or layer installation
was changed. This filtering follows the
[Khronos loader contract](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md#layer-filtering).

Both initialization-era (`2025-11-25`) and modern SDK discovery clients were
tested over actual subprocess stdio. `tools/list` returns the same four exact
input/output schemas; JSON Schema 2020-12 validators accept each schema.
`tools/call` tests strict types, extra fields, unsupported operations/projections,
excess batches, denied environment startup and continued use after invalid
requests. Structured and text results agree. Existing raw-byte transport tests
cover invalid/duplicate JSON keys, Unicode fragmentation, oversized/deep input,
partial-line expiry and slow/broken pipes; those are transport regressions,
not a replacement MCP implementation.

The successful real-editor sequence independently verified document ambient
`0.11999999731779099 -> 0 -> 0.08100000023841858`, dirty state, identical result
retrieval without replay, invalid request `-32602`, timeout, explicit session
cancel and official SDK cancellation notification. A failed assertion between
an edit and a subsequent activation retained the edit and stopped the suffix.
The actual tool payloads are extracted without alteration into
`editor-ui-sdk-actual-request.json` and `editor-ui-sdk-actual-response.json`.
Their compact result is:

```json
{"ok":false,"last_completed_index":0,"failed_index":1,"effects":"partial","cleanup":"released"}
```

The three step statuses are `passed`, `failed`, `not_run`; the observed value is
`0.08100000023841858`, while the failed assertion expected `0.099`.
The protocol fault wrapper changes only the real child's hello version before
host validation. The crash wrapper terminates only its Popen-owned real editor
while a batch is pending. Neither is a fake editor or a production fault switch.
Crash results contain `unknown/unknown` and `effects=unknown`, never a fabricated
final state; subsequent close preserves the abnormal exit as `engine_error`.

Codex setup is in `.codex/editor-ui-mcp.example.toml` and `docs/DEVELOPMENT.md`.
The template parses as TOML and uses only placeholders. `command`, `args`,
`cwd`, `env`, `startup_timeout_sec`, `tool_timeout_sec`, `enabled_tools` and
trusted-project `.codex/config.toml` support were checked against current
[official Codex MCP documentation](https://learn.chatgpt.com/docs/extend/mcp?surface=cli).
No live project/global config was written. Visual acceptance is **not_run**;
normal-editor resize/minimize presentation checks were not repeated because
this continuation changes MCP/lifecycle cancellation, not renderer behavior.
The existing task 3.2 real symlink privilege checks remain **blocked** and
unchecked. They are not waived by the SDK acceptance. No archive.

Final `openspec validate add-semantic-imgui-automation --strict` exited 0
(`editor-ui-sdk-openspec.log`). Tracked `git diff --check` exited 0; whitespace
checks also passed for 21 added files (`editor-ui-sdk-diff-check-final.log`;
`--no-index` exit 1 only means those files differ from empty). The actual
integrated diff and bounded session/channel changes were reviewed. Section 12
is verified; overall checklist **49/50**, with only the pre-existing task 3.2
symlink privilege gate still blocked.
