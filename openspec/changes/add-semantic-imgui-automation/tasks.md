## 1. Reconfirm prerequisites and contracts

- [x] 1.1 Recheck the selected change against the current checkout, applicable instructions and affected main specs; record baseline/dependency versions and any conflicts in the change's validation record. Verify no unrelated household/spec reconciliation or dependency upgrade is included.
- [x] 1.2 Record the applicable Test Engine v1.04 license basis and redistribution notices, then pin `2628e39cc0ea3a0a612d5d039543c9d4e873c720` with unchanged ImGui `v1.92.9b-docking`. Reproduce compile/link and minimal lifecycle checks under the project Clang/MSVC ABI; retain exact commands and exit codes. The planning probe is evidence, not completion of this integration task.
- [x] 1.3 Deliver strict request/result schemas for the four tools and private version handshake from design sections 5–9. Verify examples, unknown-field rejection, finite numeric types, bounded framing/nesting, availability values and the complete error envelope with schema/codec tests before adding action handlers.

## 2. Isolated build and editor lifecycle (after 1)

- [x] 2.1 Add the OFF-by-default build option and separate instrumented ImGui/editor UI/render/application targets, compiling the full required engine source set with capture/ImPlot disabled. Build ordinary and automation variants together; inspect link/configuration evidence for no mixed ImGui definitions and no new runtime public dependencies.
- [x] 2.2 Add automation composition and RAII ownership with engine Stop before backend teardown and engine destruction after ImGui context destruction. Verify normal and injected partial-construction teardown order with lifetime tests; queued work must not retain a destroyed document/session.
- [x] 2.3 Integrate the real frame loop and one persistent generic TestFunc, with no replacement workspace GuiFunc or feature scenario registration. Verify two requests share the document and active input, idle coroutine yields, callbacks/rendering never race transport access, and session lifetime does not trip the engine's ordinary per-test watchdog.
- [x] 2.4 Publish end-of-frame immutable snapshots and service control work around minimized waits using a wakeup. Verify revision ordering, unavailable-frame reporting and cancellation while minimized without a busy render loop; defer real presentation validation to section 8.

## 3. Session ownership and confined effects (after 2)

- [x] 3.1 Implement inherited-pipe handshake, one-owner/one-batch state, fixed executable/fixture configuration, bounded queues and owned temporary file slots. Verify mismatch/second-owner rejection, source fixture preservation and start refusal without a permitted environment; opening a transport alone must not launch a window.
- [ ] 3.2 Enforce file policy at every editor level-read/write/pending-decision boundary and block game process creation in the test profile. Test allowed temporary save/open plus outside, traversal, UNC/device, alternate-stream, symlink/junction/reparse and non-owned overwrite cases; normal editor file/Play behavior must retain its regressions. Implementation and available checks pass; two actual symlink checks remain blocked because this Windows token lacks link-creation privileges (see `validation.md`).
- [x] 3.3 Isolate physical GLFW events, separately disable physical camera capture/viewport interference, suppress OS cursor warps/changes and use a session-local clipboard. Verify no camera/document/host-clipboard change from physical input during running and idle batches while close/window events remain handled.
- [x] 3.4 Implement finite step/batch/idle/session deadlines, cancellation priority, release of synthetic keys/buttons/modifiers and bounded child shutdown. Verify cleanup and truthful partial/stale results under cancellation, EOF, engine failure and stalled frame progress; no held input may cross a normal request boundary.
- [x] 3.5 Add host-owned Windows Job Object cleanup before session startup, with non-inherited kill-on-close handle and startup failure on assignment errors. Process tests must verify host crash kills its owned responsive or unresponsive child and cannot affect an unrelated process.

## 4. Semantic observation and read-only inspection (after 3)

- [x] 4.1 Add UI-local metadata helpers and immutable records for scopes, keys, owners, capabilities, components and values. Verify they carry no mutation callbacks/pointers and compile away in ordinary UI; combine them with current engine item facts without using interactive read helpers.
- [x] 4.2 Implement refs/exact selectors and local structural incarnations; repair real duplicate ImGui identities where necessary. Test label rename/layout/scroll stability, generation replacement, deletion/recreation, reordered nested rows, ambiguity and expired refs without a first-match fallback.
- [x] 4.3 Implement passive bounded `ui_observe`, active-buffer sampling, typed drafts, availability/provenance, scope coverage and snapshot pagination. Tests must preserve active input/focus/history/dirty state and distinguish incomplete numeric text, closed options, menu-layer metadata, clipped content and expired cursors.
- [x] 4.4 Implement the explicit application projections and UI-to-applied-field bindings. Test const snapshot access to document, selection, history, concrete objects, diagnostics and previews; verify unknown fields are rejected and UI draft/document/preview revisions cannot be mistaken for one another.

