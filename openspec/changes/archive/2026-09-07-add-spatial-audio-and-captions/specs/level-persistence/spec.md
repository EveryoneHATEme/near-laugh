## MODIFIED Requirements

### Requirement: Bounded versioned level document
The system SHALL read and write a human-readable version-7 level document describing a required nullable terrain field, one through 240 axis-aligned solids, named entries and a default entry as defined by interior-level-authoring, exactly two point lights plus ambient intensity, a required props array as defined by authored-scene-assets, a required nullable light-switch field, and a required array of zero through 32 doors as defined by interactive-doors, and a required audio object as defined by spatial-audio. Present terrain SHALL retain the 97-by-97 heightfield and add one structural material identity; each solid SHALL select a structural material independently of its existing collision kind. Props SHALL contain only identifier, model identity, translation, yaw, uniform scale and ordered local collision boxes. A non-null switch SHALL contain only position, yaw, linked point-light index (0 or 1), and boolean initial state. Each door SHALL contain only its durable identifier, hinge position, closed yaw, leaf width/height/thickness, signed opening angle, angular speed, lock side, and boolean initial open and locked states. The document SHALL contain no filesystem paths and SHALL reject missing required data, unknown fields, unsupported versions, removed shooting-target values, and out-of-profile values. Exact version-2/3/4/5 shapes SHALL remain readable; their singleton chair SHALL normalize to one prototype-chair placement with unchanged transform, legacy appearance and box, and old surface roles SHALL map to matching legacy material identities. Versions 2 through 4 SHALL normalize with no doors; version 5 SHALL preserve every authored door. Versions 2 and 3 SHALL normalize their spawn to a default entry named `default`; version 2 SHALL normalize without a switch. Older shapes SHALL NOT accept later-version fields. Versions 6 and 7 SHALL reject the old static_prop and surface fields; versions 4 through 7 SHALL reject the superseded single spawn. Exact version-6 documents SHALL preserve all their authored fields. Versions 2 through 6 SHALL normalize with empty audio collections and SHALL reject audio fields in their original shapes. Version 1 SHALL remain unsupported without a parser, translator or alias.

#### Scenario: Supported level document is read
- **WHEN** a document declares version 7 and supplies every required field within the bounded profile using current solid kinds and known asset/material identities
- **THEN** loading produces its optional terrain, solids, entries, default identifier, environment light, static placements/material assignments, optional switch, ordered door definitions, and authored audio definitions without inventing missing required data

#### Scenario: Document shape is unsupported
- **WHEN** a document has an unsupported version, missing or unknown field, excessive object count, unsupported kind or material reference, embedded resource path, or malformed door array
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
- **THEN** authored geometry, lighting, appearance, collision and entry ordering are preserved through legacy mapping, doors are empty, and the source remains unchanged

#### Scenario: Version-2 level is inspected in the editor
- **WHEN** a valid version-2 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 7, which older builds cannot read

#### Scenario: Version-3 level is inspected in the editor
- **WHEN** a valid version-3 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 7, which older builds cannot read

#### Scenario: Version-4 level is inspected in the editor
- **WHEN** a valid version-4 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 7, which older builds cannot read

#### Scenario: Version-5 level is inspected in the editor
- **WHEN** a valid version-5 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 7, which older builds cannot read

#### Scenario: Switch field is malformed
- **WHEN** a version-3, version-4, version-5, version-6, or version-7 document omits the switch field, supplies a switch array, selects a nonexistent slot, or uses a non-boolean initial state
- **THEN** loading rejects it with a field-specific diagnostic

#### Scenario: Terrain is intentionally absent
- **WHEN** a version-4, version-5, version-6, or version-7 document supplies null terrain and otherwise valid interior data
- **THEN** loading does not synthesize a heightfield

#### Scenario: Older document contains new fields
- **WHEN** a version-2, version-3, or version-4 document includes a doors field
- **THEN** strict shape validation rejects that field instead of interpreting the document as version 5

#### Scenario: Version-5 doors survive asset migration
- **WHEN** a valid version-5 level containing doors is loaded
- **THEN** all door IDs, order, configurations, entry/default/switch state, legacy chair collision and appearance are preserved in the normalized version-7 document without writing the source

#### Scenario: Content fields belong to a different version
- **WHEN** a version-2/3/4/5 document contains props or new material fields, or a version-6 or version-7 document contains static_prop or old surface fields
- **THEN** strict decoding rejects the mismatched shape rather than accepting two incompatible interpretations

#### Scenario: Version-6 level is opened and inspected
- **WHEN** a valid version-6 document is opened
- **THEN** its authored content is preserved with empty audio collections, the source is unchanged, and the editor opens it clean with notice that explicit saving writes version 7

#### Scenario: Audio shape belongs to another version
- **WHEN** a version-2 through version-6 document contains audio, or version 7 omits audio or contains an unknown audio field
- **THEN** strict decoding rejects the shape with field context

### Requirement: Deterministic semantic round trip
Saving a valid current-format level SHALL emit version 7 with canonical field order, stable solid, entry, prop, collision-box, door, cue, source, room and connection order, locale-independent numeric representation, and one trailing newline. Loading that output SHALL reproduce all authored values, including terrain absence or samples/material, solid material assignments, prop IDs/model references/transforms/boxes, entry identifiers and poses, default entry, switch absence or fields, and every door identity and initial configuration, and all authored audio fields and references. Saving again without edits SHALL be byte-identical. Exact version-2/3/4/5/6 inputs SHALL normalize through the defined compatibility mapping and SHALL be written as version 7 only on explicit save.

#### Scenario: Valid level is saved twice
- **WHEN** a valid level with repeated props, authored materials and multiple doors is saved, loaded, and saved again without edits
- **THEN** both byte sequences are identical and preserve prop/model/material identities and boxes as well as door identities, order, transforms, opening limits, and initial states

#### Scenario: Process locale differs
- **WHEN** the same valid level is saved under different process locales
- **THEN** field ordering, decimal syntax, and newline policy remain identical

#### Scenario: Version-2 document is explicitly saved
- **WHEN** the user saves a normalized version-2 document without adding a switch or doors
- **THEN** output uses version 7 with a null switch, empty doors, and the `default` entry while preserving original authored content

#### Scenario: Version-3 document is explicitly saved
- **WHEN** the user saves a normalized version-3 document
- **THEN** output uses version 7 with empty doors and preserves the switch and other authored values including the original spawn as `default`

#### Scenario: Version-4 document is explicitly saved
- **WHEN** the user saves a normalized version-4 document without other edits
- **THEN** output uses version 7 with empty doors and unchanged entries, default, terrain presence, lights, normalized chair placement/materials, and switch

#### Scenario: Version-5 document is explicitly saved
- **WHEN** the author saves a normalized version-5 level
- **THEN** version-7 output retains every door field and order alongside the mapped chair and structural materials, and reloading/saving is byte-identical

#### Scenario: Audio document round trips
- **WHEN** a valid version-7 level containing cues, sources, rooms and linked doors is saved, loaded and saved again without edits
- **THEN** every audio value, nullable reference and collection order is preserved and both outputs are byte-identical

#### Scenario: Version-6 document is explicitly saved
- **WHEN** an author explicitly saves a normalized version-6 document without adding audio
- **THEN** output uses version 7 with empty audio collections and unchanged prior content
