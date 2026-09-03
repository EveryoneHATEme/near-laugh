## MODIFIED Requirements

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
