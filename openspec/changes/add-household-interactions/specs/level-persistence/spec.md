## MODIFIED Requirements

### Requirement: Bounded versioned level document
The system SHALL read and write a human-readable version-10 level document describing a required nullable terrain field, one through 240 axis-aligned solids, named entries and a default entry as defined by interior-level-authoring, zero through eight point lights plus ambient intensity as defined by interior-lighting, a required props array as defined by authored-scene-assets, a required array of zero through sixteen light switches as defined by light-switch, and a required array of zero through 32 doors as defined by interactive-doors, a required audio object as defined by spatial-audio, a required characters object as defined by scripted-characters, and a required household object containing exactly boxes, documents and radios arrays as defined by household-interactions. Present terrain SHALL retain the 97-by-97 heightfield and add one structural material identity; each solid SHALL select a structural material independently of its existing collision kind. Props SHALL contain only identifier, model identity, translation, yaw, uniform scale and ordered local collision boxes. Each point light SHALL contain only its durable ID, position, RGB color, intensity, radius, initial enabled state and shadow flag. Each switch SHALL contain only its durable ID, position, yaw and referenced light ID. The current shape SHALL use light_switches rather than the old nullable light_switch field. Each door SHALL contain only its durable identifier, hinge position, closed yaw, leaf width/height/thickness, signed opening angle, angular speed, lock side, and boolean initial open and locked states. The document SHALL contain no filesystem paths and SHALL reject missing required data, unknown fields, unsupported versions, removed shooting-target values, and out-of-profile values. Exact version-2/3/4/5 shapes SHALL remain readable; their singleton chair SHALL normalize to one prototype-chair placement with unchanged transform, legacy appearance and box, and old surface roles SHALL map to matching legacy material identities. Versions 2 through 4 SHALL normalize with no doors; version 5 SHALL preserve every authored door. Versions 2 and 3 SHALL normalize their spawn to a default entry named `default`; version 2 SHALL normalize without a switch. Older shapes SHALL NOT accept later-version fields. Versions 6 through 10 SHALL reject the old static_prop and surface fields; versions 4 through 10 SHALL reject the superseded single spawn. Exact version-6 and version-7 documents SHALL preserve their authored geometry, appearance, entries, doors and audio where present. Their lighting and switches, and those from versions 2 through 5, SHALL normalize as follows: old light slots receive IDs point-light-0 and point-light-1, with shadows disabled and initial enables true; an old switch becomes light-switch-0 referencing its old target by the mapped ID, and its initial enable moves onto that light. An absent old switch becomes an empty switch array. Authored light parameters, ambient and ordering SHALL remain unchanged. Versions 8 through 10 SHALL reject old light-index/switch-initial fields; older shapes SHALL reject new light/switch fields. Versions 2 through 6 SHALL normalize with empty audio collections and SHALL reject audio fields in their original shapes. Exact version-8 documents SHALL preserve every v8 field and normalize to empty character collections without applying legacy lighting remapping. Exact v2-v7 inputs SHALL also normalize to empty character collections after their existing migrations. Versions 2 through 8 SHALL reject characters; versions 9 and 10 SHALL require that object with exactly actors, marks and routes. Exact v9 inputs SHALL preserve every existing field and order and normalize to empty household collections. Exact v2-v8 inputs SHALL also normalize to empty household collections after their existing migrations. Versions 2 through 9 SHALL reject household fields, and v10 SHALL require all three household arrays without accepting runtime state or arbitrary action fields. Version 1 SHALL remain unsupported without a parser, translator or alias.

#### Scenario: Supported level document is read
- **WHEN** a document declares version 10 and supplies every required field within the bounded profile using current solid kinds and known asset/material identities
- **THEN** loading produces its optional terrain, solids, entries, default identifier, environment light, static placements/material assignments, switch array, ordered door definitions, authored audio definitions, character definitions and household definitions without inventing missing required data

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
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 10, which older builds cannot read

#### Scenario: Version-3 level is inspected in the editor
- **WHEN** a valid version-3 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 10, which older builds cannot read

#### Scenario: Version-4 level is inspected in the editor
- **WHEN** a valid version-4 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 10, which older builds cannot read

#### Scenario: Version-5 level is inspected in the editor
- **WHEN** a valid version-5 level is opened in the editor
- **THEN** the normalized document starts clean and the editor identifies that an explicit save writes version 10, which older builds cannot read

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
- **THEN** all door IDs, order, configurations, entry/default/switch state, legacy chair collision and appearance are preserved in the normalized version-10 document without writing the source

#### Scenario: Content fields belong to a different version
- **WHEN** a version-2/3/4/5 document contains props or new material fields, or a version-6 or version-7 document contains static_prop or old surface fields
- **THEN** strict decoding rejects the mismatched shape rather than accepting two incompatible interpretations

