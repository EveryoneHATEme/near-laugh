# level-persistence Specification

## Purpose

Defines the bounded, versioned level document used to author and load the game's world while preserving deterministic validation and immutable runtime consumption.

## Requirements

### Requirement: Bounded versioned level document
The system SHALL accept one human-readable version-2 level document with complete descriptions of one 97-by-97 heightfield, no more than 240 axis-aligned solids, one player spawn, exactly two point lights plus one ambient intensity, and exactly one placement of the packaged static prop with its box collision proxy. The document SHALL use only the fixed solid kinds and surface roles required by the current game prototype, SHALL contain no filesystem paths, and SHALL reject missing required data, unsupported versions, unknown fields, removed legacy shooting-target values, and values outside the bounded profile. The project SHALL reject version-1 test levels and SHALL NOT retain a compatibility parser, translator, or alias for them.

#### Scenario: Supported level document is read
- **WHEN** a document declares version 2 and supplies every required field within the bounded profile using only current solid kinds and surface roles
- **THEN** loading produces the corresponding terrain, solids, spawn, environment light, and static-prop placement without inventing defaults

#### Scenario: Document shape is unsupported
- **WHEN** a document has an unsupported version, missing field, unknown field, excessive solid count, unsupported or removed kind or surface role, or embedded resource path
- **THEN** loading rejects the document before producing a runtime level

#### Scenario: Legacy shooting-target value is read
- **WHEN** a test level document contains `shooting_target` as a solid kind or surface role
- **THEN** loading rejects the document as unsupported rather than translating or accepting the legacy value

#### Scenario: Version-1 test level is read
- **WHEN** a test level document declares format version 1
- **THEN** loading rejects the version without attempting to migrate or translate the document

### Requirement: Deterministic semantic round trip
Saving a valid level SHALL emit a canonical field order, stable object order, locale-independent numeric representation, and one trailing newline. Loading the emitted document SHALL reproduce the same level values, and saving the reproduced level without edits SHALL produce byte-identical output.

#### Scenario: Valid level is saved twice
- **WHEN** a valid in-memory level is saved, loaded from that result, and saved again without edits
- **THEN** the two saved byte sequences are identical and describe semantically equal level data

#### Scenario: Process locale differs
- **WHEN** the same valid level is saved under different process locales
- **THEN** each output uses the same field ordering, decimal syntax, and newline policy

### Requirement: Shared level validation and diagnostics
Loaded and editor-produced levels SHALL pass the same structural and gameplay validation before they can be saved or handed to runtime consumers. Validation SHALL reject non-finite values, non-positive dimensions, unsupported terrain slopes, a spawn outside or unsupported by the terrain, spawn overlap with blocking geometry, invalid light bounds, invalid prop transforms or proxy extents, and invalid solid data. A parse or validation failure SHALL identify the level path, the failing field or object when available, and a concise reason.

#### Scenario: Parsed level violates gameplay constraints
- **WHEN** a syntactically valid document places the spawn inside a blocking solid or contains an unsupported terrain slope
- **THEN** validation rejects the level before renderer or physics initialization and identifies the violated constraint

#### Scenario: Level syntax is malformed
- **WHEN** a required level document cannot be parsed
- **THEN** loading fails with an actionable diagnostic containing the resolved path and source location or field context when available

### Requirement: Immutable runtime handoff
After startup validation succeeds, the runtime SHALL expose one immutable level value to rendering and physics for the application lifetime. The game SHALL NOT save, hot-reload, discover, or mutate level documents while its main loop is running.

#### Scenario: Runtime enters the frame loop
- **WHEN** the required level document loads and validates and all dependent subsystems initialize
- **THEN** rendering and physics consume the same unchanged terrain, solids, lights, spawn, and prop description for every frame and simulation step

#### Scenario: Level loading fails
- **WHEN** the required document is absent, unreadable, malformed, unsupported, or invalid
- **THEN** startup does not initialize consumers that require the level and releases every already-created owner exactly once
