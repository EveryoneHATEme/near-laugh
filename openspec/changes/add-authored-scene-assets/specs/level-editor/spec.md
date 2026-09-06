## MODIFIED Requirements

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