## 5. Dynamic executor and failure semantics (after 4)

- [x] 5.1 Dispatch the finite data-only operation set from arbitrary runtime batches in the persistent coroutine. Verify full static prevalidation, per-step current target resolution, initial revision guards and newly revealed targets, with no generated C++, script evaluation or application-command shortcut.
- [x] 5.2 Add typed assertions and bounded `wait_until` over UI/application snapshots. Test equality/range/approx/literal contains/count, explicit numeric tolerance, unavailable values, passive wait and a failing assertion that prevents later Save while retaining prior edits.
- [x] 5.3 Implement per-step results, verified completed prefix/not-run suffix, explicit unknown steps when execution evidence is lost, error mapping, request-ID deduplication and bounded result retention. Verify identical retries never repeat mutations, conflicting/expired IDs fail, and recovery never resumes a failed batch implicitly.
- [x] 5.4 Integrate engine-error detection with cleanup/faulted-session behavior. Test recoverable target/assertion failures followed by explicit observation/corrected requests, and fatal engine/cleanup faults requiring a new session with no replay or fabricated final state.

## 6. Shared adapters for the real workspace (after 5)

- [x] 6.1 Cover buttons, menu items, checkboxes, selectable lists and both combo patterns using real Test Engine interactions. Verify enabled/disabled/check/open/selection state, duplicate-label disambiguation and popup option discovery on actual editor controls, without direct handlers.
- [x] 6.2 Cover single-line and multiline UTF-8 text with separate edit/Enter/Tab-deactivation semantics. Test Cyrillic, newlines, capacities, a rejected identifier, and draft preservation across observe/inspect/execute requests before a real commit.
- [x] 6.3 Cover InputFloat, DragFloat, DragFloat3 and SliderFloat, including component metadata. Verify ambient Enter behavior, deferred property changes, active incomplete text, normal undo/history and transient seek; no numeric memory writes substitute for input.
- [x] 6.4 Cover ColorEdit3(NoPicker) components and encoding/rounding, collapsing headers, scopes and semantic scrolling. Verify color round trips through widgets, unsubmitted/collapsed discovery, modal blocking and page/item scrolling without client coordinates.
- [x] 6.5 Register read-only diagnostics and readable-preview semantics and explicit unsupported viewport/gizmo operations. Run a coverage inventory against all actual editor widget families; every mandatory standard-family gap remains unchecked, and excluded actions must return `unsupported` without OS mouse or direct scene-command fallback.

## 7. Local MCP host (after 6)

- [x] 7.1 Implement the narrow stdio MCP lifecycle and four tools around the established child/session protocol. Conformance tests must check initialization/version negotiation, tool input/output schemas, stdout purity, UTF-8 framing, structured/text results, protocol versus domain errors and no unsupported advertised capabilities. The initial handwritten implementation is superseded by the official SDK work in section 12.
- [x] 7.2 Keep cancellation/status responsive during execution, enforce pre-parse size/nesting/partial-line limits on both channels, and enforce serialized response limits. Test fragmented/oversized/unterminated input, slow/broken pipes, bounded diagnostics and closed sessions without leaking memory or blocking cleanup.
- [x] 7.3 Provide a bounded runtime JSON client/driver for arbitrary batches and fixture selection, retaining transcripts/results. Verify it needs no named scenario catalogue and exposes no shell, arbitrary executable/path, eval, reflection, pointer or raw-coordinate operation.

## 8. Integrated acceptance and hardware checks (after 7)

