## Purpose

Enable a local coding agent to inspect and exercise the real game editor through structured, dynamically supplied UI actions and observable assertions, without image-based targeting or feature-specific automation handlers.

## ADDED Requirements

### Requirement: Dedicated local test session

The automation interface SHALL provide explicit start, status, cancellation and close operations for one dedicated editor session owned by one local controller. The session SHALL retain its document, selection, UI state and unfinished input between requests until normal UI behavior changes them or the session ends. Startup SHALL use an authorized test environment and disposable fixture data, report its build, protocol and capabilities, and SHALL NOT attach to or take control of an existing user editor. Concurrent controllers or overlapping execution requests SHALL be rejected clearly.

#### Scenario: Separate requests continue an edit
- **WHEN** one request edits a field without committing and a later request observes or commits it in the same session
- **THEN** the original editor, document, active input and draft remain available without restarting the application or silently committing at a request boundary

#### Scenario: Another controller or batch competes
- **WHEN** an active session receives a competing start or a second execution while a batch is running
- **THEN** it reports `busy` without interleaving actions or changing ownership

#### Scenario: Environment is not authorized or available
- **WHEN** session start lacks authorization for the selected test environment or the necessary window/GPU facilities
- **THEN** startup reports the specific precondition failure and does not seize the user's desktop or fall back to another application

### Requirement: Passive structured observation

The interface SHALL expose submitted controls and relevant scopes with references, labels, known state, supported operations, typed values where available, commit semantics, source provenance and snapshot/revision identity. Observation SHALL distinguish unknown, unavailable, not-applicable and truncated data from known values. It SHALL report partial discovery, closed/collapsed content, unsubmitted content and pagination rather than claim a complete UI tree. Observation SHALL NOT focus, activate, scroll, expand, edit or commit UI as a side effect.

#### Scenario: Scope contains conditional content
- **WHEN** a client observes a scope containing a closed combo or collapsed section
- **THEN** it receives the available control and its state plus the discovery limitation, and can explicitly open it before requesting its newly submitted contents

#### Scenario: Observation during active input
- **WHEN** the client observes an active input field repeatedly
- **THEN** the response reports its available buffer and draft without changing focus, dirty state, history or commit status

#### Scenario: Passive sampling advances frames
- **WHEN** observation waits for a completed application frame
- **THEN** it reports that frame and per-source freshness without claiming that animation or application time was frozen, and does not use activating/copying Test Engine readers

#### Scenario: Incomplete numeric buffer has a retained typed draft
- **WHEN** an active numeric field contains incomplete text while its backing UI draft remains a known number
- **THEN** observation reports the text, invalid input syntax and the actual retained typed draft separately from the applied document

#### Scenario: Data exceeds observation limits
- **WHEN** an observation requires more items or text than its declared bounds
- **THEN** the response identifies pagination or truncation with snapshot-bound continuation, and expired continuation returns an explicit error rather than mixing frames

#### Scenario: Inactive text or label exceeds the byte budget
- **WHEN** an inactive input or full visible label exceeds the observation byte limit
- **THEN** the response identifies the UTF-8-safe prefix as truncated, and full-value assertions fail with `value_unavailable` and the actual observed availability

#### Scenario: Immediate application uses the same numeric widget family
- **WHEN** a numeric control immediately changes preview or placement state rather than a deferred document property
- **THEN** its adjacent metadata reports immediate application while keeping input termination gestures distinct from document application

### Requirement: Safe semantic references

Automation SHALL resolve controls using observed references or exact semantic selectors with explicit scope and owner identity. A surviving semantic control SHALL retain its reference across layout changes, label renaming and unrelated edits. Replacement, deletion/recreation or structural reuse of an indexed child SHALL invalidate affected references. The interface SHALL reject missing, ambiguous, expired and stale targets without selecting a similar label, a first match or another object occupying the old position.

#### Scenario: Owner is renamed or layout moves
- **WHEN** a control's owner survives a rename, resize or scroll
- **THEN** the previously issued semantic reference still identifies that same control independently of label text or coordinates

