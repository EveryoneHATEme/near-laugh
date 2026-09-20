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

## Everyday agent workflow continuation, 2026-09-20

Baseline: clean actual checkout `c177e42` (`add semantic imgui automation`).
The change is still active. The existing transport, C++ executor, identity
system, dependency pins, file policy, input isolation and child ownership remain
intact. No new product widget, feature-specific MCP handler, level JSON mutation,
Windows input, renderer behavior or screenshot infrastructure was introduced.

Implemented section 13: semantic-first AGENTS/runner routing for both existing
regressions and newly prepared runtime scenarios; short `editor-ui-testing`
skill; official SDK `Server(instructions=...)`; no-window doctor and opt-in live
smoke; reusable bounded SDK session/evidence helper; strict reviewed fixture
manifest; a real deferred-title/persistence/failure regression. Fixture additions
need a manifest entry and packaged level, not host semantic changes. Windows
reserved filenames, path syntax, duplicate aliases and reparse components are
rejected. New UI readiness requires metadata, independent applied-state checks
and a reproducible scenario. All prior viewport/docking/OS/game exclusions remain
future coverage, not completed functionality.

Independent review found and corrected two client-evidence defects before native
acceptance: structured host `unknown` results must remain unknown in summaries,
and summary destinations must be reserved before starting/mutating the editor.
The reviewer confirmed both fixes; regression tests cover them and preserve
original errors during evidence cleanup. Reports/call counts have explicit
bounds; response loss never automatically replays mutations.

### Setup, build and A checks

The existing pinned Python 3.12 venv was reused; no dependency version changed.
`pip check` passed. Setup reproduction remains:

```powershell
python -m venv build/editor-ui-venv
& ./build/editor-ui-venv/Scripts/python.exe -m pip install --only-binary=:all: -r scripts/requirements-editor-ui.txt
cmake --preset debug -DNEAR_LAUGH_UI_AUTOMATION=ON
cmake --build --preset debug --target level_editor level_editor_automation editor_automation_tests engine_tests editor_automation_dependency_probe editor_automation_input_probe -j 4
& ./build/editor-ui-venv/Scripts/python.exe -B -m unittest discover -s tests/tools -p "test_editor_ui_*.py"
ctest --test-dir build/debug -L editor-automation --output-on-failure
ctest --test-dir build/debug -R 'EditorUi|boundary|automation_dependency' --output-on-failure
```

Configure/build exited 0 (`build/editor-ui-daily-configure.log`,
`editor-ui-daily-build.log`). Initial sandbox-denied Ninja execution was resolved
through command approval, not by changing agent/system settings. The later
environment permission change was supplied by the user. An incremental build
preceded the functional test group; MCP never builds per action. Fingerprint
agreement alone is not treated as source freshness.

| A check | Actual result | Evidence under `build/` |
| --- | --- | --- |
| Pinned environment | `pip check` exit 0 | `editor-ui-daily-pip-check.log` |
| Full Python protocol/process/host/SDK/doctor/helper/fixture suite | 119 passed, 39.944 s, exit 0 | `editor-ui-daily-python.log` |
| Final host regression including one newly added manifest-only fixture test | 44 passed, 26.007 s, exit 0 | `editor-ui-daily-host-final.log` |
| Final reserved-device filename/manifest checks | 3 passed, exit 0 | `editor-ui-daily-fixtures-final.log` |
| Native automation | 68/68 passed, 317.36 s, exit 0 | `editor-ui-daily-native.log` |
| Ordinary UI/boundary selection | 35/35 passed, 34.31 s, exit 0 (11 overlap automation UI) | `editor-ui-daily-ordinary.log` |
| Ordinary doctor | passed, 3.391 s, 2 SDK requests; no window | `editor-ui-daily-doctor.{json,jsonl,log}` |

The final focused checks cover the only source/test additions after the full
119-test run (120 distinct Python tests now exist). No whole suite was repeated
for documentation-only edits. The skill validator and portable TOML parse passed.
The ordinary doctor leaves desktop/GPU, Vulkan, source freshness, agent tool
visibility and appearance `not_checked`.