- [x] 8.1 Run normal configure/build, affected deterministic tests and boundary checks following `docs/DEVELOPMENT.md`, including a fresh option-OFF configuration with no Test Engine fetch. Delegate existing automated UI execution to `ui_test_runner`; retain command/exit/result evidence and report unavailable checks.
- [x] 8.2 In an explicitly authorized test environment, run an actual MCP client -> host -> dedicated real editor scenario: discover, edit uncommitted, observe/inspect, commit, Undo/Redo, Save As to a temporary slot, reopen and verify applied state across requests. Verify no screenshot, image processing, hand-authored document mutation or desktop input is involved.
- [x] 8.3 Demonstrate the decisive extension test: add an actual editor control of a supported family with ordinary adjacent metadata (and a permitted read-only field binding), rebuild only affected application targets and discover/change/assert it using a new runtime batch. Preserve diff evidence showing no MCP/generic-executor/scenario-handler modification; do not replace this with a synthetic test-only window.
- [x] 8.4 Through the integrated path verify missing/ambiguous/stale/disabled/modal/unsupported targets, assertion failure, denied paths/Play, timeout, cancellation, disconnect, duplicate requests and host death. Each case must show the stopped suffix, correct partial effects and released input or bounded owned-child termination.
- [x] 8.5 Run ordinary and automation editor Vulkan lifecycle validation on available authorized hardware: resize, minimize/restore, cancel while minimized, presentation recovery, partial construction, controller loss and final destruction. Require zero error-severity validation messages; record blocked checks without claiming visual acceptance or completion of an unverified mandatory integration gate.
- [x] 8.6 Record representative snapshot size, frame/round-trip latency and coverage results; classify visual checks separately as passed/failed/blocked/not_run. If appearance checks are requested, delegate a prepared authorized scenario to `ui_driver`; absence of that run must remain an explicit visual limitation, not a semantic-test failure or an implied pass.

## 9. Documentation and review (after 8)

- [x] 9.1 Update `docs/DEVELOPMENT.md`, `docs/ARCHITECTURE.md` and `.codex/agents/ui_test_runner.toml`/`ui_driver.toml` only as needed for the implemented commands, dependency/license notices, ownership, four-tool workflow, coverage/error examples and separate visual authorization. Verify documented commands and a new-control example against the integrated build.
- [x] 9.2 Review the actual diff and independent lifetime/input/persistence-boundary findings, rerun affected checks for corrections, and run `openspec validate add-semantic-imgui-automation --strict`. Record implemented versus excluded coverage and all unresolved acceptance risks; do not check off unavailable mandatory work or archive as part of this planning request.

## 10. Core-stage continuation and evidence audit

- [x] 10.1 Join passive metadata with actual pinned Test Engine facts and per-source freshness; retain typed drafts independently of invalid numeric input and correct disabled/selection/color facts. Verify active input, focus, clipboard, popup, dirty and history invariants.
- [x] 10.2 Close nested-row reference reuse gaps for combined edits/reorders and equal-content structural actions; document conservative invalidation and verify scope/depth/pagination behavior.
- [x] 10.3 Implement explicit strict/assisted execution policy with no implicit popup dismissal, ancestor opening, window resizing or layout movement; return auxiliary action evidence and relevant error state.
- [x] 10.4 Add a data-only drag operation using real engine mouse-down/movement/release, separate from numeric editing; verify real property commit, cancellation and input cleanup.
- [x] 10.5 Add the deliberately non-applying test handler regression, wrong-type and timeout actual-value checks; retain runtime multi-batch and fail-fast tests.
- [x] 10.6 Run integrated builds, affected deterministic/native widget tests and authorized Vulkan checks; retain a real request/response pair and exact results, report exclusions and unavailable gates.
- [x] 10.7 Reconcile design/spec/developer documentation, review actual changes and independent findings, run strict OpenSpec validation, update this checklist without archiving.

## 11. Observation truthfulness follow-up

- [x] 11.1 Correct immediate preview/placement commit metadata using adjacent value-only annotations; preserve real handlers and gesture methods, and verify metadata on the real editor controls.
- [x] 11.2 Report truncated inactive input and labels explicitly, reject full comparisons against their prefixes with actual unavailable-value evidence, and add regression coverage.
- [x] 11.3 Rebuild both editor variants, run affected in-memory and protocol checks, retain a new actual request/response pair, and record unavailable native/visual checks without archiving.

## 12. Official SDK MCP adapter continuation

- [x] 12.1 Replace the handwritten MCP lifecycle/dispatch with the official pinned Python SDK, preserving the four strict tool contracts, editor-side batch execution, bounded transport, cancellation and owned-session lifecycle. Retain bounded diagnostic artifacts and return their locations.
- [x] 12.2 Exercise initialization, tools/list, schemas and tools/call with the official SDK client, including malformed input, version negotiation, bounds, cancellation and disconnect. Verify uncertain outcomes and no mutation replay.
- [x] 12.3 Run the SDK client through the host to the real editor and verify actual UI effects, editor death, incompatible private protocol and failure midway through a batch. Retain real exchanges and Vulkan shutdown evidence; mark unavailable checks blocked.
- [x] 12.4 Document reproducible pinned setup, exact run/test commands and a portable project Codex configuration template checked against current official documentation. Review the integrated diff and independent lifetime findings, build affected targets and run strict OpenSpec validation without archiving.
