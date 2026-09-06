## MODIFIED Requirements

### Requirement: Bounded versioned level document
The system SHALL read and write a human-readable version-5 level document describing a required nullable terrain field, one through 240 axis-aligned solids, named entries and a default entry as defined by interior-level-authoring, exactly two point lights plus one ambient intensity, exactly one packaged static prop with its box collision proxy, a required nullable light-switch field, and a required array of zero through 32 doors as defined by interactive-doors. Present terrain SHALL retain the existing 97-by-97 heightfield profile. A non-null switch SHALL contain only its position, yaw, linked point-light index (0 or 1), and boolean initial state. Each door SHALL contain only its durable identifier, hinge position, closed yaw, leaf width/height/thickness, signed opening angle, angular speed, lock side, and boolean initial open and locked states. The document SHALL retain the fixed current solid kinds and surface roles, contain no filesystem paths, and reject missing required data, unknown fields, unsupported versions, removed shooting-target values, and values outside the bounded profile. Exact version-2, version-3, and version-4 shapes SHALL remain readable and normalize with no doors. Versions 2 and 3 SHALL normalize their single spawn to one entry named `default`, selected as default; version 2 SHALL normalize without a switch. Older shapes SHALL NOT accept fields introduced by later versions. Versions 4 and 5 SHALL NOT accept the superseded single-spawn field. Version 1 SHALL remain unsupported without a parser, translator, or alias.

#### Scenario: Supported level document is read
- **WHEN** a document declares version 5 and supplies every required field within the bounded profile using only current solid kinds and surface roles
- **THEN** loading produces its optional terrain, solids, entries, default identifier, environment light, static prop, optional switch, and ordered door definitions without inventing missing required data

#### Scenario: Document shape is unsupported
- **WHEN** a document has an unsupported version, missing or unknown field, excessive object count, unsupported kind or surface role, embedded resource path, or malformed door array
- **THEN** loading rejects it before producing a runtime level with the affected field identified

#### Scenario: Legacy shooting-target value is read
- **WHEN** a test document contains `shooting_target` as a solid kind or surface role
- **THEN** loading rejects it rather than translating or accepting the legacy value

#### Scenario: Version-1 test level is read
- **WHEN** a document declares format version 1
- **THEN** loading rejects the version without attempting to migrate it

#### Scenario: Existing version-2 level is opened
- **WHEN** a valid version-2 document is opened
- **THEN** loading preserves its authored terrain, solids, spawn pose, lights, and prop, produces a current document with a `default` entry, no switch and no doors, and never rewrites the source

#### Scenario: Existing version-3 level is opened
- **WHEN** a valid version-3 document is opened
- **THEN** loading preserves its fields including switch absence or presence, maps its spawn to the `default` entry, and supplies no doors without rewriting the source

#### Scenario: Existing version-4 level is opened
- **WHEN** a valid version-4 document is opened
- **THEN** all authored values and entry ordering remain unchanged, doors are empty, and the source file remains unchanged

#### Scenario: Version-2 level is inspected in the editor
- **WHEN** a valid version-2 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 5, which older builds cannot read

#### Scenario: Version-3 level is inspected in the editor
- **WHEN** a valid version-3 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 5, which older builds cannot read

#### Scenario: Version-4 level is inspected in the editor
- **WHEN** a valid version-4 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 5, which older builds cannot read

#### Scenario: Switch field is malformed
- **WHEN** a version-3, version-4, or version-5 document omits the switch field, supplies a switch array, selects a nonexistent slot, or uses a non-boolean initial state
- **THEN** loading rejects it with a field-specific diagnostic

#### Scenario: Terrain is intentionally absent
- **WHEN** a version-4 or version-5 document supplies null terrain and otherwise valid interior data
- **THEN** loading does not synthesize a heightfield

#### Scenario: Older document contains new fields
- **WHEN** a version-2, version-3, or version-4 document includes a doors field
- **THEN** strict shape validation rejects that field instead of interpreting the document as version 5

### Requirement: Deterministic semantic round trip
Saving a valid current-format level SHALL emit version 5 with canonical field order, stable solid, entry, and door order, locale-independent numeric representation, and one trailing newline. Loading the emitted document SHALL reproduce the same authored values, including terrain absence or samples, entry identifiers and poses, default entry, switch absence or fields, and every door identifier and initial configuration. Saving the reproduced document without edits SHALL produce byte-identical output. Version-2, version-3, and version-4 inputs SHALL normalize before this round trip and SHALL be written as version 5 only on an explicit save.

#### Scenario: Valid level is saved twice
- **WHEN** a valid level with multiple doors is saved, loaded, and saved again without edits
- **THEN** both byte sequences are identical and preserve door identities, order, transforms, opening limits, and initial states

#### Scenario: Process locale differs
- **WHEN** the same valid level is saved under different process locales
- **THEN** field ordering, decimal syntax, and newline policy remain identical

