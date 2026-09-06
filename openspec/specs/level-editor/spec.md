# level-editor Specification

## Purpose

Defines the standalone game-specific level-editor workspace used to inspect persisted levels safely without coupling editor state or UI dependencies to the game runtime.

## Requirements

### Requirement: Standalone editor application
The project SHALL provide a desktop `level_editor` application that owns its window, editor input, editor camera, document state, rendering, and orderly shutdown independently from the `near_laugh` game application. Building or running the game SHALL NOT require editor UI code, editor document state, or editor-only resources, and neither application SHALL require the removed shooting-target texture.

#### Scenario: Editor starts
- **WHEN** the editor executable starts with valid current resources
- **THEN** it opens an editor workspace without constructing the grounded player, gameplay input mapper, player flashlight, or physics simulation

#### Scenario: Game target is inspected
- **WHEN** the shipping `near_laugh` target's transitive dependencies and public headers are inspected
- **THEN** they contain no editor UI dependency or editor document type

#### Scenario: Runtime resources are inspected
- **WHEN** the game and editor executable-relative resource layouts are inspected
- **THEN** neither layout contains or requires a shooting-target texture asset

### Requirement: Inspectable scene workspace
The editor SHALL render the currently open level in its main window and SHALL provide menus, a document summary, validation feedback, bounded object and entry editing, a playtest entry selector, and height-sample terrain brushes when terrain exists. Terrain origin, spacing, and dimensions SHALL remain read-only. With no terrain, the workspace SHALL identify that absence and disable terrain-only placement and sculpting. Structurally safe documents or edits that violate gameplay validation SHALL remain visible and editable while saving and playtesting are unavailable.

#### Scenario: Valid level is opened
- **WHEN** the user opens a supported valid level document
- **THEN** the workspace displays its scene from the editor camera and presents its bounded contents, entries, default entry, and valid status

#### Scenario: No document is open
- **WHEN** the editor starts without a level path or the current document is closed
- **THEN** the workspace remains responsive and clearly reports that no level is open without rendering stale scene data

#### Scenario: Edit violates gameplay validation
- **WHEN** a finite, structurally safe object or terrain edit causes entry overlap, unsupported terrain slope, or another gameplay validation failure
- **THEN** the workspace previews the edited scene, presents diagnostics, and permits correction or undo while saving and playtesting remain unavailable

#### Scenario: Interior has no terrain
- **WHEN** a terrain-free interior is created or opened
- **THEN** structural editing and entry selection remain available, terrain tools are disabled with an explanation, and no terrain preview or brush footprint appears

### Requirement: Editor camera and input ownership
The editor SHALL provide a free-fly perspective camera with mouse-look, three-axis movement, sprint acceleration, pitch limits, and framebuffer-aspect handling. Camera controls SHALL operate only while the scene view owns navigation input, and UI interaction SHALL suppress conflicting camera movement or mouse look.

#### Scenario: Scene navigation is active
- **WHEN** the scene view owns input and the user supplies navigation controls
- **THEN** the editor camera moves or looks independently of the level's player spawn and collision

#### Scenario: UI owns input
- **WHEN** a menu, text field, dialog, or other UI control captures keyboard or pointer input
- **THEN** the same input does not move or rotate the editor camera

### Requirement: Level document lifecycle
The editor SHALL support creating an interior, opening a level by explicit path, saving the current document, saving it to a new path, closing it, and reporting its resolved path. Opening SHALL install only a supported, structurally safe decoded candidate and SHALL retain gameplay-invalid candidates for repair with diagnostics. A newly opened unchanged document SHALL be clean, including a normalized older-format document; a new interior SHALL be dirty and have no path. A successful save SHALL clear dirty state, and a failed save SHALL retain both the document and its prior dirty state. Opening or creating a replacement SHALL clear document history and stale placement, terrain-gesture, and playtest-entry selections.

#### Scenario: Existing document is opened
- **WHEN** the user chooses a supported structurally safe level path
- **THEN** the editor replaces the current document only after successful decoding, marks the replacement clean, and presents any gameplay validation failures for repair

#### Scenario: Save-as succeeds
- **WHEN** the user saves the current document to a writable new path
- **THEN** deterministic level output is written, the current path becomes the new resolved path, and dirty state is cleared

#### Scenario: Save fails
- **WHEN** serialization or writing the selected path fails
- **THEN** the editor keeps the in-memory document and dirty state and presents an actionable error

#### Scenario: Gameplay-invalid interior is opened
- **WHEN** a structurally safe file contains an unsupported or obstructed entry
- **THEN** it opens as a clean editable document with entry-specific diagnostics, and saving and playtesting remain unavailable until it is repaired

### Requirement: Unsaved-change protection
The editor SHALL require an explicit discard or successful save decision before replacing a dirty document, including creating a new interior, closing it, or exiting. Canceling the decision SHALL leave the document and application state unchanged. Playtesting a dirty document SHALL instead offer Save and Play or Cancel and SHALL never implicitly discard edits or launch the older saved contents.

