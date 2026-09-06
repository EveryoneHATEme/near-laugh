## MODIFIED Requirements

### Requirement: Bounded versioned level document
The system SHALL read and write a human-readable version-6 level document describing a required nullable terrain field, one through 240 axis-aligned solids, named entries and a default entry as defined by interior-level-authoring, exactly two point lights plus ambient intensity, a required props array as defined by authored-scene-assets, a required nullable light-switch field, and a required array of zero through 32 doors as defined by interactive-doors. Present terrain SHALL retain the 97-by-97 heightfield and add one structural material identity; each solid SHALL select a structural material independently of its existing collision kind. Props SHALL contain only identifier, model identity, translation, yaw, uniform scale and ordered local collision boxes. A non-null switch SHALL contain only position, yaw, linked point-light index (0 or 1), and boolean initial state. Each door SHALL contain only its durable identifier, hinge position, closed yaw, leaf width/height/thickness, signed opening angle, angular speed, lock side, and boolean initial open and locked states. The document SHALL contain no filesystem paths and SHALL reject missing required data, unknown fields, unsupported versions, removed shooting-target values, and out-of-profile values. Exact version-2/3/4/5 shapes SHALL remain readable; their singleton chair SHALL normalize to one prototype-chair placement with unchanged transform, legacy appearance and box, and old surface roles SHALL map to matching legacy material identities. Versions 2 through 4 SHALL normalize with no doors; version 5 SHALL preserve every authored door. Versions 2 and 3 SHALL normalize their spawn to a default entry named `default`; version 2 SHALL normalize without a switch. Older shapes SHALL NOT accept later-version fields. Version 6 SHALL reject the old static_prop and surface fields; versions 4 through 6 SHALL reject the superseded single spawn. Version 1 SHALL remain unsupported without a parser, translator or alias.

#### Scenario: Supported level document is read
- **WHEN** a document declares version 6 and supplies every required field within the bounded profile using current solid kinds and known asset/material identities
- **THEN** loading produces its optional terrain, solids, entries, default identifier, environment light, static placements/material assignments, optional switch, and ordered door definitions without inventing missing required data

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
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 6, which older builds cannot read

#### Scenario: Version-3 level is inspected in the editor
- **WHEN** a valid version-3 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 6, which older builds cannot read

#### Scenario: Version-4 level is inspected in the editor
- **WHEN** a valid version-4 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 6, which older builds cannot read

#### Scenario: Version-5 level is inspected in the editor
- **WHEN** a valid version-5 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 6, which older builds cannot read

#### Scenario: Switch field is malformed
- **WHEN** a version-3, version-4, version-5, or version-6 document omits the switch field, supplies a switch array, selects a nonexistent slot, or uses a non-boolean initial state
- **THEN** loading rejects it with a field-specific diagnostic

#### Scenario: Terrain is intentionally absent
- **WHEN** a version-4, version-5, or version-6 document supplies null terrain and otherwise valid interior data
- **THEN** loading does not synthesize a heightfield

#### Scenario: Older document contains new fields
- **WHEN** a version-2, version-3, or version-4 document includes a doors field
- **THEN** strict shape validation rejects that field instead of interpreting the document as version 5

#### Scenario: Version-5 doors survive asset migration
- **WHEN** a valid version-5 level containing doors is loaded
- **THEN** all door IDs, order, configurations, entry/default/switch state, legacy chair collision and appearance are preserved in the normalized version-6 document without writing the source

#### Scenario: Content fields belong to a different version
- **WHEN** a version-2/3/4/5 document contains props or new material fields, or a version-6 document contains static_prop or old surface fields
- **THEN** strict decoding rejects the mismatched shape rather than accepting two incompatible interpretations

### Requirement: Deterministic semantic round trip
Saving a valid current-format level SHALL emit version 6 with canonical field order, stable solid, entry, prop, collision-box and door order, locale-independent numeric representation, and one trailing newline. Loading that output SHALL reproduce all authored values, including terrain absence or samples/material, solid material assignments, prop IDs/model references/transforms/boxes, entry identifiers and poses, default entry, switch absence or fields, and every door identity and initial configuration. Saving again without edits SHALL be byte-identical. Exact version-2/3/4/5 inputs SHALL normalize through the defined compatibility mapping and SHALL be written as version 6 only on explicit save.

#### Scenario: Valid level is saved twice
- **WHEN** a valid level with repeated props, authored materials and multiple doors is saved, loaded, and saved again without edits
- **THEN** both byte sequences are identical and preserve prop/model/material identities and boxes as well as door identities, order, transforms, opening limits, and initial states

