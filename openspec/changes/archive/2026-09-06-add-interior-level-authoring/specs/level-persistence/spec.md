## MODIFIED Requirements

### Requirement: Bounded versioned level document
The system SHALL read and write a human-readable version-4 level document describing a required nullable terrain field, one through 240 axis-aligned solids, named entries and a default entry as defined by interior-level-authoring, exactly two point lights plus one ambient intensity, exactly one packaged static prop with its box collision proxy, and a required nullable light-switch field. Present terrain SHALL retain the existing 97-by-97 heightfield profile. A non-null switch SHALL contain only its position, yaw, linked point-light index (0 or 1), and boolean initial state. The document SHALL retain the fixed current solid kinds and surface roles, contain no filesystem paths, and reject missing required data, unknown fields, unsupported versions, removed shooting-target values, and values outside the bounded profile. Exact version-2 and version-3 shapes SHALL remain readable and normalize their single spawn to one entry named `default`, selected as default; version 2 SHALL normalize without a switch. Older shapes SHALL NOT accept version-4 fields, and version 2 SHALL NOT accept version-3 fields. Version 4 SHALL NOT accept the superseded single-spawn field. Version 1 SHALL remain unsupported without a parser, translator, or alias.

#### Scenario: Supported level document is read
- **WHEN** a document declares version 4 and supplies every required field within the bounded profile using only current solid kinds and surface roles
- **THEN** loading produces the corresponding optional terrain, solids, entries, default identifier, environment light, static-prop placement, and explicit switch presence or absence without inventing missing required data

#### Scenario: Document shape is unsupported
- **WHEN** a document has an unsupported version, missing field, unknown field, excessive solid count, unsupported or removed kind or surface role, or embedded resource path
- **THEN** loading rejects the document before producing a runtime level

#### Scenario: Legacy shooting-target value is read
- **WHEN** a test level document contains `shooting_target` as a solid kind or surface role
- **THEN** loading rejects the document as unsupported rather than translating or accepting the legacy value

#### Scenario: Version-1 test level is read
- **WHEN** a test level document declares format version 1
- **THEN** loading rejects the version without attempting to migrate or translate the document

#### Scenario: Existing version-2 level is opened
- **WHEN** a valid document uses the previous version-2 profile
- **THEN** loading preserves its authored terrain, solids, spawn pose, lights, and prop, produces a current-format document with a `default` entry and no switch, and never rewrites the source file

#### Scenario: Version-2 level is inspected in the editor
- **WHEN** the user opens a valid version-2 level in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 4, which older builds cannot read

#### Scenario: Switch field is malformed
- **WHEN** a version-3 or version-4 document omits the switch field, supplies a switch array, references a nonexistent light slot, or uses a non-boolean initial state
- **THEN** loading rejects the document with a field-specific diagnostic

#### Scenario: Existing version-3 level is opened
- **WHEN** a valid document uses the previous version-3 profile
- **THEN** loading preserves its authored fields including switch absence or presence and maps its single spawn pose to the `default` entry without rewriting the file

#### Scenario: Version-3 level is inspected in the editor
- **WHEN** the user opens a valid version-3 level in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 4, which older builds cannot read

#### Scenario: Terrain is intentionally absent
- **WHEN** a version-4 document supplies null terrain and otherwise valid interior data
- **THEN** the document contains no heightfield and loading does not synthesize one

### Requirement: Deterministic semantic round trip
Saving a valid current-format level SHALL emit version 4 with canonical field order, stable solid and entry order, locale-independent numeric representation, and one trailing newline. Loading the emitted document SHALL reproduce the same level values, including terrain absence or samples, entry identifiers and poses, default entry, and switch absence or all authored switch fields. Saving the reproduced document without edits SHALL produce byte-identical output. Version-2 and version-3 inputs SHALL normalize before this round trip and SHALL be written as version 4 only on an explicit save.

#### Scenario: Valid level is saved twice
- **WHEN** a valid in-memory level is saved, loaded from that result, and saved again without edits
- **THEN** the two saved byte sequences are identical and describe semantically equal level data

#### Scenario: Process locale differs
- **WHEN** the same valid level is saved under different process locales
- **THEN** each output uses the same field ordering, decimal syntax, and newline policy