Option-OFF configure/build of `level_editor` and `near_laugh` also exited 0 in
the existing `build/semantic-imgui-option-off` tree (the initial fresh-OFF check
is retained above). Reproduction:

```powershell
cmake --preset debug -B build/semantic-imgui-option-off -DNEAR_LAUGH_UI_AUTOMATION=OFF -DBUILD_TESTING=OFF
cmake --build build/semantic-imgui-option-off --target level_editor near_laugh -j 2
```

Audit: 268 compilation entries, zero automation/Test Engine entries, zero
automation target/definition references, zero Test Engine fetch directories,
ordinary game/editor links without automation libraries. Evidence:
`editor-ui-daily-off-configure-root.log`, `editor-ui-daily-off-build.log`,
`editor-ui-daily-off-inspection.log`. The reviewer's earlier configure attempt
was blocked by its inherited sandbox (`editor-ui-daily-off-configure.log`);
the main agent's later successful run is the build evidence.

### B: production host and real editor

The user authorized the dedicated `authorized-test-desktop` profile. The
`ui_test_runner` executor controlled these prepared scenarios, with no Windows
mouse/keyboard injection, screenshots, user's editor or source-fixture writes.

```powershell
& ./build/editor-ui-venv/Scripts/python.exe -B scripts/editor_ui_doctor.py --report build/editor-ui-daily-doctor.json
& ./build/editor-ui-venv/Scripts/python.exe -B scripts/editor_ui_doctor.py --launch-editor --environment authorized-test-desktop --disable-implicit-layers --report build/editor-ui-daily-smoke-filtered.json
& ./build/editor-ui-venv/Scripts/python.exe -B tests/automation/real_editor_workflow.py --environment authorized-test-desktop --disable-implicit-layers --transcript build/editor-ui-daily-workflow.jsonl
& ./build/editor-ui-venv/Scripts/python.exe -B tests/automation/real_editor_sdk.py --environment authorized-test-desktop --disable-implicit-layers --transcript build/editor-ui-daily-sdk.jsonl
```

Use fresh evidence names for repeats. The first **unfiltered** live doctor failed
(exit 1, 6.047 s): startup reached ready but close returned `engine_error`, editor
exit 3 and two Vulkan loader errors from the installed Overwolf implicit layer
(missing `vkGetInstanceProcAddr`). Its failure is retained in
`editor-ui-daily-smoke.{json,jsonl,log}`. The corrected run filters implicit
layers only in the launched test host/child environment, retaining explicit
Khronos validation; no global environment/registry/layer install was changed.
Filtered smoke passed (exit 0, 5.187 s, 4 MCP requests), with a real ready frame
and zero Vulkan teardown errors. Evidence: `editor-ui-daily-smoke-filtered.*`.

The everyday workflow passed (exit 0). It used Objects selectable selection,
Properties' real deferred `Document title` InputText with Cyrillic text and
independent old/applied title assertions, dirty/history, Undo/Redo, Document
Summary's real ambient InputFloat, and ImGui Save As/Open on `output-1`. It
rediscovered all scopes/object refs after document replacement and verified the
persisted title and ambient value. Source fixture hash remained unchanged;
normal close reported `cleanup=released`, validation enabled and zero teardown
errors. Functional success does not establish appearance.

Measured whole workflow: **45 MCP requests** (initialize + tools/list + **43 tool
calls**), **35.766 s**, **1,049,341 bytes** total SDK-result JSON. Largest response
750,936 bytes is tools/list; largest tool result 21,699 bytes. Counts exclude
notifications; byte measurements exclude JSON-RPC envelopes and initialization
uses negotiated SDK facts. Full transcript: `editor-ui-daily-workflow.jsonl`
(1,073,177 bytes); compact report: `.summary.json`; host log: `.stderr.log`;
summary also points to the retained owned-child diagnostic file. No speculative
speed ratio or Windows-click comparison was measured.