#### Scenario: Process locale differs
- **WHEN** the same valid level is saved under different process locales
- **THEN** field ordering, decimal syntax, and newline policy remain identical

#### Scenario: Version-2 document is explicitly saved
- **WHEN** the user saves a normalized version-2 document without adding a switch or doors
- **THEN** output uses version 6 with a null switch, empty doors, and the `default` entry while preserving original authored content

#### Scenario: Version-3 document is explicitly saved
- **WHEN** the user saves a normalized version-3 document
- **THEN** output uses version 6 with empty doors and preserves the switch and other authored values including the original spawn as `default`

#### Scenario: Version-4 document is explicitly saved
- **WHEN** the user saves a normalized version-4 document without other edits
- **THEN** output uses version 6 with empty doors and unchanged entries, default, terrain presence, lights, normalized chair placement/materials, and switch

#### Scenario: Version-5 document is explicitly saved
- **WHEN** the author saves a normalized version-5 level
- **THEN** version-6 output retains every door field and order alongside the mapped chair and structural materials, and reloading/saving is byte-identical

### Requirement: Shared level validation and diagnostics
Loaded and editor-produced levels SHALL pass the same structural and gameplay validation before saving or runtime handoff. Validation SHALL reject non-finite values or derived bounds, non-positive dimensions, invalid present terrain or unsupported slopes, invalid entries or default references, unsupported entry foot positions, standing entry clearance overlapping blocking geometry including doors in their authored initial poses, invalid light bounds, invalid or duplicate prop identities, unknown model/material references, out-of-bound prop/box counts, invalid prop transforms or proxy extents, invalid solids, invalid switches or transformed bounds, and door definitions outside interactive-doors constraints. Each entry's height SHALL match walkable terrain or the upward top face of a structural solid at its horizontal position within numerical tolerance; validation SHALL NOT move the entry or substitute another floor. Props, switches, and doors SHALL NOT provide entry support. Terrain presence or footprint SHALL NOT define a world boundary for entries, props, switches, solids, or doors. Switch validation SHALL NOT require wall attachment or certify reachability or visibility. Door validation SHALL reject initial leaf penetration of terrain, structural collision, any authored prop box, another initial door leaf, or any standing entry; it SHALL NOT require the entire possible swing to be unobstructed. A failure SHALL identify the level path, failing field or object when available, and a concise reason; entry, prop, and door diagnostics SHALL identify a usable durable identifier or otherwise the array location.

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
- **WHEN** an initially open or closed leaf penetrates a structural solid, any prop box, terrain, or another initial leaf
- **THEN** validation identifies the door and prevents saving and launch while a safely decoded editor document remains available for repair

#### Scenario: Later swing is obstructed
- **WHEN** the initial leaf placement is clear but its possible swing encounters a wall or another door
- **THEN** initial validation permits the placement and runtime obstruction behavior governs the attempted movement

#### Scenario: New material conflicts with structural use
- **WHEN** a solid or terrain references a cutout model material instead of a supported opaque structural material
- **THEN** shared validation identifies its material field and prevents a visually perforated but solid-colliding structural surface

#### Scenario: Repeated prop invalidates an entry or door
- **WHEN** an authored box of any static placement overlaps a standing entry or an initially positioned door
- **THEN** validation identifies the affected objects and blocks saving/play until correction without loading any render geometry

### Requirement: Immutable runtime handoff
After startup validation and entry resolution succeed, the runtime SHALL expose one immutable authored level value to rendering and physics for the application lifetime and initialize the player at the selected entry. Selecting an entry SHALL NOT rewrite the default or reorder entries. The game SHALL NOT save, hot-reload, discover, or mutate level documents in its main loop. Switch interactions SHALL change separately owned light enable state; door interactions SHALL change separately owned door motion, lock, and feedback state. Authored light values, switch initial state, geometry, prop IDs and model/material references, collision definitions, door identifiers, and door initial configurations SHALL remain unchanged. Static model placements SHALL remain fixed and their resources SHALL be loaded only during scene initialization.

#### Scenario: Runtime enters the frame loop
- **WHEN** the selected level and entry validate and dependent subsystems initialize
- **THEN** consumers retain the same immutable authored definitions while effective light state and door poses and locks can change independently

#### Scenario: Level loading fails
- **WHEN** the selected document is absent, unreadable, malformed, unsupported, or invalid, or its selected entry is unknown
- **THEN** startup constructs no consumer requiring the unresolved level or entry and releases every already-created owner exactly once

#### Scenario: Door actions are followed by restart
- **WHEN** the player changes door pose and lock state, exits, and starts again
- **THEN** the source file is unchanged and the new run restores authored initial door state
