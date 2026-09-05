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
The editor SHALL render the currently open level in its main window and SHALL provide menus, a document summary, validation feedback, bounded object selection and property editing, and height-sample terrain brushes. Terrain origin, spacing, and dimensions SHALL remain read-only. Structurally safe edits that violate gameplay validation SHALL remain visible and editable while saving is unavailable.

#### Scenario: Valid level is opened
- **WHEN** the user opens a supported valid level document
- **THEN** the workspace displays its scene from the editor camera and presents its bounded contents and valid status

#### Scenario: No document is open
- **WHEN** the editor starts without a level path or the current document is closed
- **THEN** the workspace remains responsive and clearly reports that no level is open without rendering stale scene data

#### Scenario: Edit violates gameplay validation
- **WHEN** a finite, structurally safe object or terrain edit causes a spawn overlap, unsupported terrain slope, or another gameplay validation failure
- **THEN** the workspace previews the edited scene, presents the diagnostics, and permits correction or undo while saving remains unavailable

### Requirement: Editor camera and input ownership
The editor SHALL provide a free-fly perspective camera with mouse-look, three-axis movement, sprint acceleration, pitch limits, and framebuffer-aspect handling. Camera controls SHALL operate only while the scene view owns navigation input, and UI interaction SHALL suppress conflicting camera movement or mouse look.

#### Scenario: Scene navigation is active
- **WHEN** the scene view owns input and the user supplies navigation controls
- **THEN** the editor camera moves or looks independently of the level's player spawn and collision

#### Scenario: UI owns input
- **WHEN** a menu, text field, dialog, or other UI control captures keyboard or pointer input
- **THEN** the same input does not move or rotate the editor camera

### Requirement: Level document lifecycle
The editor SHALL support opening a level by explicit path, saving the current document, saving it to a new path, closing it, and reporting its resolved path. A newly opened unchanged document SHALL be clean; a successful save SHALL clear dirty state; and a failed save SHALL retain both the document and its prior dirty state.

#### Scenario: Existing document is opened
- **WHEN** the user chooses a valid level path
- **THEN** the editor replaces the current document only after the selected level loads and validates successfully and marks the replacement clean

#### Scenario: Save-as succeeds
- **WHEN** the user saves the current document to a writable new path
- **THEN** deterministic level output is written, the current path becomes the new resolved path, and dirty state is cleared

#### Scenario: Save fails
- **WHEN** serialization or writing the selected path fails
- **THEN** the editor keeps the in-memory document and dirty state and presents an actionable error

### Requirement: Unsaved-change protection
The editor SHALL require an explicit discard or successful save decision before replacing a dirty document, closing it, or exiting. Canceling the decision SHALL leave the document and application state unchanged.

#### Scenario: Dirty document would be replaced
- **WHEN** the user opens another level while the current document is dirty
- **THEN** the editor offers save, discard, and cancel choices before replacing it

#### Scenario: User cancels close
- **WHEN** the user cancels an exit or document-close request for a dirty document
- **THEN** the editor remains open with the same document, path, and dirty state

### Requirement: Actionable validation presentation
The editor SHALL preserve the current usable document when opening another document fails and SHALL present parse, validation, resource, and save errors without terminating the editor. Each reported level error SHALL include the path and field or object context supplied by level persistence when available.

#### Scenario: Invalid document is selected
- **WHEN** the user attempts to open a malformed or invalid level while another valid level is open
- **THEN** the existing document remains active and the workspace presents the rejected path and diagnostic reason

#### Scenario: Editor rendering is temporarily unavailable
- **WHEN** the editor window has zero framebuffer extent or presentation recovery is required
- **THEN** the editor retains its document and UI state and resumes rendering after a usable extent is restored
