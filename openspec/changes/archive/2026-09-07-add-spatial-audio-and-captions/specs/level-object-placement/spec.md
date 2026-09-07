## MODIFIED Requirements

### Requirement: Flat supported object set
The editor SHALL present one flat selectable set containing every axis-aligned solid and named entry, exactly two point lights, every static-prop placement with its authored boxes, the optional singleton switch, every authored hinged door, and the bounded audio sources, cues, rooms and connections. The editor SHALL allow solids and entries to be added, duplicated, and removed within their bounds and the switch to be added when absent and removed when present. Doors SHALL support add, duplicate, and remove within their thirty-two-door bound. Props SHALL support add, duplicate and remove within the 128-placement bound using known catalog models. It SHALL NOT create or remove the fixed-count lights, edit the packaged catalog, or introduce unsupported component or hierarchy types. The default entry SHALL be visibly identified.

#### Scenario: Level objects are listed
- **WHEN** a valid level document is active
- **THEN** the object list contains each supported object exactly once with its concrete game-specific type, entry, prop, door or audio-record identifier where applicable, and no parent-child hierarchy

#### Scenario: User requests a new object
- **WHEN** the user adds a solid
- **THEN** the editor creates one axis-aligned solid using a supported solid kind and a known structural material independent of kind without offering arbitrary components or filesystem paths

#### Scenario: Switch is added to a level
- **WHEN** the user adds a light switch while the document has none
- **THEN** one switch is created, selected, previewed, and recorded as one undoable edit; another switch cannot be added while it is present

#### Scenario: Entry is added
- **WHEN** the user adds an entry below the entry limit
- **THEN** the editor creates and selects an entry with a unique durable identifier, refreshes validation, and records one undoable edit

#### Scenario: Model placement is added
- **WHEN** the author chooses a catalog model below the placement bound
- **THEN** one selected placement is created with a unique prop ID and copied editable model-default boxes in one history entry

#### Scenario: Audio records are listed
- **WHEN** a document contains audio authoring records
- **THEN** each source, cue, room and connection is selectable once in the flat list without creating an entity hierarchy

### Requirement: Object duplication and removal
Duplicating a solid SHALL create an independently selectable copy with the same authored values at a deterministic visible offset. Removing a selected solid SHALL remove only that solid. Solid operations SHALL respect the level's maximum solid count. Duplicating an entry SHALL preserve its pose, allocate a unique identifier, select the copy, and leave the default unchanged. Entry operations SHALL respect the sixteen-entry maximum. Removing the last entry or the current default entry SHALL be unavailable; the author SHALL be able to select another default before removing the old one. The optional switch SHALL support removal and undo/redo of that removal but SHALL NOT support duplication. Duplicating a door SHALL allocate a unique durable door identifier, preserve the configuration at a deterministic visible horizontal offset, select the copy, and refresh validation. Undo and redo SHALL restore the same allocated identifier. Removing a door SHALL remove only that definition and its preview; incoming audio connections SHALL retain their now-unresolved door IDs with actionable diagnostics until repaired or deletion is undone. Duplicating a prop SHALL preserve its model, scale, yaw and local boxes at a deterministic visible horizontal offset, allocate a unique prop identifier and select the copy; undo/redo SHALL restore that same allocated identifier. Removal SHALL delete only that placement and never its catalog asset or another placement sharing it. Only the two fixed lights SHALL prohibit duplication and removal.

#### Scenario: Solid is duplicated
- **WHEN** the user duplicates a selected solid below the maximum count
- **THEN** one offset copy is inserted, selected, previewed, and marks the document dirty

#### Scenario: Fixed-count object removal is requested
- **WHEN** the user requests deletion of either point light
- **THEN** the operation is unavailable and the document remains unchanged

#### Scenario: Switch is removed and restored
- **WHEN** the user removes the selected switch and then undoes the removal
- **THEN** the switch disappears and returns with its original authored fields and selection, and preview, validation, and dirty state follow the restored document

#### Scenario: Entry is duplicated and restored
- **WHEN** the user duplicates an entry and then undoes and redoes that edit
- **THEN** the copy disappears and returns with the same allocated identifier and pose, and the original default remains unchanged

#### Scenario: Default entry removal is requested
- **WHEN** the user requests removal of the default or only entry
- **THEN** removal is unavailable with an explanation and the document remains unchanged

#### Scenario: Door is duplicated and removed
- **WHEN** the user duplicates a door, removes the copy, and undoes and redoes those operations
- **THEN** door configuration, allocated identity, selection, preview, validation, and dirty state follow the corresponding document states without changing the original

#### Scenario: Shared model placement is deleted
- **WHEN** two chairs reference the same model and the author deletes one
- **THEN** only that placement is removed and one undo restores its identity, transform, boxes and selection while the other chair remains unchanged

## ADDED Requirements

### Requirement: Concrete audio authoring operations
Sources, cues, rooms and connections SHALL support bounded add, select, edit, duplicate and delete operations through the existing history. Sources SHALL expose cue selection, position, gain, distances and autoplay; cues SHALL expose catalog clip/caption selection and their concrete flags; rooms SHALL expose numeric bounds; connections SHALL expose endpoints, door and gains. Source markers and room wireframes SHALL be viewport-selectable with the same selection as their list entries. Connections and cue definitions SHALL be list-selectable. Selected links SHALL be inspectable without affecting runtime rendering or collision. Source surface placement SHALL use the nearest eligible surface and an explicit height/outward offset, retaining existing miss, cancellation, UI capture and unsuitable-nearer-surface guarantees.

#### Scenario: Source is placed and restored
- **WHEN** the author places a source on an upper floor using a displayed height offset and then undoes/redoes
- **THEN** its position, selection, validation and dirty state follow one history entry while cue identity and unrelated fields remain unchanged

#### Scenario: Room and connection are authored
- **WHEN** the author edits room bounds and chooses a door-linked connection with valid endpoints and gains
- **THEN** the current records and selected wireframes/links reflect each committed edit and share the existing bounded undo history

### Requirement: Audio reference integrity during object edits
Renaming a cue, source, room or door SHALL reject malformed/duplicate IDs and update any incoming audio references in the same undoable operation. Duplication SHALL allocate a new durable ID, preserve outgoing references and leave incoming references targeting the original. Deleting referenced cues, rooms or doors SHALL leave incoming IDs unresolved and diagnosable without cascading deletion or silent retargeting. Undo SHALL restore the original identities and reference validity. Unsafe individual field edits SHALL be refused; safe cross-record invalidity SHALL remain editable and block saving/Play until repaired.

#### Scenario: Linked door is renamed
- **WHEN** a door used by an audio connection is renamed to a valid unused ID
- **THEN** the door and connection reference change together and one undo restores both

#### Scenario: Referenced room is deleted
- **WHEN** a room is removed while a connection refers to it
- **THEN** the connection remains visible with a broken-room diagnostic and undo restores its original valid link

#### Scenario: Cue is duplicated
- **WHEN** a referenced cue is duplicated and the operation is undone and redone
- **THEN** the copy retains the same newly allocated identity and outgoing catalog references while existing sources continue referencing the original