#### Scenario: Version-2 document is explicitly saved
- **WHEN** the user saves a normalized version-2 document without adding a switch or doors
- **THEN** output uses version 5 with a null switch, empty doors, and the `default` entry while preserving original authored content

#### Scenario: Version-3 document is explicitly saved
- **WHEN** the user saves a normalized version-3 document
- **THEN** output uses version 5 with empty doors and preserves the switch and other authored values including the original spawn as `default`

#### Scenario: Version-4 document is explicitly saved
- **WHEN** the user saves a normalized version-4 document without other edits
- **THEN** output uses version 5 with empty doors and unchanged entries, default, terrain presence, lights, prop, and switch

### Requirement: Shared level validation and diagnostics
Loaded and editor-produced levels SHALL pass the same structural and gameplay validation before saving or runtime handoff. Validation SHALL reject non-finite values or derived bounds, non-positive dimensions, invalid present terrain or unsupported slopes, invalid entries or default references, unsupported entry foot positions, standing entry clearance overlapping blocking geometry including doors in their authored initial poses, invalid light bounds, invalid prop transforms or proxy extents, invalid solids, invalid switches or transformed bounds, and door definitions outside interactive-doors constraints. Each entry's height SHALL match walkable terrain or the upward top face of a structural solid at its horizontal position within numerical tolerance; validation SHALL NOT move the entry or substitute another floor. Props, switches, and doors SHALL NOT provide entry support. Terrain presence or footprint SHALL NOT define a world boundary for entries, props, switches, solids, or doors. Switch validation SHALL NOT require wall attachment or certify reachability or visibility. Door validation SHALL reject initial leaf penetration of terrain, structural collision, the prop proxy, another initial door leaf, or any standing entry; it SHALL NOT require the entire possible swing to be unobstructed. A failure SHALL identify the level path, failing field or object when available, and a concise reason; entry and door diagnostics SHALL identify a usable durable identifier or otherwise the array location.

#### Scenario: Parsed level violates gameplay constraints
- **WHEN** a parsed document places an entry inside blocking geometry or contains an unsupported terrain slope
- **THEN** validation rejects runtime handoff before physics or renderer construction and identifies the violated constraint

#### Scenario: Level syntax is malformed
- **WHEN** a required document cannot be parsed
- **THEN** loading fails with its resolved path and source location or field context when available

#### Scenario: Switch placement is invalid
- **WHEN** a switch has non-finite transformed bounds
- **THEN** validation reports its field and refuses saving or runtime handoff

#### Scenario: Entries share horizontal coordinates on different floors
- **WHEN** entries at the same horizontal coordinates have different clear supporting structural floors
- **THEN** each validates at its own authored height without snapping to the other floor or terrain

#### Scenario: Entry has no support or insufficient headroom
- **WHEN** an entry floats, overlaps a solid, prop proxy, terrain, or initially positioned door, or lacks standing headroom
- **THEN** validation identifies that entry and prevents saving and launch

#### Scenario: Interior extends beyond optional terrain
- **WHEN** a structurally supported entry and finite valid props, switches, and doors lie outside a present terrain footprint
- **THEN** terrain bounds alone do not invalidate them

#### Scenario: Door starts inside another object
- **WHEN** an initially open or closed leaf penetrates a structural solid, prop proxy, terrain, or another initial leaf
- **THEN** validation identifies the door and prevents saving and launch while a safely decoded editor document remains available for repair

#### Scenario: Later swing is obstructed
- **WHEN** the initial leaf placement is clear but its possible swing encounters a wall or another door
- **THEN** initial validation permits the placement and runtime obstruction behavior governs the attempted movement

### Requirement: Immutable runtime handoff
After startup validation and entry resolution succeed, the runtime SHALL expose one immutable authored level value to rendering and physics for the application lifetime and initialize the player at the selected entry. Selecting an entry SHALL NOT rewrite the default or reorder entries. The game SHALL NOT save, hot-reload, discover, or mutate level documents in its main loop. Switch interactions SHALL change separately owned light enable state; door interactions SHALL change separately owned door motion, lock, and feedback state. Authored light values, switch initial state, geometry, collision definitions, door identifiers, and door initial configurations SHALL remain unchanged.

#### Scenario: Runtime enters the frame loop
- **WHEN** the selected level and entry validate and dependent subsystems initialize
- **THEN** consumers retain the same immutable authored definitions while effective light state and door poses and locks can change independently

#### Scenario: Level loading fails
- **WHEN** the selected document is absent, unreadable, malformed, unsupported, or invalid, or its selected entry is unknown
- **THEN** startup constructs no consumer requiring the unresolved level or entry and releases every already-created owner exactly once

#### Scenario: Door actions are followed by restart
- **WHEN** the player changes door pose and lock state, exits, and starts again
- **THEN** the source file is unchanged and the new run restores authored initial door state