#### Scenario: Version-2 document is explicitly saved
- **WHEN** the user explicitly saves a document loaded from version 2 without adding a switch or changing its other authored fields
- **THEN** output uses version 4 with a null switch and the `default` entry while preserving the original authored content, and a subsequent load/save is byte-identical

#### Scenario: Version-3 document is explicitly saved
- **WHEN** the user explicitly saves a document loaded from version 3
- **THEN** output uses version 4, preserves the switch and all other authored values, and records the original spawn pose as the `default` entry

### Requirement: Shared level validation and diagnostics
Loaded and editor-produced levels SHALL pass the same structural and gameplay validation before they can be saved or handed to runtime consumers. Validation SHALL reject non-finite values or derived bounds, non-positive dimensions, invalid present terrain or unsupported terrain slopes, invalid entries or default references, unsupported entry foot positions, standing entry clearance that overlaps blocking geometry, invalid light bounds, invalid prop transforms or proxy extents, invalid solid data, and invalid switch fields or transformed bounds. Each entry's authored height SHALL match a walkable terrain surface or the upward top face of a structural solid at its horizontal position within numerical tolerance; validation SHALL NOT silently move the entry to a different height or substitute another floor. Prop proxies and switches SHALL NOT provide entry support. Terrain presence or its footprint SHALL NOT define a world boundary for entries, props, switches, or solids. A switch SHALL have finite position and yaw and a valid point-light index; validation SHALL NOT require wall attachment or certify that it is reachable or unoccluded. A parse or validation failure SHALL identify the level path, the failing field or object when available, and a concise reason. Entry diagnostics SHALL distinguish the affected entry by identifier when usable and by array location otherwise.

#### Scenario: Parsed level violates gameplay constraints
- **WHEN** a syntactically valid document places an entry inside a blocking solid or contains an unsupported terrain slope
- **THEN** validation rejects runtime handoff before renderer or physics initialization and identifies the violated constraint

#### Scenario: Level syntax is malformed
- **WHEN** a required level document cannot be parsed
- **THEN** loading fails with an actionable diagnostic containing the resolved path and source location or field context when available

#### Scenario: Switch placement is invalid
- **WHEN** a switch placement has non-finite transformed bounds
- **THEN** shared validation reports the switch field and refuses saving or runtime construction

#### Scenario: Entries share horizontal coordinates on different floors
- **WHEN** two entries share horizontal coordinates but each has an authored height on a different clear structural floor
- **THEN** both validate against their own floor without either being snapped to terrain or to the other floor

#### Scenario: Entry has no support or insufficient headroom
- **WHEN** an entry floats above a floor, is placed inside a wall or chair proxy, lies beneath intersecting terrain, or lacks standing clearance beneath a ceiling
- **THEN** validation reports that entry's support or clearance failure and prevents saving and launch

#### Scenario: Interior extends beyond optional terrain
- **WHEN** an entry is supported by a solid outside a present heightfield's footprint and the prop and switch also have finite valid placements outside that footprint
- **THEN** terrain bounds alone do not invalidate those placements

### Requirement: Immutable runtime handoff
After startup validation and entry resolution succeed, the runtime SHALL expose one immutable authored level value to rendering and physics for the application lifetime and use the selected authored entry pose for initial player placement. Selecting an entry SHALL NOT rewrite the document's default or reorder entries. The game SHALL NOT save, hot-reload, discover, or mutate level documents while its main loop is running. Switch interactions SHALL change only separately owned run-local light enable state; authored light values, switch initial state, geometry, and collision descriptions SHALL remain unchanged.

#### Scenario: Runtime enters the frame loop
- **WHEN** the selected level and entry validate and all dependent subsystems initialize
- **THEN** rendering and physics consume the same unchanged optional terrain, solids, lights, entries, prop, and switch description for every frame and simulation step, while effective light enable state can change independently

#### Scenario: Level loading fails
- **WHEN** the selected document is absent, unreadable, malformed, unsupported, or invalid, or the selected entry is unknown
- **THEN** startup does not initialize consumers that require the resolved level and entry and releases every already-created owner exactly once
