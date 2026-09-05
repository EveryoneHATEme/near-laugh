## MODIFIED Requirements

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