#### Scenario: Document or nested row is replaced
- **WHEN** a document is replaced or an indexed child is deleted/reordered and its old reference is used
- **THEN** the action reports `stale_ref` and cannot mutate the new document or replacement row

#### Scenario: Duplicate visible labels exist
- **WHEN** a selector matches multiple controls with the same visible label
- **THEN** execution reports `ambiguous_target` and exposes enough scope/identity information for an explicit selection

### Requirement: Runtime batches exercise real widgets

The interface SHALL accept a bounded ordered batch of actions and assertions at runtime without compiling or registering a scenario for that batch. Supported actions SHALL pass through the UI automation input engine and the editor's real widgets, preserving their normal editing, validation, commit and history behavior. The automation path SHALL NOT substitute document mutation, application-command invocation, a widget's application handler, or memory writes for an interaction. Each action SHALL be checked against the target's advertised operations and current state.

#### Scenario: Previously unseen batch is supplied
- **WHEN** the client submits a new sequence using supported actions and assertions
- **THEN** the existing executor runs it against the current editor without rebuilding the executor or adding a feature-specific handler

#### Scenario: Earlier action reveals a later target
- **WHEN** a batch opens a real menu or adds/selects an object and a later step uses an exact selector for newly submitted content
- **THEN** the later target resolves against the updated UI and the corresponding real widget receives the action

#### Scenario: Disabled or modal-blocked action is requested
- **WHEN** a target is disabled or blocked by a modal scope
- **THEN** execution identifies the condition and stops without invoking the target's application handler

### Requirement: Input draft and applied state remain distinct

Observation and assertions SHALL distinguish the active editing buffer, the UI's typed draft, applied document values and transient preview values. Controls SHALL publish supported commit methods and immediate-versus-deferred behavior. Successful text or value entry SHALL NOT by itself certify document application or validation. Read-only application inspection SHALL expose only explicit bounded projections, with snapshot provenance and no arbitrary member traversal, pointer access or mutating operations.

#### Scenario: Numeric input is incomplete
- **WHEN** a deferred numeric field contains incomplete text such as a minus sign
- **THEN** observation exposes that text separately from any known typed draft and the previous applied document value, without inventing a numeric value

#### Scenario: Multiline edit is committed
- **WHEN** newline-containing text is entered into a multiline control and later committed by an advertised deactivation method
- **THEN** newline input and commit are distinct interactions and the resulting applied text can be inspected independently

#### Scenario: Editor refuses a value
- **WHEN** a real commit gesture triggers an editor validation failure
- **THEN** the original applied value and the validation diagnostic remain inspectable, and the automation result does not claim that successful input applied the rejected value

#### Scenario: A test handler deliberately ignores input
- **WHEN** the real input gesture succeeds but an intentionally faulty test handler does not apply it to the document
- **THEN** an assertion expecting the submitted document value fails with the actual old value and stops later steps

#### Scenario: Preview value changes
- **WHEN** a supported preview control changes transient playback state
- **THEN** inspection reports that preview state separately from authored document values, dirty state and history

### Requirement: Generic standard-widget coverage

The first version SHALL support shared adapters for buttons/menu items, checkboxes, selectable lists and combo options, single-line and multiline text inputs, scalar and compound numeric inputs/drags, numeric sliders, the editor's component color editor, collapsible sections and the necessary window/menu/popup/child scopes. Read-only semantic text SHALL be available through explicit metadata where it cannot be discovered automatically. A new control of a supported family SHALL become discoverable, actionable and verifiable after the editor rebuild with at most ordinary adjacent metadata, without changing MCP tools or the generic executor and without registering a scenario.

#### Scenario: Newly added supported control passes acceptance
- **WHEN** a new control of an already supported family is added to the real editor with its metadata and the editor is rebuilt
- **THEN** a runtime-supplied batch discovers it, changes it through its widget and checks the observed UI and relevant applied state, with unchanged MCP/executor code and no image inspection

#### Scenario: Compound value is edited
- **WHEN** an agent addresses a published vector or color component
- **THEN** the real component widget receives input, other components remain governed by normal UI behavior, and any encoding or rounding is explicit in observation/assertion data