#### Scenario: Version-6 level is opened and inspected
- **WHEN** a valid version-6 document is opened
- **THEN** its authored content is preserved with empty audio collections, the source is unchanged, and the editor opens it clean with notice that explicit saving writes version 10

#### Scenario: Audio shape belongs to another version
- **WHEN** a version-2 through version-6 document contains audio, or version 7 omits audio or contains an unknown audio field
- **THEN** strict decoding rejects the shape with field context

#### Scenario: Version-7 lighting is migrated
- **WHEN** a valid v7 file contains two lights and an initially-off switch targeting slot 1
- **THEN** v10 normalization preserves parameters/order/audio and produces point-light-0 initially on, point-light-1 initially off, both unshadowed, and light-switch-0 referencing point-light-1 without writing the source

#### Scenario: Current light arrays use the wrong shape
- **WHEN** v9 omits light_switches, contains light_switch, omits required light IDs/flags, or puts a light index or initial state on a switch
- **THEN** decoding rejects the mismatched field with context rather than guessing a migration

#### Scenario: Current audio remains required
- **WHEN** a v9 document omits audio or supplies unknown fields within it
- **THEN** strict decoding rejects the invalid audio shape just as for v7

#### Scenario: Version-8 document is opened
- **WHEN** an exact valid v8 file with P10 lights, switches and audio is opened
- **THEN** every existing field and order is preserved, characters normalize to empty arrays, the source remains untouched and the editor opens clean with a v10 explicit-save notice

#### Scenario: Characters belong to a different version
- **WHEN** v8 contains characters, v9 omits characters, or a characters object has missing or unknown fields
- **THEN** strict shape validation rejects the mismatch with field context

#### Scenario: Version-9 scene is opened without rewriting
- **WHEN** an exact valid v9 scene containing P07 actors/routes is opened
- **THEN** all existing definitions and order survive, household arrays normalize empty, the file stays untouched, and the editor opens clean with notice that explicit Save writes v10

#### Scenario: Household shape belongs to another version
- **WHEN** v9 contains household fields, v10 omits household or one of its required arrays, or a household record supplies unknown fields or runtime state
- **THEN** strict decoding rejects the mismatch with field context instead of silently accepting or discarding it


### Requirement: Deterministic semantic round trip
Saving a valid current-format level SHALL emit version 10 with canonical field order, stable solid, entry, light, switch, prop, collision-box, door, cue, source, room, connection, actor, mark, route, household box, readable document/page and radio order, locale-independent numeric representation, and one trailing newline. Loading that output SHALL reproduce all authored values, including terrain absence or samples/material, solid material assignments, prop IDs/model references/transforms/boxes, entry identifiers and poses, default entry, every light ID/parameter/initial/shadow flag and switch ID/placement/light reference, and every door identity and initial configuration, all authored audio fields and references, every character field, nullable reference and ordered route mark list, and every household ID, initial transform, readable title/page, radio link and initial on/off value. Saving again without edits SHALL be byte-identical. Exact version-2/3/4/5/6/7/8/9 inputs SHALL normalize through the defined compatibility mapping and SHALL be written as version 10 only on explicit save.

#### Scenario: Valid level is saved twice
- **WHEN** a valid level with repeated props, authored materials and multiple doors is saved, loaded, and saved again without edits
- **THEN** both byte sequences are identical and preserve prop/model/material identities and boxes as well as door identities, order, transforms, opening limits, and initial states

#### Scenario: Process locale differs
- **WHEN** the same valid level is saved under different process locales
- **THEN** field ordering, decimal syntax, and newline policy remain identical

#### Scenario: Version-2 document is explicitly saved
- **WHEN** the user saves a normalized version-2 document without adding a switch or doors
- **THEN** output uses version 10 with an empty switch array, empty doors, and the `default` entry while preserving original authored content

#### Scenario: Version-3 document is explicitly saved
- **WHEN** the user saves a normalized version-3 document
- **THEN** output uses version 10 with empty doors and preserves the switch and other authored values including the original spawn as `default`

#### Scenario: Version-4 document is explicitly saved
- **WHEN** the user saves a normalized version-4 document without other edits
- **THEN** output uses version 10 with empty doors and unchanged entries, default, terrain presence, lights, normalized chair placement/materials, and switch

#### Scenario: Version-5 document is explicitly saved
- **WHEN** the author saves a normalized version-5 level
- **THEN** version-10 output retains every door field and order alongside the mapped chair and structural materials, and reloading/saving is byte-identical

#### Scenario: Audio document round trips
- **WHEN** a valid version-7 level containing cues, sources, rooms and linked doors is saved, loaded and saved again without edits
- **THEN** every audio value, nullable reference and collection order is preserved and both outputs are byte-identical