Request 16 executes seven edit/draft/applied/history checks; request 19 executes
eleven commit/Undo/Redo checks within one editor batch, with no model round trips
between steps. Negative request 39 applies a title change, fails step 1's dirty
assertion (expected known false, observed known true), then reports
`passed/failed/not_run/not_run`, `effects=partial`, `cleanup=released`. The saved
slot hash proves the Save suffix did not execute. Fresh scoped observation and
independent title inspection precede corrected request 42 (six passing steps).
Earlier changes are retained honestly; no replay or rollback is inferred.

The existing `real_editor_sdk.py` regression also exited 0: normal UI assertions,
duplicate-result retention, invalid requests, timeout, explicit/SDK cancellation,
mid-batch failure, incompatible private protocol and owned-editor death all
passed. Crash evidence retains unknown effects/steps; normal teardown has zero
Vulkan errors. It recorded 40 tools/call across three connections, plus three
initialize and three tools/list requests; this older driver does not measure
scenario duration. Evidence: `editor-ui-daily-sdk.{jsonl,summary.json,log}` and
the three `.normal/.protocol/.crash.stderr.log` files.

`ctest --test-dir build/debug -R '^editor_automation_input_isolation$'
--output-on-failure` passed 1/1, exit 0, 3.95 s, with the same process-local
`VK_LOADER_LAYERS_DISABLE=~implicit~` filter. Final explicit Vulkan validation
errors: zero. Evidence: `editor-ui-daily-input-isolation.log` and
`editor-ui-daily-input-isolation-details.log`. The runner released GUI ownership;
no physical Windows input was injected.

### C, symlinks and remaining acceptance