#### Scenario: Numeric value is dragged and released
- **WHEN** a runtime packet requests a supported numeric drag gesture
- **THEN** engine input holds the button, moves over bounded frames and releases through the real widget and its normal deactivation handler, without substituting text input or assigning a numeric field

### Requirement: Explicit execution assistance

Execution SHALL default to a strict policy without automatic scrolling or window focus recovery. Clients MAY explicitly permit these two conveniences; results SHALL identify attempted auxiliary actions and their before/after snapshots, including partial failure. Neither policy SHALL authorize hidden dismissal of an obstructing popup, ancestor expansion, window movement/resizing or document mutation. Requested menu transitions and ordinary widget handlers remain real UI behavior.

#### Scenario: Strict interaction targets clipped content
- **WHEN** a target needs scrolling and the packet did not permit automatic scrolling
- **THEN** execution fails before that assistance and identifies the unavailable target, allowing a subsequent explicit scroll or permitted retry

#### Scenario: Window key or scroll is obstructed by a popup
- **WHEN** a packet targets a different scope while a popup blocks it
- **THEN** the executor reports the obstruction and leaves the popup open instead of using focus recovery to dismiss it

#### Scenario: Permitted assistance precedes a failed gesture
- **WHEN** permitted scrolling or focus recovery executes and the step later fails or is cancelled
- **THEN** its result retains the auxiliary action evidence and does not claim no UI effects

### Requirement: Honest coverage and visual boundary

Functional automation SHALL require no screenshot, OCR, vision model, model-selected coordinate or system mouse/keyboard injection. Rendering can remain active. Unsupported actions SHALL return `unsupported` with the target's available capabilities and no hidden fallback. Version 1 SHALL explicitly exclude custom viewport picking/placement/sculpting/navigation, gizmo gestures, docking rearrangement, OS dialogs and game-process control; standard widgets configuring those features remain subject to their published coverage and session policy. Unimplemented mandatory widget families SHALL remain incomplete rather than being counted as supported. Structured functional success SHALL NOT establish visual rendering correctness.

#### Scenario: Custom viewport action is requested
- **WHEN** a client requests scene picking, placement, a brush stroke or gizmo manipulation outside published coverage
- **THEN** the result is `unsupported`, with no OS input, guessed pixel target or direct scene mutation

#### Scenario: Functional assertion passes while rendering is unreviewed
- **WHEN** structured UI and document assertions succeed without visual checks
- **THEN** the evidence reports functional success and leaves appearance/readability/rendering acceptance unverified

### Requirement: Bounded execution and truthful failure

The interface SHALL validate request shape and static bounds before execution, apply finite operation/batch/session deadlines, and support cancellation while requests are in flight. The first failed step SHALL stop later steps. Results SHALL report step status, expected/observed data where applicable, revisions, completed prefix, possible partial effects and cleanup state. Assertions SHALL use a finite set of typed predicates over observable values; unavailable values SHALL NOT pass as default values. Earlier UI effects SHALL NOT be implicitly rolled back or replayed, and duplicate request IDs SHALL NOT repeat mutation.

#### Scenario: Assertion fails after an edit
- **WHEN** a batch applies a real edit, then fails an assertion before a later Save step
- **THEN** the edit remains as the real UI produced it, Save is not executed, and the response identifies the failed assertion and completed prefix

#### Scenario: Unknown value is compared
- **WHEN** an assertion attempts equality or a negative comparison against an unknown or truncated full value
- **THEN** it reports `value_unavailable` rather than treating missing data as success

#### Scenario: Invalid or excessive batch arrives
- **WHEN** a request has an unknown operation, invalid static schema, excessive size or non-finite typed number
- **THEN** the complete request is rejected before any step executes

#### Scenario: Result response is retried
- **WHEN** a completed or running request ID is submitted again
- **THEN** it returns its existing status/result or an explicit conflict/expired-result error without re-executing the batch

#### Scenario: Child fails before execution evidence arrives
- **WHEN** child death, transport loss or stalled cleanup prevents verifying which submitted steps executed
- **THEN** the result retains any verified prefix, includes every requested step, reports unverified steps as `unknown` and effects as unknown, and uses `not_run` only for steps known not to have executed without inventing final state