#### Scenario: Version-6 document is explicitly saved
- **WHEN** an author explicitly saves a normalized version-6 document without adding audio
- **THEN** output uses version 10 with empty audio collections and unchanged prior content

#### Scenario: Light identity survives collection edits
- **WHEN** a valid v9 document with shared switch links and reordered lights is saved, loaded and saved again
- **THEN** light/switch IDs, links, array order, flags and ambient are preserved and the two outputs are byte-identical

#### Scenario: Version-7 document is explicitly saved
- **WHEN** an author explicitly saves a normalized v7 document
- **THEN** output is canonical v10 with the deterministic light/switch migration and unchanged audio/door/prop content

#### Scenario: Character routes round trip
- **WHEN** a v9 document with four actors, shared marks, null/non-null sources and initial routes is saved, loaded and saved again
- **THEN** every actor/mark/route field, link and ordering is preserved and both outputs are byte-identical

#### Scenario: Version-8 file is explicitly saved
- **WHEN** a normalized v8 document is explicitly saved without adding actors
- **THEN** output is canonical v10 with empty character arrays and unchanged v8 content; retaining an original for an older build requires Save As or a separate copy

#### Scenario: Physical household definitions round trip
- **WHEN** a v10 scene with boxes, Russian multipage documents and radio links is saved, loaded and saved again
- **THEN** every authored value and order survives, both outputs are byte-identical, and no current velocity, held object or opened page is serialized

#### Scenario: Version-9 scene is explicitly saved
- **WHEN** an author saves a normalized v9 scene without adding household objects
- **THEN** output is canonical v10 with empty household arrays and unchanged prior content, and use with an older build requires preserving the original separately


### Requirement: Immutable runtime handoff
After startup validation and entry resolution succeed, the runtime SHALL expose one immutable authored level value to rendering and physics for the application lifetime and initialize the player at the selected entry. Selecting an entry SHALL NOT rewrite the default or reorder entries. The game SHALL NOT save, hot-reload, discover, or mutate level documents in its main loop. Switch interactions SHALL change separately owned light enable state; door interactions SHALL change separately owned door motion, lock, and feedback state. Authored light values, per-light initial and shadow flags and switch references, geometry, prop IDs and model/material references, collision definitions, door identifiers, and door initial configurations SHALL remain unchanged. Household interactions SHALL change separately owned physical box poses/velocities/held state, reading state and radio enable state without changing household initial definitions. Static model placements SHALL remain fixed and their resources SHALL be loaded only during scene initialization.

#### Scenario: Runtime enters the frame loop
- **WHEN** the selected level and entry validate and dependent subsystems initialize
- **THEN** consumers retain the same immutable authored definitions while effective light state and door poses and locks can change independently

#### Scenario: Level loading fails
- **WHEN** the selected document is absent, unreadable, malformed, unsupported, or invalid, or its selected entry is unknown
- **THEN** startup constructs no consumer requiring the unresolved level or entry and releases every already-created owner exactly once

#### Scenario: Door actions are followed by restart
- **WHEN** the player changes door pose and lock state, exits, and starts again
- **THEN** the source file is unchanged and the new run restores authored initial door state

#### Scenario: A physical box moves during play
- **WHEN** the player holds, drops or throws an authored household box
- **THEN** the physical and displayed pose changes independently of the immutable initial center/yaw and no level file is written


## ADDED Requirements

### Requirement: Shared household validation
Shared validation SHALL enforce the household counts, IDs, finite derived bounds, fixed box profile, readable content bounds and radio ownership/reference rules before Save or runtime handoff, without requiring a physics world, audio device or GPU. Initial boxes SHALL have zero initial velocities and SHALL NOT penetrate terrain, structures, static prop boxes, other boxes, accepted initial doors/actors or any standing player entry. Contact at a supporting surface within the defined numerical tolerance SHALL be valid; an initially airborne but clear box SHALL be valid and fall during play. Boxes SHALL NOT provide authored player-entry or actor-mark support. Radio renames/deletions and source/prop edits SHALL be validated across household and existing actor/audio consumers. Text glyph and minimum-framebuffer layout validation SHALL additionally run in selected-resource preflight. Safely decoded invalid definitions SHALL remain editable with the record, link or page identified.

#### Scenario: Initial box overlaps an entry or door
- **WHEN** an authored initial box penetrates a standing entry, actor or initial door leaf
- **THEN** shared diagnostics identify the affected objects and refuse Save and Play without moving either object automatically

#### Scenario: Initial box is clear above a floor
- **WHEN** a box begins above supporting geometry without overlap
- **THEN** validation accepts the authored center and ordinary runtime gravity determines its later position

#### Scenario: Radio source is also used by a character
- **WHEN** a radio and actor reference the same owned source
- **THEN** shared validation rejects the ownership conflict and preserves the broken reference for editor repair
