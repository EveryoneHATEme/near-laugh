## MODIFIED Requirements

### Requirement: Flat supported object set
The editor SHALL present one flat selectable set containing every axis-aligned solid and named entry, every authored point light, every static-prop placement with its authored boxes, every authored switch, every authored hinged door, the bounded audio sources, cues, rooms and connections, and every character actor, scene mark and route. Actors, marks and routes SHALL support add, duplicate and remove within their respective bounds. The editor SHALL allow solids and entries to be added, duplicated, and removed within their bounds and lights/switches to be added, duplicated and removed within their bounds. Doors SHALL support add, duplicate, and remove within their thirty-two-door bound. Props SHALL support add, duplicate and remove within the 128-placement bound using known catalog models. It SHALL NOT edit the packaged catalog, or introduce unsupported component or hierarchy types. The default entry SHALL be visibly identified.

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


## ADDED Requirements

### Requirement: Concrete actor and route editing
The editor SHALL edit the scripted-characters fields through concrete actor, mark and route properties using catalog choices and durable references. Actor placement SHALL visibly operate on its initial mark; shared-mark consumers SHALL be identified before editing that mark, and all uses SHALL update coherently. Numeric edits SHALL follow existing finite/safe versus repairable-invalid behavior. Adding an actor SHALL create a new initial mark in the same command if needed. Duplicating an actor SHALL create a new actor and independent initial mark, copy model/speed, and clear initial route/audio links to avoid implicitly driving another actor's route or sharing reserved sources. Route duplicates SHALL preserve owner, ordered marks and final clip with a new route ID. Capacity failures SHALL commit no partial records. Each compound action SHALL occupy one entry of the existing 128-entry history.

#### Scenario: Actor is duplicated
- **WHEN** capacity exists for both the actor and a new initial mark
- **THEN** one undoable command creates independent identities/placement, clears actor-owned route/source links and selects the new actor without altering the original

#### Scenario: Compound creation exceeds a bound
- **WHEN** adding or duplicating an actor needs another mark but the mark array is full
- **THEN** neither record is added and the document/history/selection remain unchanged with a capacity diagnostic

#### Scenario: Shared mark is edited
- **WHEN** the author changes a mark referenced by several routes or an initial actor placement
- **THEN** the properties identify those uses, all consumers reflect the same authored mark and one undo restores them together

### Requirement: Character reference integrity and repair
Renaming an actor, mark, route or audio source SHALL atomically update every incoming character reference in the same undoable command. Deletion SHALL retain incoming broken links for explicit repair without cascading deletion or selecting a substitute. Actor and route property controls SHALL display unknown references instead of silently selecting their first valid option. Reordering route marks SHALL preserve their durable identities and produce one undoable action. Undo/redo SHALL restore definitions, references, selection and saved-state identity coherently.

#### Scenario: Referenced actor or source is renamed
- **WHEN** its durable ID changes
- **THEN** route ownership or actor sound links change in the same history entry and undo restores both names and links

#### Scenario: Route mark is deleted
- **WHEN** a mark still used by a route or actor is removed
- **THEN** consumers keep the missing ID visibly, Save/Play are blocked and restoring or relinking the mark repairs the document

### Requirement: Character selection and surface placement
Viewport selection SHALL use conservative catalog actor bounds and distinct selectable mark/route representations consistent with the list selection. Missing actor models/marks SHALL retain selectable diagnostic representations whenever their finite position can be determined; otherwise the list SHALL retain access. Route lines SHALL show ordered links and direction with missing endpoints labeled. Actor feet/marks SHALL place only on the nearest suitable upward structural face or supported terrain using the existing placement capture/cancel policy; walls and undersides SHALL block placement instead of being searched through. Overlays SHALL distinguish visible bounds, capsule proxy, facing and route links. Placement SHALL edit the mark once, with shared uses visible, and require no mesh-derived collision or physics world.

#### Scenario: Actor is placed on an upper floor
- **WHEN** its mark is placed on the nearest suitable structural floor above another floor
- **THEN** feet use that exact hit elevation, the linked actor preview moves coherently and shared validation reports any clearance conflict

#### Scenario: Nearer surface is unsuitable
- **WHEN** a wall or underside is the first scene hit
- **THEN** no actor/mark moves to a floor behind it and cancel leaves no history entry

#### Scenario: Selected route loses one endpoint
- **WHEN** its referenced mark no longer exists
- **THEN** the route remains selectable in the list, resolved segments and missing-link diagnostics remain inspectable, and no bogus line is drawn to an invented origin
