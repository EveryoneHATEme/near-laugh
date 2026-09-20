## Context

See `proposal.md` for the intended outcome. Research baseline: clean commit `b784ec7`, 2026-09-17; no project implementation was changed during planning.

- `CMakeLists.txt` pins ImGui `v1.92.9b-docking`; the fetched checkout is `b48d1afbe8ee8b238e2961dc363a949dd7304e23`, version number 19291. `near_laugh_imgui` includes GLFW/Vulkan backends and exports `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`. Editor UI/render targets share it; the game must retain its existing dependency boundary.
- `EditorGlfwBridge` creates the ImGui context and installs/chains GLFW callbacks. `EditorApplication::tick` polls events, starts backend/ImGui frames, draws the real workspace, processes previews, finishes ImGui and renders Vulkan. Renderer destruction precedes bridge/context destruction. The minimized path waits for events; `postEmptyEvent` can wake it.
- `EditorUi::updateViewport` uses the passthrough dockspace and mouse-derived rays, not a normal ImGui viewport widget. `updateNavigation` separately reads physical `Window` input and captures the OS cursor. No ImGuizmo integration was found.
- `EditorPropertyEdit` has a draft distinct from `EditorDocument`. Property text/drags generally commit on deactivation, ambient input uses Enter, checkboxes/combos apply immediately, and preview controls affect transient state. `EditorObjectId` is transient; document replacement advances `generation()`. Some nested records use array indices and some labels participate in ImGui identity.
- `tests/core/test_editor_ui.cpp` drives real ImGui frames in memory, but uses coordinates, internal hashes and `ActivateItemByID`. Existing tests are retained as regressions; they do not establish the new MCP path.
- Current editor, toolchain, input and persistence specifications remain the baseline. The normal editor's file/play behavior is preserved; the explicitly identified test profile is restricted. The unrelated v9-main-spec/v10-household-code discrepancy is not resolved by this change.

## Goals / Non-Goals

**Goals:** implement one concrete editor test seam, one small local protocol, shared widget adapters, and reliable structured evidence. Metadata describes controls and copies observable values; it never supplies mutation callbacks. Real widget code remains responsible for editing, committing, validating and creating history.

**Non-Goals:** see the proposal's v1 exclusions. Supporting an additional standard control requires an adjacent registration at most. Supporting a genuinely new widget family requires an explicit adapter and tests; supporting a new game feature composed of existing families does not. There is no promise of automatic DOM construction or inspection of every piece of C++ state.

## Decisions

### 1. Dependency pin, license and verified API surface

Select Test Engine **`2628e39cc0ea3a0a612d5d039543c9d4e873c720`**, dated 2026-09-15, whose change specifically amends compatibility below ImGui 19297. Keep the existing ImGui pin and every other project dependency. Research uses a disposable checkout under `build/semantic-imgui-research/`; implementation must use the full immutable SHA, not `main`, a moving date or a shallow tag assumption.

Primary sources inspected:

- [Pinned engine header](https://github.com/ocornut/imgui_test_engine/blob/2628e39cc0ea3a0a612d5d039543c9d4e873c720/imgui_test_engine/imgui_te_engine.h): context lifecycle, queue/abort interfaces, `ImGuiTest::UserData`, `TestFunc`, optional `GuiFunc`, `TeardownFunc`, item records.
- [Pinned automation header](https://github.com/ocornut/imgui_test_engine/blob/2628e39cc0ea3a0a612d5d039543c9d4e873c720/imgui_test_engine/imgui_te_context.h): `ItemInfo`, `GatherItems`, `ItemClick`, `ItemCheck`/`ItemUncheck`, `ItemOpen`/`ItemClose`, `ItemInput`, `KeyCharsReplace`, `KeyPress`, `ComboClick`, scrolling and yielding.
- [Pinned context implementation](https://github.com/ocornut/imgui_test_engine/blob/2628e39cc0ea3a0a612d5d039543c9d4e873c720/imgui_test_engine/imgui_te_context.cpp): `ItemInputValue` replaces text **and presses Enter**; `ItemReadAsScalar`/string readers interact with the field and clipboard. They are not passive reads and must not implement `ui_observe`. `GatherItems` yields across frames and currently gathers the main navigation layer.
- [Pinned engine implementation](https://github.com/ocornut/imgui_test_engine/blob/2628e39cc0ea3a0a612d5d039543c9d4e873c720/imgui_test_engine/imgui_te_engine.cpp): test startup clears input; test teardown restores IO/style and clears queued events. Keep one running dispatcher across requests to avoid those resets at request boundaries.
- [Pinned configuration](https://github.com/ocornut/imgui_test_engine/blob/2628e39cc0ea3a0a612d5d039543c9d4e873c720/imgui_test_engine/imgui_te_imconfig.h), [Setting Up](https://github.com/ocornut/imgui_test_engine/wiki/Setting-Up), [Named References](https://github.com/ocornut/imgui_test_engine/wiki/Named-References). Wiki checked on 2026-09-17; pinned source is authoritative if examples differ.

Engine code uses **Dear ImGui Test Engine License v1.04**, not the MIT license of other upstream folders. The [pinned license](https://github.com/ocornut/imgui_test_engine/blob/2628e39cc0ea3a0a612d5d039543c9d4e873c720/imgui_test_engine/LICENSE.txt) offers free use for the listed individual/non-profit/educational/open-source cases or entities below USD 2 million turnover in their previous fiscal year. Otherwise it permits a 45-day trial followed by a paid license requirement. Distribution requires its copyright/license notices. The developer's legal eligibility is not inferred from repository visibility or the project description; record the applicable basis before integration/distribution. Capture=0 does not remove license obligations.

Use `IMGUI_ENABLE_TEST_ENGINE`, `IMGUI_TEST_ENGINE_ENABLE_COROUTINE_STDTHREAD_IMPL=1`, `IMGUI_TEST_ENGINE_ENABLE_CAPTURE=0`, `IMGUI_TEST_ENGINE_ENABLE_IMPLOT=0`, retaining `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`. Compile all eight required engine translation units, including `imgui_capture_tool.cpp`: it provides lifecycle stubs even with capture disabled. Do not link the upstream test-suite application, ImPlot or external capture tools such as ffmpeg. Do not expose the engine's own test browser, source-opening, shell, capture or export utilities through the adapter. Disable saved settings, capture-on-error and debugger breaks; control logs explicitly.

Compatibility was checked with a standalone dependency-only probe: all eight upstream engine `.cpp` files and four core ImGui `.cpp` files compiled and linked with Clang 23.1.0, C++20, Debug, x64 MSVC ABI and the configuration above. Create/start, three NewFrame/Render frames, Stop, ImGui destruction and engine destruction completed with exit 0, without a window or input. The build log contains no warning/error. This establishes this dependency pair's compile/link and minimal lifetime compatibility, not queued TestFunc execution, editor adapters or GLFW/Vulkan integration.

Reproduction commands (generated probe source/config and complete logs are local research artifacts, not shipped implementation):

```powershell
cmake -S build/semantic-imgui-research/compatibility -B build/semantic-imgui-research/compatibility/out -G Ninja -DCMAKE_CXX_COMPILER=D:/LLVM/bin/clang++.exe -DCMAKE_BUILD_TYPE=Debug
cmake --build build/semantic-imgui-research/compatibility/out -j 4
& ./build/semantic-imgui-research/compatibility/out/compatibility_probe.exe
```

All three commands returned 0. Evidence: `build/semantic-imgui-research/compatibility/{configure,build,run}.log`; probe sources are in that same directory. The absolute compiler path records this machine's experiment only; project presets continue using portable Clang names. Future builds must reproduce the check rather than depend on these ignored artifacts.

### 2. Process and build boundary

```text
coding agent / ui_test_runner
  -> MCP stdio: four tools
  -> Python official MCP SDK host (one owner; fixed executable/fixture roots)
  -> inherited anonymous parent/child pipes: bounded versioned JSON messages
  -> level_editor_automation: request queue + snapshots + session policy
  -> one persistent ImGui Test Engine TestFunc
  -> real EditorUi widgets -> existing document/preview behavior
```

Use `scripts/editor_ui_mcp.py` as the host entry point and `scripts/editor_ui_sdk.py` as the MCP adapter. The 2026-09-18 user instruction supersedes the previous handwritten-standard-library decision: use official `mcp==2.2.0`, with its complete Windows x64 / Python 3.12 dependency closure pinned in `scripts/requirements-editor-ui.txt`. The SDK's low-level `Server` accepts the existing exact typed JSON Schemas without duplicating them as Python models; it owns version negotiation, initialization, method dispatch, errors and cancellation. The SDK client tests explicitly exercise the `2025-11-25` initialization handshake and current discovery negotiation. No handwritten MCP lifecycle or method router remains. The custom transport only frames bounded UTF-8 JSON and passes SDK message objects to/from `Server.run`, because default SDK stdio reads do not enforce this project's pre-parse bounds. [Official SDK low-level API](https://py.sdk.modelcontextprotocol.io/advanced/low-level-server/) documents this schema-preserving interface. [MCP stdio](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports) reserves stdout for protocol messages; logs go to stderr/files. Results retain `structuredContent`, output schemas, JSON text and `isError` for domain failures. Malformed tool arguments use SDK protocol errors; corrupt/oversized/incomplete byte framing closes the connection with a bounded stderr diagnostic. Neither connection nor tool discovery launches an editor.

The SDK invokes the existing session controller in four worker threads with at most 14 regular calls admitted, plus two reserved control workers/calls. Status is immediate; the periodic watchdog remains independent. Queued cancelled work cannot later launch or mutate; cancellation of a started startup closes only the session it acquired. EOF immediately coordinates owned-child shutdown within one cleanup deadline. Execution cancellation waits for cleanup within the same deadline and reserves termination time. The application batch is sent once; there is no model call or Python action resolver between its steps. No automatic reconnect/replay occurs. Results are evidence, not an exactly-once guarantee after lost replies.

Each child retains an exclusive temporary stderr artifact capped at 16 MiB, with an 8-KiB tail kept for cleanup diagnostics. Tool results return optional `diagnostic_path`; session close does not erase these logs. If duplicate text/structured encoding would exceed the MCP response budget, retain the full bounded result in a separate JSON artifact and return its path with a compact `limit_exceeded` result preserving step statuses, revisions and effects. Retain at most 32 such result files per host, each at most 1 MiB. Older artifact paths can expire; logs/results are local operator artifacts and may be removed after review.

The host starts only a configured `level_editor_automation` executable using literal arguments and `shell=False`. Child stdin/stdout carry the private protocol; editor/engine diagnostics use stderr. Only those inherited handles are exposed, no named public endpoint, port, arbitrary executable argument or attach-by-PID tool. The child belongs to a host-owned Windows Job Object configured with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`; its job handle is not inherited. Use a small standard-library `ctypes` Windows binding, establish job membership before the child is allowed to initialize a session, and fail startup/terminate the child if assignment fails. This bounds parent-crash cleanup even if the child is stuck in the driver and cannot process pipe EOF; the ownership behavior follows the [Windows Job Object contract](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects). Version/build/dependency fingerprints are exchanged before readiness. A second start is `busy`; ownership dies with the pipe/job. This is simpler than local TCP authentication or a globally discoverable named pipe for a single owner and child. v1 is Windows-only; no unsupported-platform transport abstraction is introduced.

An OFF-by-default CMake option adds **separate** instrumented ImGui/UI/render/application targets and `level_editor_automation`. All TUs that include ImGui for this target use the same configuration; no executable mixes ordinary and instrumented copies. Reuse source lists and existing non-ImGui core libraries; do not duplicate application logic. The normal `near_laugh_imgui`, `level_editor`, game and existing tests are unaffected even when both variants are built. No Test Engine fetch is needed with the option OFF. Instrumentation metadata compiles away in ordinary builds. A small file policy seam in document/workflow code defaults to current normal behavior; Test Engine/MCP types never enter editor core or public runtime headers.

### 3. Ownership, scheduling and lifetime

The automation composition owns transport state, queued value objects, metadata snapshots and engine RAII lifetime. The IO thread parses bounded requests, handles EOF/cancellation and publishes wake requests; it never calls ImGui or reads mutable document/UI data. The default Test Engine std::thread coroutine alternates cooperatively with the frame thread: only the active frame/coroutine phase accesses UI state, and it never runs concurrently with rendering. Use immutable end-of-frame copies for application inspection. No raw editor pointer crosses transport or persists in metadata.

Register one generic test with a non-capturing `TestFunc`, session-owned `UserData`, no replacement `GuiFunc`, and a teardown cleanup function. Queue it once per session. It yields while idle and dispatches bounded observation/action requests until close/expiry. A request is a validated data record, not generated C++, a new test registration or a feature name switch. Do not use Test Engine's ordinary 60-second whole-test watchdog as a per-request clock for this long-lived dispatcher: configure it for the bounded session lifetime, with independent operation deadlines and host watchdogs.

Create the engine after the ImGui context exists and start it against that context before automation frames. The real `EditorUi` continues to draw from `tick()`. Publish a snapshot after UI/preview processing and ImGui frame completion, tagged with the document revision sampled then. Input operations yield and are checked against a subsequent completed frame. This establishes ordering, not rendering completion or preview-resource freshness; expose those separately. Add the upstream pre/post presentation hooks at their actual locations in the renderer; captures stay disabled.

Shutdown order: stop accepting work, request cancellation and wake the loop, finish/abort the coroutine and release input while contexts exist, call engine Stop, release editor Vulkan/ImGui Vulkan backend resources, release GLFW backend and ImGui context, then destroy the engine context, window and platform in valid order. In particular, upstream requires engine context destruction **after** ImGui context destruction. Integrate this with `EditorBridgeLifetime`/`EditorGlfwBridge` RAII and partial-construction cleanup; a naive engine member destroyed before the bridge is incorrect. The TestFunc cannot outlive its session storage or document. Diagnostics remain alive through GPU teardown.

When minimized, continue servicing control/cancel deadlines without busy-rendering: wake the event wait through `postEmptyEvent`, check requests before waiting again, and return `ui_unavailable` for interactions needing a rendered UI. A hung render/driver is handled by the parent deadline; do not call ImGui from a watchdog thread.

### 4. Observation sources and identity

Test Engine supplies frame-bound item IDs, ID-stack parent/depth, window association, rectangles, item/status flags and a **truncated** debug label (`DebugLabel[32]`). Status timestamps may lag; `GatherItems` is scoped and frame-dependent, not a retained accessibility tree. Closed popup contents, collapsed branches, non-submitted clipped items, text and menu-layer entries cannot be assumed complete. Observe does not open, scroll, focus or commit anything to discover more.

Add small UI-local registrations for stable scope/control keys, full labels, concrete widget family, object identity, composite component IDs, value type/range/unit, supported operations, commit policy, read-only draft copies and allowed application bindings. Register menu items/scopes and non-interactive text explicitly where gathering cannot describe them. Shared helpers cover existing widget families; registrations contain **no setter, command callback, arbitrary member address, or reflection registry**. Collect primitive copies immediately adjacent to widget calls; a post-frame snapshot joins these with current Test Engine facts. Record unknown facts rather than infer them from labels.

For an active input, copy the input buffer using one pinned, read-only ImGui compatibility helper (`GetInputTextState`, active ID, current UTF-8 buffer); do not call engine `ItemReadAs*`. `InputFloat`'s temporary formatted input is not the applied float, and entering `-` can yield an unparseable buffer while a previous typed draft remains known. Color fields report encoding/rounding and component values. Copy document projections only after the real UI commit path has run.

Refs are opaque handles backed by `(session epoch, document generation when applicable, semantic scope/key, owner object ID, structural incarnation)`. ImGui IDs and paths are internal resolved addresses, not durable protocol identities. Rename, layout, scroll and unrelated value edits preserve a ref if the semantic owner survives. Document replacement, deletion/recreation and reordered index-only rows invalidate affected refs. Nested collision boxes, readable pages and route entries conservatively advance a local structural epoch on any array value change and on explicit structural UI gestures (including equal-content reorders); an old ref never silently selects a new row. Repair duplicate ImGui IDs with stable owner/component ID scopes where necessary. Static controls can survive document replacement; object-bound refs cannot. Identity across application restarts is not promised.

An exact selector uses `{scope, key, owner?}` from observation; `owner` is an observed object ref or the explicitly requested `selection` alias. It resolves against the current scope at each step, so an earlier Add/Open can reveal later targets in the same batch. Zero/multiple matches produce `not_found`/`ambiguous_target`; no fuzzy matching, first-match choice or wildcard traversal. Unsupported raw items may receive snapshot-only refs with explicit expiry and no guessed operations.

### 5. Public tool contract (version 1)

All calls use strict JSON schemas with unknown properties rejected. Integers/IDs that exceed JavaScript precision are encoded as opaque strings. Finite typed numbers only; raw text inputs can still test rejected numeric text. Responses include `protocol_version`, `session_id`, `request_id` and build fingerprint. Domain errors share the execution error envelope below.

| Tool | Request | Response and limits |
|---|---|---|
| `ui_session` | `op: start/status/cancel/close`; start has an allowlisted `fixture` and declared `environment`; cancel has an active request ID | State `starting/ready/executing/faulted/closing/closed`, capabilities/limits, temporary file slots, last request outcome; one child and owner. No arbitrary command, path, reset-document setter or attach operation. |
| `ui_observe` | session; `scope` root or scope ref; optional exact filter; bounded depth/page size; continuation cursor | Immutable snapshot ID, frame/document/selection revisions, active/modal scopes, items, coverage and next cursor. Default fresh completed frame; inability to obtain it is an error, with last snapshot explicitly stale. |
| `ui_execute` | session; unique request ID; optional initial generation/revision guard; ordered `steps`; explicit assistance policy; bounded batch timeout | Per-step status/results, last completed/failing index, before/after snapshot IDs/revisions, partial effects and cleanup state. One batch in flight. |
| `app_inspect` | session; allowlisted projection with bounded object refs/fields; optional existing snapshot ID | Read-only values from the same completed-frame snapshot model, per-value availability and source; no calls to mutating getters or arbitrary object graph traversal. |

`environment` is an operator-configured permitted test desktop/profile ID, not a self-granted permission. Session start must fail `environment_not_authorized` if there is no configured authorization for that run or isolated environment. It can return `environment_unavailable` for missing window/GPU support. The host does not seize the desktop on MCP connection; starting a session is explicit. Hardware availability never causes a fallback to the user's editor or OS mouse.

Each observed item contains:

```json
{
  "ref": "opaque-session-item-handle",
  "scope": "properties",
  "key": "position.x",
  "owner": "opaque-object-handle",
  "label": "Position X",
  "kind": "number",
  "capabilities": ["focus", "edit", "commit", "assert"],
  "state": {"submitted": true, "visible": true, "enabled": true,
            "active": true, "focused": true, "selected": null, "open": null},
  "value": {"availability": "known", "type": "float", "draft": 1.5, "unit": "m"},
  "input": {"availability": "known", "text": "1.5", "uncommitted": true},
  "commit": {"policy": "deactivate", "methods": ["tab"]},
  "applied_binding": {"projection": "object", "field": "position.x"},
  "provenance": {"identity": "ui_metadata", "state": "imgui", "value": "ui_draft"}
}
```

This is an illustrative shape, not a fabricated real object ID. `unknown`, `unavailable`, `not_applicable`, `truncated` and `known` are distinct availability values; `null` is never silently zero/false. State fields also carry availability when not known. Snapshot coverage identifies `submitted_only`, `collapsed`, `closed_popup`, `clipped_unsubmitted`, `unregistered`, and pagination/truncation. Options of a closed combo are not claimed discovered. Explicitly open it, then observe the popup. Submitted offscreen items can be revealed by an execution action using engine scrolling; unsubmitted content requires explicit expansion or semantic page scrolling and re-observation.

Version 1 limits: 1 MiB per message on **both external MCP stdio and private pipes**, enforced by incremental bounded framing before JSON parsing; 64 steps/batch; 256 items/page; 4096 items/snapshot; scope depth 16; input strings no longer than both 16 KiB UTF-8 and the widget's published capacity. An oversized line or a partial line unfinished for 5 seconds after its first byte closes that connection and cancels its session, with a bounded stderr diagnostic if writable; idle connections with no partial message are distinct. Reject excessive JSON nesting before recursive decoding. Enforce the output byte budget as well, including duplicated structured/text representations, by bounded pages or explicit limit errors. Report text truncation and reject full equality assertions against truncated/unknown values. Default/max operation deadlines 5/30 seconds, batch 20/60 seconds, cancellation grace 2 seconds, startup 30 seconds, idle session 5 minutes, maximum session 60 minutes. These are protocol limits, not speed claims. Snapshot/result storage is bounded and old handles/cursors report `snapshot_expired`/`result_expired`; never rerun a mutation to reconstruct a result.

Normative request shapes below use `?` only as documentation for an optional property; it is not wire syntax. Common fields on every tool call are `protocol_version: 1` and `request_id: string`. Execution IDs are monotonically increasing decimal strings within the session (separate from the MCP JSON-RPC call ID); result retry uses the original ID. Non-start calls include `session_id`; status before start may omit it. Unknown fields are rejected.

| Tool / variant | Additional fields |
|---|---|
| `ui_session`, start | `op: "start"`, `fixture: string`, `environment: string` (both allowlisted IDs) |
| `ui_session`, status/close | `op: "status"` or `"close"` |
| `ui_session`, cancel | `op: "cancel"`, `active_request_id: string` |
| `ui_observe`, first page | `scope: "root"` or scope ref, `filter?: {key?: string, kind?: string, owner?: string}`, `depth?: integer`, `page_size?: integer` |
| `ui_observe`, continuation | `cursor: string` only; scope/filter/page size are bound to that cursor |
| `app_inspect` | `projection: document|selection|history|objects|object|diagnostics|preview`, `object_ref?: string`, `fields?: string[]`, `snapshot_id?: string`, `page_size?: integer`, `cursor?: string`; object projection requires object_ref, continuation permits only cursor |
| `ui_execute` | `steps: Step[]`, `timeout_ms?: integer`, `policy?: {auto_scroll?: boolean, auto_focus?: boolean}`, `if_state?: {document_generation?: string, document_revision?: string}` (guard checked before first action) |

`Target` is exactly one of `{ref: string}` or `{selector: {scope: string, key: string, owner?: string}}`. Each `Step` has `op` and optional `timeout_ms`; input actions require `target: Target`. `activate/focus/open/close/commit` have no additional payload except `commit.method: enter|tab`; `set_checked` takes `value: boolean`; `select` targets an actual option; `edit` takes exactly one of `value: finite number` or `text: string` plus optional `commit: none|enter|tab`; compound components are targets rather than arbitrary vector memory. `key` takes a published `chord`; `scroll` takes exactly one of `{direction: up|down, pages: integer}` or `{to: Target}` within its target scope. `assert/wait_until` take `condition` as defined in section 6 and no input target. All enum values are JSON strings. A step result has `index`, `status: passed|failed|not_run|unknown`, frame/revision stamps and, when verified, an observed result or the error envelope. The batch response represents known unexecuted suffix steps as `not_run`. If child death, transport loss or stalled cleanup prevents establishing execution, retain the verified prefix and report affected steps as `unknown`, with no invented observed value or final frame. Unknown steps require `effects: unknown` and stale or unavailable final evidence; they cannot form a successful batch. Every requested step still has a result entry. This distinction was approved during implementation after identifying that a lost child result cannot prove non-execution.

### 6. Batch operations, assertions and value transitions

The finite operation set is `activate`, `focus`, `open`, `close`, `set_checked`, `select`, `edit`, `commit`, `key`, `scroll`, `drag`, `assert`, `wait_until`. Targets are refs or exact selectors. `edit` accepts a typed scalar/component or UTF-8 replacement text, with explicit `commit: none/enter/tab`; defaults to `none` for draft-capable inputs. Immediate controls advertise that fact. `key` permits bounded UI chords (Enter, Escape, Tab/Shift-Tab, navigation, and editor Undo/Redo/Save shortcuts), not arbitrary key codes, holds or OS shortcuts. `scroll` uses direction/page counts or an observed item ref, never model-supplied pixels. `select` activates a real observed selectable; it does not assign the combo's index directly. An `open` followed by an exact popup-option selector works within one batch.

Adapters map to the checked Test Engine API: real click/check/open/input/key/scroll methods. Numeric editing starts the real temporary input and uses text injection; it does not write a float pointer. Component mapping handles `DragFloat3` and `ColorEdit3` child IDs. `ItemInputValue` is only usable when its implicit Enter matches the explicitly requested commit. Multiline Enter inserts a newline; Tab deactivation is a separate supported commit, verified against current widget flags. Esc is an input gesture, not a guaranteed document rollback. Every requested gesture must be advertised for the target; otherwise return `unsupported`.

Assertions use a finite record `{source: ui|app, target/projection, field, predicate, expected, tolerance?}`. Predicates: exists, equals, not_equals, numeric range, approx (explicit absolute/relative tolerance), contains (literal text), and count. No expressions, regex execution, loops, variables, functions, branching, includes or scripts. `wait_until` repeats one such observation predicate until its finite deadline; it cannot repeat mutation steps. Unknown values fail with `value_unavailable`; they cannot satisfy a negative or default-value assertion. After each input operation, yield until a post-operation snapshot exists; assert document application separately. Completion of `edit` is never proof of commit or valid Save.

Example interaction sequence: observe selected property -> edit with no commit -> assert `input.text` -> inspect old applied document field -> separate request commits by the advertised method -> assert applied value and dirty/history -> activate Undo -> assert old applied value. The same long-running dispatcher and context retain focus, popups and text buffers between those requests. Passive observation must not change history, focus, active input or dirty state.

Prevalidate the entire batch shape, sizes, operation names and static argument types before any action. Resolve dynamic targets and check capabilities/state immediately before each step; later targets may not exist yet. Failure stops the batch without rolling back earlier real UI effects. A disabled or modal-blocked target is diagnosed before an attempted click. Application validation failure is an observable result: a commit gesture can succeed while the document refuses its value, and assertions must distinguish this from engine failure.

### 6.1. Core-stage contract corrections (2026-09-17)

- `drag` is distinct from `edit`, `commit` and `key/Escape`. Its payload is
  `{target, direction: increase|decrease, fraction: (0,1], frames?: 2..120}`.
  Distance is a fraction of the current widget width, default four movement
  frames. Only advertised horizontal numeric/vector/RGB/slider components
  accept it. Engine item geometry determines the private target and movement;
  the client supplies no pixel coordinates. Actual mouse-down, movement over
  multiple frames, and mouse-up reach the real widget drag threshold logic
  and deactivation handler. A short gesture can leave the value unchanged;
  assertions inspect the actual resulting value.
  Reason: temporary text input cannot establish drag-and-release behavior.
- An omitted `policy` is strict: `auto_scroll=false`, `auto_focus=false`.
  Either convenience can be explicitly enabled. Neither authorizes closing an
  obstructing popup, opening an ancestor, resizing/moving a window or changing
  layout. Explicitly opening another menu remains a normal requested gesture.
  Per-step `assistance` lists attempted scroll/focus actions with before/after
  stamps, including helpers interrupted by failure. Target errors include its
  available `target_state`; timeout conditions retain expected/observed values.
  A predicate error keeps its evaluation snapshot; the batch `after` snapshot
  separately reports the later cleanup frame and resulting state.
  Reason: pinned high-level Test Engine mouse helpers can move/resize windows
  and perform implicit focus/visibility recovery. The executor uses its real
  item facts and primitive input with bounded, explicit preparation instead.
- Passive observation joins UI metadata with pinned Test Engine InfoTasks.
  `engine.availability`, `engine.frame`, and `engine.status_frame` identify the
  facts actually available; stale engine statuses never override current
  metadata. This query only registers/renews hook observations, without input
  or a coroutine yield. Newly appearing items can require another frame.
  `value.draft` remains the copied typed value when `input.text` is incomplete;
  `input_validation.status` reports numeric syntax as valid/invalid/unknown.
  This syntax fact is separate from application validation diagnostics.
- Nested rows lack durable row IDs, so any changed nested array invalidates
  its row refs; explicit UI structural notifications also cover equal-content
  reorder/reset. Re-observation or current exact selectors obtain fresh rows.
  Parent/object refs still survive unrelated edits, renames and layout changes.
  Reason: comparing permutations alone misses reorder combined with edits and
  cannot identify equal-valued replacement rows safely.
- Selectable `value.draft` is its known boolean selection, not a string inferred
  from its label. RGB color components expose the actual GUI encoding; other
  effective color modes publish explicit unsupported metadata. Applied color
  values/rounding must be checked through the concrete application binding.
- Depth-limited observation adds `depth_limit`; repeat with a larger depth or
  an observed child scope. Item/byte pages use snapshot-bound `next_cursor`.
  The snapshot hard cap remains explicit `truncation`, not a complete-tree
  claim. Waiting for a completed frame advances application time normally;
  passive observation promises no injected UI changes, not frozen simulation.

Observation truthfulness correction (2026-09-18): `label` remains the visible
label string, with optional `label_availability: known|truncated` (absence means
known for existing records). Oversized labels and inactive text buffers have
the same 16 KiB UTF-8 observation budget as active input. Truncated prefixes
cannot satisfy full-value predicates; `value_unavailable` retains the observed
prefix and its availability in the error. This corrects previously silent
truncation rather than adding an alternate read mechanism.

Commit timing is application metadata: the same scalar widget can update a
preview/placement value immediately or edit a document draft applied on
deactivation. Adjacent value-only annotations identify the immediate cases;
Enter/Tab methods still describe actual input gestures, not a guarantee of
document mutation. Widget family alone cannot establish application timing.
File-dialog Path inputs remain editable buffers: deactivation ends that edit,
while the separate Open/Save button performs the document operation. Their
buffer value is not an applied-document binding.

### 7. Application inspection boundary

Allow only explicit value projections: `document` (generation/revision, temporary path/slot, dirty/pending action, validation summary and counts), `selection`, `history` (can undo/redo and bounded summary), `objects`/`object` (concrete persisted fields for current editor record types), `diagnostics`, and `preview` (audition/character/readable state, resource revision/currentness and errors). Pagination and field selection bound output. This uses concrete project types and existing const accessors, not a reflection framework.

New controls bound to fields already exposed by these projections require no inspector changes. A genuinely new application field may add a read-only projection beside its data owner; that is distinct from a scenario handler or action implementation. UI registrations point to permitted projection fields so discovery exposes how to verify application state. No query accepts a pointer, arbitrary filesystem path, C++ member chain or mutation command. Save/reopen acceptance uses real UI and observes the reloaded document; `app_inspect` is not a hidden file-open operation.

### 8. Coverage matrix and custom boundary

This is required coverage; current implementation evidence and exclusions are recorded in `validation.md`. Every standard family below needs actual editor integration tests; unsupported mandatory cases cannot be waived as implemented.

| Actual family / evidence | Shared v1 adapter and observation | Boundaries |
|---|---|---|
| Button, MenuItem; `editor_ui.cpp` menus/object actions | activate; enabled/disabled, checkable menu state | Full labels/keys need registration; Play process launch denied by session policy. |
| Checkbox; object flags, placement mode, audio mute | set checked/uncheck through engine; checked and applied/draft state | Enabling placement/sculpt mode does not claim viewport gesture coverage. |
| Selectable, Combo, BeginCombo; objects, materials, actor/route references | open popup, discover options, select real option; current selection | No inferred closed option list; duplicate labels require stable option identity. |
| InputText; IDs and Open/Save As paths | UTF-8 edit, active buffer, draft, explicit Enter/deactivation | File boundary confinement applies even when a path is typed normally. |
| InputTextMultiline; household page text | replace text including newline, read current buffer, Tab commit | Enter is not a universal submit command; clipboard stays session-local. |
| InputFloat, DragFloat, DragFloat3; ambient and properties | typed scalar/component edit; horizontal drag for DragFloat components; commit metadata and units | InputFloat does not advertise drag. Component IDs/ownership registered; inactive draft can differ from active buffer. |
| SliderFloat; character seek | real temporary input or horizontal drag; preview state | Asserts on sampled state do not certify animated pixels. |
| ColorEdit3(NoPicker); solid tint | RGB component input and horizontal drag; display encoding, draft RGB, applied uint8 and rounding | No picker/gamut gesture; HSV/hex/hidden inputs explicitly unsupported. Retain real widget configuration. |
| CollapsingHeader, menu/window/popup/child scopes | expand/collapse, focus, semantic scrolling, modal/open state | Dock rearrangement/resize is outside v1. Menus need explicit metadata beyond main-layer gather. |
| Text/TextWrapped/diagnostic summaries | explicit read-only semantic text/value registration | Not every draw-list glyph has an item; no automatic full text tree. |
| Readable preview draw-list canvas | app projection: page, text, layout/validation diagnostics | No pixel, clipping, font-shape or visual readability certification. |
| Passthrough 3D viewport, selection overlays; `updateViewport` | scope describing unsupported actions and capability reason | Picking, placement, brush strokes, navigation and gizmo drag return `unsupported`; no current gizmo widget found. |

Future viewport support must introduce a concrete project adapter mapping an observed scene-object/surface/axis semantic target through camera projection to a verified viewport hit, then inject Test Engine input through the existing viewport/raycast/gesture path. It must resolve depth, occlusion, scaling and gesture lifetime and test cancellation. It must not call `select`, `placeSelected`, brush kernels or transform commands as substitutes for an interaction. Merely adding a named MCP tool for a feature is not that adapter. This separate extension is not required or reported complete in v1.

### 9. Safety, input isolation and errors

The session uses fixture IDs resolved from a finite manifest and copies source fixtures into a newly owned temporary directory. Temporary paths/slots are returned as values for the agent to type in real file controls. Fixture preparation is setup, not evidence of UI authoring. No existing user's editor/document is attached or modified. Normal Close/Discard dialogs remain real UI; forced session close may discard its explicitly disposable fixture and reports that outcome.

Enforce test-session file policy at **all** editor Open/Save/Save As/pending-decision paths and the underlying document read/write boundary, not just at request parsing. Reads/writes of level documents are confined to owned fixture/output slots under the resolved session root; packaged resources remain read-only under the configured trusted resource root. Reject traversal, absolute outside paths, UNC/device paths, alternate data streams, symlink/junction/reparse escapes and non-owned overwrite targets before IO; validate resolved parents for new outputs. Restrict creation to bounded level-file slots and reject links in the owned root. Only operator setup adds fixtures. Deny game process creation at its boundary for this test profile, with a clear UI diagnostic and `policy_denied` result; no executable/path/command parameters are exposed. Normal editor behavior is unchanged. v1 does not certify saved-file Play.

The operator approved an automation-only save exception during implementation: validate and serialize first, then write through an exclusive, identity-checked destination handle (using exclusive creation for an absent reserved slot). Ownership is derived from that same handle. This prevents overwriting a foreign file inserted between policy validation and writing. A Windows atomic replacement cannot retain a deny-delete destination pin, so restricted temporary outputs do not promise atomic replacement: an I/O failure may leave a partly written owned file, and must report failure without marking the document saved. Ordinary editor saves retain their existing atomic replacement. A pinned guard keeps the owned root nonempty so it cannot become an in-place junction between checks and IO.

Physical GLFW keyboard/character/mouse/gamepad events must not control the automation session, including between batches; preserve window lifecycle events. Disable the separate physical navigation/cursor-capture path and any system cursor warp (`ConfigNavMoveSetMousePos`, backend `WantSetMousePos` handling), plus external clipboard effects. Use an in-memory clipboard. Test Engine may internally compute widget coordinates and feed ImGui IO; the protocol exposes no coordinate actions and never calls OS input injection. Suppress native cursor changes in the test profile. OS close cancels the session; it cannot leave queued commands executing invisibly.

Each operation and batch has a steady-clock deadline. The host continues reading cancellation/EOF while waiting for a batch. The editor checks cancellation at coroutine yields and frame boundaries; its IO worker wakes minimized waits. On failure, timeout or cancellation, stop later steps, release all synthetic keys/buttons/modifiers through the engine input path, drain the release frame if possible, and report the resulting state. No held-input primitives cross request boundaries; an active editing buffer may. Cleanup must not claim rollback: deactivation can commit according to the real widget, so include final revisions and partial effects.

Ordinary target/assertion errors leave a usable session only after verified cleanup; a fresh explicit request can recover using observation/Undo. An internal engine assertion/error, corrupt protocol or failed cleanup marks the session `faulted`; preserve the last snapshot as stale evidence and require close/new start. Disconnect triggers cancellation then child shutdown. If the UI/GPU cannot progress within the 2-second cleanup grace, the host terminates **only its owned child**, waits for exit and reports `process_terminated` with the last verified snapshot; no successful cleanup or final document state is fabricated. Parent death/pipe EOF also expires ownership. A subsequent start creates a new epoch and never replays old requests.

| Error code | Meaning / behavior |
|---|---|
| `invalid_request`, `limit_exceeded`, `version_mismatch` | Reject before execution for schema/limit/handshake errors. |
| `busy`, `session_closed`, `environment_not_authorized`, `environment_unavailable` | Session lifecycle/precondition failure. |
| `not_found`, `ambiguous_target`, `stale_ref`, `snapshot_expired`, `state_conflict` | No safe current target/snapshot or failed initial revision guard; do not guess. |
| `disabled`, `blocked_by_modal`, `ui_unavailable`, `unsupported` | Report current state, blocking scope or supported operations; never fallback. |
| `value_unavailable`, `assertion_failed` | Include source/field, expected versus observed, availability and snapshot. |
| `policy_denied` | Forbidden file/process effect prevented at the application boundary. |
| `timeout`, `cancelled`, `disconnected`, `engine_error`, `session_faulted` | Abort later steps, report partial execution and cleanup/termination evidence. |
| `request_id_conflict`, `result_expired` | Never execute a duplicate mutation to recover a lost response. |

The error envelope contains `code`, `message`, failed `step_index`/target if applicable, last completed index, `expected`/`observed`, snapshot/revision IDs, `effects: none|partial|unknown`, `cleanup: released|process_terminated|unverified`, and `session_state`. Domain failures set MCP `isError`; malformed method/schema errors use SDK protocol errors. Transport framing failures close the bounded connection. Keep a bounded request-ID/result cache: identical duplicate returns its running/completed status without re-execution; same ID/different payload fails. Expired IDs remain non-replayable within a session using a monotonic sequence/high-water mark. The child also retains at most 16 distinct session/request cancellation IDs until execution completion or channel closure. Repeated cancellation shares capacity; overflow closes the channel. This prevents priority cancellation arriving before its batch from being silently lost.

### 10. Validation and acceptance layers

- **Planning evidence:** official sources and local pinned code inspected; no editor automation implemented or GPU/desktop session launched. The dependency-only compile/link/lifecycle probe is recorded in decision 1 above.
- **Automated without GPU:** protocol/schema/framing and cancellation tests; identity/snapshot/limits tests; fixture/path-policy negative tests; real ImGui/Test Engine tests for every standard family, draft/commit separation, conditional scopes, compound fields, Unicode/newlines, failure and input cleanup. Keep existing deterministic editor tests and ordinary build/boundary checks. Test behavior, not exact helper names or source ordering.
- **MCP integration:** communicate as an actual stdio client with the host and dedicated real editor; send previously unregistered batches. Exercise initial setup, multiple requests, observe/edit/commit/inspect, Undo/Redo, temporary Save As/reopen and every documented failure class. Add one actual editor control using an existing family and adjacent metadata only, then discover/edit/verify it without changing host/executor. Retain commands, request/response transcripts, dependency fingerprints and outcomes; no images required.
- **GPU/lifetime:** because composition, callbacks and shutdown change, run normal and automation editor Vulkan validation with resize/minimize/restore, pending cancellation, partial construction, disconnect and final destruction. Hardware/desktop authorization remains a prerequisite. Zero validation errors does not establish visual correctness.
- **Manual/visual:** only separate prepared scenarios assigned to `ui_driver` in an authorized environment can establish appearance. They are not a gate for semantic functional success and are not silently claimed. `ui_test_runner` owns existing automated UI execution under repository routing.
- **Performance:** record representative observation sizes, frames and action/round-trip latency; no speculative optimization or new FPS benchmark gate. Confirm finite limits and absence of mandatory capture. Headless operation is an optional future deployment, not an acceptance prerequisite.

## Risks / Trade-offs

- **Version-coupled internals** -> full Test Engine pin, one read-only buffer/component compatibility area, compile/lifecycle probe and real-widget regression coverage. No automatic dependency update.
- **Incomplete submission and metadata drift** -> explicit coverage/availability, fresh-frame stamps, duplicate-key validation and per-family acceptance; never infer a complete DOM.
- **Per-request test reset corrupts drafts** -> a persistent yielding dispatcher and cross-request active-input tests; engine-fatal recovery starts a new session explicitly.
- **Compound/legacy widget identity** -> register actual child IDs, add stable owner scopes, and reject ambiguous/stale refs. Do not declare coverage until tested.
- **Path typing bypasses transport restrictions** -> enforce policies at document/process boundaries and test real dialogs plus negative path forms.
- **Cooperative abort cannot unblock a hung driver** -> parent watchdog, wakeable waits, bounded child termination and truthful stale/unknown evidence.
- **Restricted test profile differs from normal Play/viewport behavior** -> expose profile/capabilities, retain ordinary tests, exclude those claims from semantic acceptance.
- **SDK/worker lifecycle boundary** -> pin the official SDK and dependencies; use actual SDK clients to test initialization, schemas, cancellation, EOF and bounded child cleanup. SDK interoperability does not replace real editor acceptance.

## Everyday workflow continuation (2026-09-20)

Functional UI validation starts with the four semantic tools, including new features without a compiled regression. The main agent prepares expected state transitions; a runner executes a whole bounded scenario. An ordinary Python helper may manage the existing SDK session, IDs, discovery, slots, evidence and close; it is not a scenario language or a new executor. Known action/assertion sequences form one batch. Discovery is scoped/filtered/paginated, with selected application fields and no cross-session ref cache. Timeout/disconnect never automatically replay mutations.

Before a functional run, perform an incremental build of the affected targets outside MCP. Configure only for setup or changed build options. The build fingerprint validates host/child compatibility, not source freshness. A read-only doctor verifies exact pins and fixed build/resource manifests plus real SDK initialization/discovery without launching an editor; desktop/GPU and source freshness stay `not_checked`. An explicit authorized launch can separately establish native startup/teardown. A checked-in bounded fixture-ID manifest names only simple level filenames under the fixed packaged levels directory; it cannot select executables, arbitrary paths or user documents.

Keep three evidence levels: A no-window unit/protocol/client checks, B official SDK through production host and real editor, C a runner that actually sees and calls the four MCP tools. B never proves C. Missing tools, scenario failure or unsupported operations produce precise blocked/unsupported evidence without Windows fallback. New supported-family features need adjacent semantic metadata, independent applied-state verification and a reproducible regression. Visual acceptance and all existing custom viewport/docking/OS/game exclusions remain separate future coverage.

## Migration Plan

No document or public runtime API migration. Integrate in the order in `tasks.md`: dependency/license check, isolated build/lifetime, session/policy, metadata/inspection, dynamic adapters, MCP, acceptance and documentation. Keep the option OFF until explicitly enabled; rollback is removal/disabling of the automation targets and host configuration. Existing scenario tests need not be rewritten en masse. Update `docs/DEVELOPMENT.md`, `docs/ARCHITECTURE.md` and agent guidance during implementation, when the commands actually exist. Do not change current docs to imply this planned system already runs.

## Open Questions

- License eligibility for the actual user/legal entity must be recorded before integration/distribution; the design does not assume it.
- Exact family-specific child-ID/Tab-deactivation behavior, passive buffer sampling and callback isolation remain integration checks. If an adapter cannot meet its required contract, the task remains incomplete and needs a reviewed scope/design revision.
- Real editor/GPU lifecycle, MCP-client interoperability and timings have not been exercised by this planning task. Availability/results must be recorded during implementation, not inferred from the dependency probe.