#### Scenario: Dirty document would be replaced
- **WHEN** the user opens another level or creates a new interior while the current document is dirty
- **THEN** the editor offers save, discard, and cancel choices before replacing it

#### Scenario: User cancels close
- **WHEN** the user cancels an exit or document-close request for a dirty document
- **THEN** the editor remains open with the same document, path, and dirty state

#### Scenario: User cancels dirty playtest
- **WHEN** the user chooses Cancel from the dirty playtest decision
- **THEN** no file is saved, no process is launched, and the document, path, selected entry, and dirty state remain unchanged

### Requirement: Actionable validation presentation
The editor SHALL preserve the current usable document when opening another document fails and SHALL present parse, validation, resource, save, and process-launch errors without terminating the editor. Structurally safe gameplay-invalid documents SHALL be admitted for repair under the document lifecycle requirement. Each reported level error SHALL include the path and field or object context supplied by level persistence when available. Resource errors SHALL include the model/material identity and affected placement where available. Unknown references SHALL remain editable and selectable with diagnostics. Failed asset decoding or GPU preview replacement SHALL retain the last usable preview resources and clearly identify any preview that is stale relative to the active document; such failure SHALL NOT erase the active document or be presented as successful replacement. Launch feedback SHALL distinguish process creation from successful gameplay initialization and report unsuccessful child exit with the launched path and entry.

#### Scenario: Invalid document is selected
- **WHEN** the user attempts to open a malformed, unsupported, or structurally unsafe level while another level is open
- **THEN** the existing document remains active and the workspace presents the rejected path and diagnostic reason

#### Scenario: Editor rendering is temporarily unavailable
- **WHEN** the editor window has zero framebuffer extent or presentation recovery is required
- **THEN** the editor retains its document and UI state and resumes rendering after a usable extent is restored

#### Scenario: Game process cannot start
- **WHEN** the game executable is absent or process creation fails
- **THEN** the editor reports the attempted executable, level, entry, and available failure reason while keeping the active document usable

#### Scenario: Packaged prop fails during editing
- **WHEN** a candidate preview references a missing, unsupported or corrupt packaged model or material
- **THEN** the editor remains usable, identifies that asset and placement, retains the editable values and last usable preview, and allows correction or undo without claiming the stale preview matches the document

### Requirement: Saved-document playtest transaction
The editor SHALL launch playtests in a separate game process using the sibling game executable, the active document's resolved absolute path, and the chosen entry identifier. The playtest entry SHALL initially follow the document default; changing the launch selection alone SHALL NOT modify authored data or dirty state. The editor SHALL finish active edits, validate the complete current document, resolve any required save decision, and verify the saved file, selected entry and required selected-scene assets before process creation. A document without a path SHALL require successful Save As. Failed validation, failed or canceled saving, an unknown entry, a missing or unsupported required asset, or a saved file that no longer matches the prepared authored contents SHALL prevent launch and remain diagnosable. A launch request SHALL be consumed once and SHALL NOT be replayed on a later UI frame or recovery. At most one editor-launched game process SHALL be active per editor instance; the editor SHALL remain responsive and allow authoring while it runs. Later document edits and saves SHALL NOT change the running level, and closing the editor SHALL NOT forcibly terminate an already launched game.

#### Scenario: Clean document is played
- **WHEN** the user plays a valid saved document with a valid selected entry and no active playtest
- **THEN** exactly one process starts with the explicit saved path and entry, without saving or altering the document default or the packaged prototype

#### Scenario: Dirty document is saved and played
- **WHEN** the user confirms Save and Play and the complete save and subsequent file validation succeed
- **THEN** the document becomes clean and one process starts from the newly saved file at the selected entry

#### Scenario: New document needs a path
- **WHEN** the user plays a valid new interior without a save path
- **THEN** launch waits for Save As, and canceling or failing Save As creates no process

#### Scenario: Save or validation fails
- **WHEN** the current level, any entry, or the attempted saved file fails validation, or the requested save fails
- **THEN** the editor reports the failure and launches no process, preserving the editable document and the dirty state resulting from any completed operation

#### Scenario: Saved file was changed externally
- **WHEN** preflight detects that the saved file differs semantically from the prepared document or no longer contains the selected entry
- **THEN** the editor refuses launch and requires an explicit save or reopen before a new attempt

#### Scenario: Playtest is already running
- **WHEN** a second play request occurs while the editor's game process remains active
- **THEN** no second process is created and the workspace identifies the active playtest

#### Scenario: File path contains spaces or non-ASCII characters
- **WHEN** the saved level path contains spaces, non-ASCII characters, or shell-significant characters valid in the host filesystem
- **THEN** process launch preserves the literal path as one argument and does not interpret any part of it as a shell command

#### Scenario: Selected model fails play preflight
- **WHEN** the saved level is semantically valid but a required model or material cannot be loaded under the configured package root
- **THEN** the editor reports the asset/placement context and creates no game process
