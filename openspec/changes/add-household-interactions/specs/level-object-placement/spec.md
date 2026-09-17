## MODIFIED Requirements

### Requirement: Flat supported object set
The editor SHALL present one flat selectable set containing every axis-aligned solid and named entry, every authored point light, every static-prop placement with its authored boxes, every authored switch, every authored hinged door, the bounded audio sources, cues, rooms and connections, every character actor, scene mark and route, and every household box, readable document and radio control. Household collections SHALL support add, duplicate and remove within their declared bounds. Actors, marks and routes SHALL support add, duplicate and remove within their respective bounds. The editor SHALL allow solids and entries to be added, duplicated, and removed within their bounds and lights/switches to be added, duplicated and removed within their bounds. Doors SHALL support add, duplicate, and remove within their thirty-two-door bound. Props SHALL support add, duplicate and remove within the 128-placement bound using known catalog models. It SHALL NOT edit the packaged catalog, or introduce unsupported component or hierarchy types. The default entry SHALL be visibly identified.

#### Scenario: Level objects are listed
- **WHEN** a valid level document is active
- **THEN** the object list contains each supported object exactly once with its concrete game-specific type, entry, light, switch, prop, door, audio-record, actor, mark or route identifier where applicable, and no parent-child hierarchy

#### Scenario: User requests a new object
- **WHEN** the user adds a solid
- **THEN** the editor creates one axis-aligned solid using a supported solid kind and a known structural material independent of kind without offering arbitrary components or filesystem paths

#### Scenario: Switch is added to a level
- **WHEN** the user adds a light switch below the sixteen-switch bound
- **THEN** one switch is created, selected, previewed, and recorded as one undoable edit; additional switches can be added up to the bound, and an absent light link remains diagnosable

#### Scenario: Entry is added
- **WHEN** the user adds an entry below the entry limit
- **THEN** the editor creates and selects an entry with a unique durable identifier, refreshes validation, and records one undoable edit

#### Scenario: Model placement is added
- **WHEN** the author chooses a catalog model below the placement bound
- **THEN** one selected placement is created with a unique prop ID and copied editable model-default boxes in one history entry

#### Scenario: Audio records are listed
- **WHEN** a document contains audio authoring records
- **THEN** each source, cue, room and connection is selectable once in the flat list without creating an entity hierarchy

#### Scenario: Character records are listed
- **WHEN** the active document contains actors, shared marks and routes
- **THEN** each record has one selectable flat list entry with its own durable ID and no invented transform hierarchy

#### Scenario: Household objects are listed
- **WHEN** household records are present
- **THEN** each has one selectable typed list entry with its own durable identity and no implied runtime entity hierarchy


## ADDED Requirements

### Requirement: Concrete household authoring operations
Household boxes and documents SHALL support list/viewport selection, finite initial position/yaw editing, add/duplicate/remove and surface placement through the existing command history. Boxes SHALL show their fixed dimensions/mass without arbitrary physics/material editing; viewport selection SHALL use their initial oriented cube. Documents SHALL use their finite panel bounds. Radio controls SHALL be listed independently and selectable through the referenced static radio bounds, with selection distinguishing the control from its prop. A broken radio link SHALL remain list-selectable without invented world coordinates. Surface placement SHALL use the nearest suitable upward structural face or actual terrain triangle, resting a box's bottom or a document panel just above the hit; unsuitable nearer faces SHALL block placement. Household definitions SHALL never run physics in the editor, and unrelated edits SHALL preserve them.

#### Scenario: Box is placed on an upper floor
- **WHEN** the author positions a selected box on an upward upper-floor face
- **THEN** its initial center places its bottom at that face in one undoable command without changing its ID or starting simulation

#### Scenario: A wall is the nearer surface
- **WHEN** the placement ray first meets an unsuitable wall before a farther floor
- **THEN** the editor reports unavailable placement and does not place through that wall or add history

#### Scenario: Radio control and static prop share bounds
- **WHEN** the author switches selection between the radio control and its referenced prop
- **THEN** the list/properties identify the intended record and the control's links remain distinct from prop geometry fields

### Requirement: Household history and reference integrity
Each committed household add, duplicate, remove, property edit or placement SHALL use one existing bounded undo/redo history entry and preserve allocated identities across redo. Renaming a referenced prop or audio source SHALL rewrite incoming radio links atomically with that rename. Deletion SHALL retain incoming unresolved links with actionable diagnostics, and undo SHALL restore the old identity and links. A duplicated record SHALL receive a fresh ID and preserve outgoing references; any resulting exclusive radio ownership conflict SHALL be visible and repairable rather than silently sharing runtime ownership. Compound operations that exceed a bound SHALL commit nothing. Text/page edits SHALL participate in dirty-state and history semantics without one history entry per typed character.

#### Scenario: Radio source is renamed and restored
- **WHEN** an author renames a source used by a radio and undoes that edit
- **THEN** source identity and every incoming radio reference change and return together in one history step

#### Scenario: Referenced radio prop is deleted
- **WHEN** a radio's static prop is removed
- **THEN** the radio retains its unresolved prop ID, saving/play is refused with a diagnostic, and undo restores the original relationship

#### Scenario: Box capacity is exhausted
- **WHEN** a seventeenth box is added or duplicated
- **THEN** the operation leaves definitions, selection, history and dirty state unchanged and identifies the count limit
