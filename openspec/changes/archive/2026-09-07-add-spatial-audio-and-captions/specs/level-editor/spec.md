## ADDED Requirements

### Requirement: Explicit isolated sound audition
The editor SHALL provide explicit Start, Stop, mute and pause/resume controls for one selected source, using a validated snapshot, the editor camera listener and authored initial door states. Audition SHALL display the same caption content and preserve shared cue lifetime behavior without constructing gameplay physics or altering level data, history or dirty state. Opening, selecting or editing content SHALL NOT start playback automatically. Any committed edit, undo/redo, document replacement, minimization or Play request SHALL stop audition and clear its text; restoration SHALL NOT restart it. Device failure SHALL warn and retain the editable document and caption audition.

#### Scenario: Source is explicitly auditioned
- **WHEN** the user starts a valid selected source and moves the editor camera
- **THEN** playback and captions start once and spatial presentation follows the camera without changing authored values or dirty state

#### Scenario: Audition snapshot becomes stale
- **WHEN** the user edits, undoes, replaces the document, minimizes or requests Play during audition
- **THEN** the audition stops and its captions clear without a deferred restart after recovery

#### Scenario: Audition is suspended and resumed
- **WHEN** the user pauses and resumes an otherwise unchanged audition
- **THEN** its audio and captions continue from the same offset without creating a new instance

### Requirement: Audio diagnostics and play preflight
The editor SHALL present cue, source, room, door-link, clip, caption and font failures with identity and field/path context. Unknown references SHALL remain selectable and repairable. Malformed unsafe documents SHALL retain the previous active document. Saving SHALL require valid authored audio definitions; audition and saved-file Play SHALL additionally validate selected decoded clips, caption timing/coverage/layout and font assets. Failure SHALL start no invalid audition or game process and preserve editable state. Output-device availability SHALL NOT be a prerequisite for saving or creating a caption-capable playtest.

#### Scenario: Caption or clip is missing during Play
- **WHEN** the saved document is valid but a selected audio or caption/font resource is missing or invalid
- **THEN** preflight identifies the resource and affected cue and launches no game process

#### Scenario: Audio link is broken
- **WHEN** a source or connection references an absent cue, room or door
- **THEN** the workspace retains the record with a diagnostic and allows correction or undo while saving and Play are unavailable

#### Scenario: No device is present during Play
- **WHEN** all selected content validates but the machine has no output device
- **THEN** the editor can launch the game, whose device warning and captions provide the defined silent behavior