**C is blocked.** Both main and actual runner catalogs contain none of the four
editor-ui MCP tools. SDK tools/list and B success do not establish an agent
connection. With explicit user permission, the local ignored `.codex/config.toml`
now contains only the new `mcp_servers.editor_ui` table, absolute local Python/
host/cwd paths, authorized profile, 15/75-second timeouts and four enabled tools.
Its child-local env filters the diagnosed implicit overlay. No global Codex
config, model or system permissions were changed by this implementation.
The portable template and SDK server-instructions support were checked against
the [official Codex MCP documentation](https://learn.chatgpt.com/docs/extend/mcp?surface=cli)
on 2026-09-20.

Restart Codex in this trusted repository, check `/mcp`, and delegate the minimal
C continuation in `docs/DEVELOPMENT.md`: fresh household fixture, discover/select
the `letter` readable, observe its title, one draft/commit/applied assertion batch,
independent `app_inspect`, then close through actual agent tools and retain the
exchanges. Running the Python driver again would still only establish B. Task
13.5 remains unchecked until this actual runner test succeeds.

The two **real** symlink tests were attempted again without mocks or privileges:

```powershell
& ./build/debug/tests/engine_tests.exe --gtest_filter=RestrictedEditorFiles.SymlinksInTheOwnedRootAreRejected:EditorFilePolicyCodecDeathTest.DanglingSiblingTemporaryLinkIsNeverFollowed
```

Exit 0 means the test process finished, **not acceptance**: zero passed and two
skipped, both reporting `A required privilege is not held by the client.` See
`editor-ui-daily-symlinks.log`. Task 3.2 remains blocked/unchecked. Rerun that
exact command in a Windows environment already allowed to create symlinks;
no Windows settings or agent-wide privileges were changed to bypass it.

Visual acceptance is **not_run**. Ordinary-editor full resize/minimize/game smoke
was not repeated because this continuation changes no C++ rendering/window/
lifetime code; dedicated host startup/teardown and affected isolation checks are
recorded separately. No archive while mandatory C/symlink gates remain open.

Integrated diff review, tracked/new-file whitespace checks, skill validation,
portable/local TOML parsing and strict OpenSpec validation passed. Final logs:
`editor-ui-daily-diff-check.log`, `editor-ui-daily-openspec-final.log`. Current
checklist: **54/56 verified**, with only 3.2 and 13.5 open for the reasons above.

## Restart continuation (2026-09-20)

The user resumed this change after restarting Codex. Both the main agent and
the delegated `ui_test_runner` now discover all four actual editor-ui MCP tools.
The existing authorization for the dedicated disposable test session is retained;
no local/global configuration or Windows settings were changed in this continuation.

Before the runner's functional scenario, the incremental build passed:

```powershell
cmake --build --preset debug --target level_editor level_editor_automation editor_automation_tests engine_tests -j 4
```

The first sandbox attempt failed because Ninja execution was denied
(`editor-ui-restart-build.log`). The same build through scoped command approval
exited 0 (`editor-ui-restart-build-approved.log`). No source changes required a
new configure or broader test rerun. The no-window doctor passed, exit 0,
3.14 seconds; evidence: `build/editor-ui-restart-doctor.{json,jsonl,log}`.

### Task 3.2: actual symlink checks remain blocked

A separate delegated no-window check reran the two required real symlink tests:

```powershell
& ./build/debug/tests/engine_tests.exe --gtest_filter=RestrictedEditorFiles.SymlinksInTheOwnedRootAreRejected:EditorFilePolicyCodecDeathTest.DanglingSiblingTemporaryLinkIsNeverFollowed
```

The sandbox attempt exited 1: one test skipped, and the other failed during
fixture setup with `policy_denied: cannot pin the owned path for this operation`
(`build/editor-ui-restart-symlinks-sandbox.log`). The same command outside the
sandbox exited 0 with **zero passed and two skipped**, both reporting
`Creating symlinks unavailable: A required privilege is not held by the client.`
See `build/editor-ui-restart-symlinks.log`. Neither test reached its behavioral
assertions; this does not satisfy task 3.2. It remains unchecked until the exact
tests run in a Windows environment permitted to create real symlinks. No mocks,
junction substitutions or privilege/settings changes were used.

### Task 13.5: actual runner MCP connection passed

The delegated `ui_test_runner` used its actual callable
`mcp__editor_ui__ui_session`, `ui_observe`, `ui_execute` and `app_inspect` tools;
this is level C evidence. Its catalog is retained at
`build/editor-ui-restart-c.catalog.json`. No SDK subprocess was substituted for
the agent tools, and the main agent did not replay the scenario.

The runner started a fresh `household-interactions` fixture in the previously
authorized profile, discovered current Objects/Properties scopes, paginated
object inspection and selected the `readable_document` with persisted ID
`letter`. Request 6 passed all three selection steps. Request 9 passed all seven
steps in one batch: enter `Agent MCP check` without commit, verify input and
typed draft, verify the old applied title, commit by the advertised Tab method,
verify the new applied title and dirty state. Request 10 independently inspected
the known applied title as `Agent MCP check`; document revision advanced from
1 to 2. Both batches reported `effects: partial` and `cleanup: released`, with
no failed, unknown or not-run steps.

Request 11 closed successfully (`state: closed`); the owned temporary directory
was removed. The returned close response does not expose the editor exit code,
so none is inferred. Retained diagnostics show Vulkan validation enabled and
**zero errors after teardown**. The source and packaged fixture SHA256 hashes
were unchanged. No source-document writes, screenshots or Windows input were
used. GUI ownership was released.

| Measurement | Observed result |
| --- | --- |
| Actual agent tool calls | 11: session 2, observe 3, inspect 4, execute 2 |
| Execution steps | 10 passed; 0 failed, unknown or not_run |
| Cumulative tool-call elapsed time | 47.116 s |
| Runner time from catalog capture through close | 191.032 s, including agent work between calls |
| Retained result JSON | 79,160 UTF-8 bytes total; largest response 21,606 bytes |
| Actual request/result transcript | 83,662 bytes, 11 records |

Result sizes include the actual tool result's text and structured representations,
excluding outer RPC envelopes. Initialization/tools-list counts are unavailable
from the runner tool transcript and are not invented. Evidence:
`build/editor-ui-restart-c.jsonl`, `.summary.json`, `.catalog.json` and
`.stderr.log`. The main agent reviewed the real request 9 step observations,
request 10 independent inspection and request 11 close result before checking
task 13.5.

Level A doctor and level C passed in this continuation. The separate level B
SDK regression and broad native/Vulkan lifecycle suites were not repeated;
their prior evidence remains above, and this continuation changed no application
code. Visual acceptance remains **not_run**. Current checklist: **55/56
verified**, with only task 3.2 blocked by unavailable real symlink creation.
No archive was performed.

Final `git diff --check` and
`openspec validate add-semantic-imgui-automation --strict` both exited 0.
Evidence: `build/editor-ui-restart-diff-check.log` and
`build/editor-ui-restart-openspec-final.log`. OpenSpec apply status confirms
55 complete / 1 remaining; its CLI state remains `ready`, with the task 3.2
environment blocker recorded explicitly rather than converted into acceptance.

## Administrator retry: task 3.2 accepted (2026-09-20)

The user restarted Codex from an administrator terminal and requested another
attempt at the remaining gate. A delegated worker incrementally built
`engine_tests` and ran the unchanged no-window filesystem tests. The main agent
reviewed the logs and GoogleTest XML: both actual symlink tests reached and
passed their assertions, with zero skips. No source, test, privilege or Windows
setting changes were required.

The sandbox still denied Ninja execution (build exit 1) and link creation/path
pinning (test exit 1: one skipped test and one fixture setup failure). Those
attempts remain recorded in `build/editor-ui-admin-retry-20260920-184444-build.log`
and `build/editor-ui-admin-retry-20260920-184605-symlinks.{log,xml}`. Sanctioned
execution outside the sandbox used the restarted administrator environment and
passed; sandbox failures are not counted as acceptance.

Exact successful commands, each with exit 0:

```powershell
cmake --build --preset debug --target engine_tests -j 4
.\build\debug\tests\engine_tests.exe '--gtest_filter=RestrictedEditorFiles.SymlinksInTheOwnedRootAreRejected:EditorFilePolicyCodecDeathTest.DanglingSiblingTemporaryLinkIsNeverFollowed' '--gtest_output=xml:build/editor-ui-admin-retry-20260920-184628-symlinks-escalated.xml'
.\build\debug\tests\engine_tests.exe '--gtest_filter=EditorFilePolicy.*:RestrictedEditorFiles.*-RestrictedEditorFiles.SymlinksInTheOwnedRootAreRejected' '--gtest_output=xml:build/editor-ui-admin-retry-20260920-184717-file-policy-escalated.xml'
```

| Check | Result | Evidence under `build/` |
| --- | --- | --- |
| Incremental affected target build | Current; Ninja reported no work | `editor-ui-admin-retry-20260920-184547-build-escalated.log` |
| Actual symlink rejection and dangling sibling temporary-link protection | 2 passed, 0 failed, 0 skipped; 57 ms | `editor-ui-admin-retry-20260920-184628-symlinks-escalated.{log,xml}` |
| Remaining file-policy regressions, including ordinary saves, owned outputs, unsafe paths, junctions and denied Play | 18 passed, 0 failed, 0 skipped; 368 ms | `editor-ui-admin-retry-20260920-184717-file-policy-escalated.{log,xml}` |

All 20 distinct tests completed. This closes task 3.2 and supersedes the prior
environment blocker; the checklist is now **56/56 verified**. Existing SDK/MCP
and Vulkan lifecycle evidence remains above. No GUI, MCP or Vulkan run was
repeated for this filesystem-only validation continuation; visual acceptance
remains **not_run**. The documented same-user hard-link limitation is unchanged.
No archive was performed.

Final diff review, `git diff --check` and
`openspec validate add-semantic-imgui-automation --strict` passed (both commands
exited 0). Logs: `build/editor-ui-admin-retry-diff-check.log` and
`build/editor-ui-admin-retry-openspec-final.log`. OpenSpec apply confirms
**56 complete / 0 remaining**, state `all_done`.