### Requirement: Cancellation and input cleanup

Failure, cancellation, timeout, controller loss and shutdown SHALL stop further actions and release synthetic keys, buttons and modifiers. A normal request boundary SHALL NOT retain held synthetic input. Cleanup SHALL report real resulting state rather than promise rollback of drafts or commits. If progress or cleanup cannot be established within its deadline, the session SHALL become faulted and the owner SHALL terminate only its dedicated child when necessary, identifying stale or unavailable final state. Recovery SHALL require a fresh explicit request after verified cleanup or a new session after a fatal fault; no old command SHALL be replayed.

#### Scenario: Cancel interrupts an active gesture
- **WHEN** cancellation arrives while a supported interaction is running
- **THEN** later steps do not run, held input is released within the cleanup bound, and the response identifies completed and uncertain effects

#### Scenario: Minimized or stalled editor cannot make progress
- **WHEN** the editor is minimized or stops making frame progress during a request
- **THEN** cancellation and wall-clock deadlines remain effective, and the owner reports UI unavailability or terminates its stalled child without claiming a successful final snapshot

#### Scenario: Controller disconnects
- **WHEN** the owning local connection closes or its process dies
- **THEN** ownership expires, queued work is canceled, the disposable editor is cleaned up or boundedly terminated, and a subsequent session starts with a new identity

#### Scenario: Cancellation precedes batch delivery
- **WHEN** priority cancellation reaches the editor before its matching session/request execution packet
- **THEN** cancellation is retained within bounded storage and prevents that packet from mutating the UI, rather than being silently forgotten

#### Scenario: Startup request is cancelled
- **WHEN** the MCP client cancels a startup request that has acquired an owned editor
- **THEN** that startup is cleaned up within the session shutdown bound even if readiness occurs before the cancelled response would have been delivered

#### Scenario: Recoverable target error is corrected
- **WHEN** a target error finishes with verified input cleanup and the client observes then supplies a corrected request
- **THEN** the same surviving document remains usable and the failed request is not resumed implicitly

### Requirement: Confined effects and physical input isolation

The automation interface SHALL expose no eval, arbitrary program launch, shell command, address write or unrestricted file operation. Test sessions SHALL restrict level reads/writes to owned temporary fixtures/output slots and resources to the configured trusted read-only package. These restrictions SHALL apply at application file/process boundaries even when requests type paths into real widgets. Path escapes and forbidden process effects SHALL be rejected without damaging active state or user files. Physical input SHALL NOT compete with synthetic session input or activate camera capture; native close/window events SHALL remain serviceable. The host clipboard and system cursor SHALL NOT be used as automation channels.

#### Scenario: Real Save As targets an outside file
- **WHEN** a batch types an outside/traversing/link-escaping path into the real Save As dialog and activates Save
- **THEN** the file policy blocks writing, reports the denial, preserves the document's failure semantics and leaves the outside file unchanged

#### Scenario: Temporary save and reopen succeed
- **WHEN** real Save As and Open controls operate on an owned temporary file slot
- **THEN** normal document validation, serialization, dirty-state transitions and reloaded values are observable without modifying the source fixture

#### Scenario: A foreign file competes for an output slot
- **WHEN** another process inserts or replaces a destination after the automation policy checks its path
- **THEN** an exclusive creation or identity-checked write handle rejects the foreign destination without overwriting it, and ownership is derived from the retained handle

#### Scenario: Restricted temporary writing fails
- **WHEN** serialization succeeds but writing or flushing the exclusive automation output handle fails
- **THEN** the save reports failure and retains unsaved document state; the owned temporary output may contain partial bytes, while ordinary editor saves continue using atomic replacement

#### Scenario: Process launch is requested in the test profile
- **WHEN** a real Play control attempts to launch a game process in the restricted session
- **THEN** the policy prevents process creation with explicit feedback, and the result does not certify normal saved-file Play

#### Scenario: Physical navigation input arrives between requests
- **WHEN** the user moves or presses physical mouse/keyboard controls while the dedicated automation session is idle or executing
- **THEN** it does not move the editor camera, capture/warp the system cursor or alter the automated document, while window close still cancels the session
