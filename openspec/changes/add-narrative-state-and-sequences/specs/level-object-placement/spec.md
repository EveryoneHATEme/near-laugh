## MODIFIED Requirements

### Requirement: Flat supported object set
The editor SHALL present one flat selectable set containing every axis-aligned solid and named entry, every authored point light, every static-prop placement with its authored boxes, every authored switch, every authored hinged door, the bounded audio sources, cues, rooms and connections, every character actor, scene mark and route, and every household box, readable document and radio control, and every narrative fact, region and event. Household collections SHALL support add, duplicate and remove within their declared bounds. Actors, marks and routes SHALL support add, duplicate and remove within their respective bounds. The editor SHALL allow solids and entries to be added, duplicated, and removed within their bounds and lights/switches to be added, duplicated and removed within their bounds. Doors SHALL support add, duplicate, and remove within their thirty-two-door bound. Props SHALL support add, duplicate and remove within the 128-placement bound using known catalog models. It SHALL NOT edit the packaged catalog, or introduce unsupported component or hierarchy types. The default entry SHALL be visibly identified.

#### Scenario: Level objects are listed
- **WHEN** a valid level document is active
- **THEN** the object list contains each supported object exactly once with its concrete game-specific type, entry, light, switch, prop, door, audio-record, actor, mark, route, fact, region or event identifier where applicable, and no parent-child hierarchy

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

### Requirement: Narrative records and region placement
Facts, regions and events SHALL support add, duplicate, remove and typed property editing within their bounds using the existing command history. Region bounds SHALL support list/viewport selection, numeric editing and placement on the nearest suitable upward structural or terrain surface, retaining extents and placing the lower face at the hit. Unsuitable nearer geometry SHALL block placement. Regions SHALL have distinguishable editor-only wire bounds; facts/events SHALL remain list records without invented world transforms. Canceling placement SHALL preserve document/history.

#### Scenario: Region is placed upstairs
- **WHEN** the nearest suitable placement hit lies on an upper floor
- **THEN** its lower face uses that hit height in one undoable edit with unchanged ID/extents

#### Scenario: Nearer wall blocks placement
- **WHEN** a wall lies before the intended floor
- **THEN** the region is not placed through it and cancellation creates no history entry

### Requirement: Narrative history and typed reference integrity
Each committed narrative edit SHALL be one undoable operation, preserving stable IDs across redo. Renaming a referenced fact, region, event, light, switch, door, actor, route, source, box, document or radio SHALL atomically update corresponding typed narrative references with existing incoming links. Deletion SHALL preserve unresolved references for repair, without cascade or substitution. Duplicate SHALL assign a fresh ID and preserve outgoing references; capacity failure SHALL commit nothing. Step edits/reordering SHALL preserve unrelated document data and the saved baseline while updating dirty state normally. Undo back to the saved document SHALL restore clean state; redo of the edit SHALL restore dirty state.

#### Scenario: Referenced source is renamed
- **WHEN** the author renames a source used by an event and then undoes it
- **THEN** source and all incoming event links change and restore in the same history entry

#### Scenario: Referenced region is deleted
- **WHEN** the author deletes a region used by triggers or conditions
- **THEN** events retain that missing ID visibly and Save/Play remain blocked until repair
