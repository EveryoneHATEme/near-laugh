## MODIFIED Requirements

### Requirement: Bounded versioned level document
The system SHALL read and write a human-readable version-3 level document describing one 97-by-97 heightfield, no more than 240 axis-aligned solids, one player spawn, exactly two point lights plus one ambient intensity, exactly one packaged static prop with its box collision proxy, and a required nullable light-switch field. A non-null switch SHALL contain only its position, yaw, linked point-light index (0 or 1), and boolean initial state. The document SHALL retain the fixed current solid kinds and surface roles, contain no filesystem paths, and reject missing required data, unknown fields, unsupported versions, removed shooting-target values, and values outside the bounded profile. The exact version-2 profile SHALL remain readable and normalize to a current document without a switch; it SHALL NOT accept version-3 fields. Version-1 test levels SHALL remain unsupported without a parser, translator, or alias.

#### Scenario: Supported level document is read
- **WHEN** a document declares version 3 and supplies every required field within the bounded profile using only current solid kinds and surface roles
- **THEN** loading produces the corresponding terrain, solids, spawn, environment light, static-prop placement, and explicit switch presence or absence without inventing missing required data

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
- **THEN** loading preserves its authored terrain, solids, spawn, lights, and prop and produces a current-format in-memory document without a switch, without rewriting the source file

#### Scenario: Version-2 level is inspected in the editor
- **WHEN** the user opens a valid version-2 level in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 3, which older builds cannot read

#### Scenario: Switch field is malformed
- **WHEN** a version-3 document omits the switch field, supplies a switch array, references a nonexistent light slot, or uses a non-boolean initial state
- **THEN** loading rejects the document with a field-specific diagnostic

### Requirement: Deterministic semantic round trip
Saving a valid current-format level SHALL emit version 3 with canonical field order, stable object order, locale-independent numeric representation, and one trailing newline. Loading the emitted document SHALL reproduce the same level values, including switch absence or all authored switch fields, and saving the reproduced document without edits SHALL produce byte-identical output. A version-2 input SHALL normalize before this round trip and SHALL be written as version 3 only on an explicit save.

#### Scenario: Valid level is saved twice
- **WHEN** a valid in-memory level is saved, loaded from that result, and saved again without edits
- **THEN** the two saved byte sequences are identical and describe semantically equal level data

#### Scenario: Process locale differs
- **WHEN** the same valid level is saved under different process locales
- **THEN** each output uses the same field ordering, decimal syntax, and newline policy

#### Scenario: Version-2 document is explicitly saved
- **WHEN** the user explicitly saves a document loaded from version 2 without adding a switch or changing its other authored fields
- **THEN** the output uses version 3 with a null switch while preserving the original authored content, and a subsequent load/save is byte-identical

### Requirement: Shared level validation and diagnostics
Loaded and editor-produced levels SHALL pass the same structural and gameplay validation before they can be saved or handed to runtime consumers. Validation SHALL reject non-finite values, non-positive dimensions, unsupported terrain slopes, a spawn outside or unsupported by the terrain, spawn overlap with blocking geometry, invalid light bounds, invalid prop transforms or proxy extents, invalid solid data, and invalid switch fields or transformed bounds. A switch SHALL have finite position and yaw, a valid point-light index, and horizontal bounds inside the terrain footprint; validation SHALL NOT require wall attachment or certify that the switch is reachable or unoccluded. A parse or validation failure SHALL identify the level path, the failing field or object when available, and a concise reason.

#### Scenario: Parsed level violates gameplay constraints
- **WHEN** a syntactically valid document places the spawn inside a blocking solid or contains an unsupported terrain slope
- **THEN** validation rejects the level before renderer or physics initialization and identifies the violated constraint

#### Scenario: Level syntax is malformed
- **WHEN** a required level document cannot be parsed
- **THEN** loading fails with an actionable diagnostic containing the resolved path and source location or field context when available

#### Scenario: Switch placement is invalid
- **WHEN** a finite switch placement extends horizontally outside the terrain footprint or has non-finite transformed bounds
- **THEN** shared validation reports the switch field and refuses saving or runtime construction

### Requirement: Immutable runtime handoff
After startup validation succeeds, the runtime SHALL expose one immutable authored level value to rendering and physics for the application lifetime. The game SHALL NOT save, hot-reload, discover, or mutate level documents while its main loop is running. Switch interactions SHALL change only separately owned run-local light enable state; authored light values, switch initial state, geometry, and collision descriptions SHALL remain unchanged.

#### Scenario: Runtime enters the frame loop
- **WHEN** the required level document loads and validates and all dependent subsystems initialize
- **THEN** rendering and physics consume the same unchanged authored terrain, solids, lights, spawn, prop, and switch description for every frame and simulation step, while effective light enable state can change independently

#### Scenario: Level loading fails
- **WHEN** the required document is absent, unreadable, malformed, unsupported, or invalid
- **THEN** startup does not initialize consumers that require the level and releases every already-created owner exactly once
