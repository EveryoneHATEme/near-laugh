# character-animation Specification

## Purpose

Defines this game's controlled animated mannequin assets and deterministic
pose playback so authored character motion can be inspected and rendered.

## Requirements

### Requirement: Reproducible selected character resources
The project SHALL package one neutral mannequin identity with idle, walk and interact clips prepared from the pinned local non-root-motion animation library. Preparation SHALL retain source and derivative hashes, license notices, selected clips and intentional material/transform conversions. Normal builds and runtime SHALL use committed prepared resources without requiring downloads, source archives or build/p07-assets. The source files SHALL remain unchanged. The catalog SHALL expose stable logical model/clip identities and movement/contact metadata without level-controlled filesystem paths. Unselected resources SHALL NOT be required.

#### Scenario: Clean checkout is built
- **WHEN** packaged derivatives and licenses are present but local source downloads are absent
- **THEN** the viewer and selected runtime resources work without asset conversion or network access

#### Scenario: Derivative is regenerated
- **WHEN** preparation runs twice against the same verified input
- **THEN** it produces identical prepared files and records the selected clips and conversions with matching hashes

### Requirement: Controlled animated GLB profile
Selected character assets SHALL use embedded binary glTF 2.0 with one default scene, one acyclic skeleton of at most 65 joints within at most 80 nodes, one skin and one identity-transformed mesh with one or two indexed triangle primitives sharing that skin. Total geometry SHALL be bounded to 10,000 source vertices and 50,000 indices. Each primitive SHALL supply finite POSITION, NORMAL and TEXCOORD_0 data, four valid JOINTS_0 indices and four finite nonnegative WEIGHTS_0 values with positive total normalized within 0.00001 of one, plus valid unsigned indices. Joint transforms SHALL be finite translation and unit rotation with unit scale; inverse bind matrices SHALL be finite, nonsingular and consistent with the supported bind pose. Materials SHALL be constant base-color OPAQUE, metallic zero, roughness one, with no other shading input or texture, and rendered on both sides. The asset SHALL contain exactly the catalog's three uniquely named clips, with finite nonnegative strictly increasing time keys, at most 256 keys per channel, at most one translation and one rotation channel per joint per clip, LINEAR interpolation, and positive duration at most 10 seconds. The root transform SHALL remain constant across clips. The file SHALL be at most 16 MiB and decoded CPU data at most 32 MiB. External references, extensions, compression, sparse accessors, unsupported attributes/channels, animated scale, morphs, cameras, lights, extra scenes/skins/meshes and out-of-range counts or accesses SHALL fail with asset and field context before unsafe allocation or runtime handoff. Existing static assets SHALL retain their separate static profile.

#### Scenario: Prepared mannequin is loaded
- **WHEN** its geometry, skin, material and clips satisfy the profile
- **THEN** loading obtains immutable validated data for both primitives and all three clips

#### Scenario: Animation input is malformed
- **WHEN** a channel has duplicate times, a missing target, invalid weights/joints, a cyclic hierarchy, invalid bind data or an overflowing byte range
- **THEN** loading rejects it with model and channel/accessor context without returning a partially usable character

#### Scenario: Raw source is passed to the runtime loader
- **WHEN** the unprepared library or a Superhero source uses excluded clips, scale channels or material/mesh features
- **THEN** it is rejected explicitly instead of silently truncating or treating it as a static prop

### Requirement: Deterministic sampled poses and bounded transitions
Sampling SHALL reproduce the supported bind pose and clip key poses, interpolate translations linearly and rotations along the normalized shortest arc, and use rest values for absent channels. Idle and walk SHALL loop over a half-open duration; interact SHALL clamp at its final pose and report completion once to its owner. Playback SHALL support a single transition from the currently displayed local pose to one selected target clip over 0.15 seconds; interruption SHALL begin from the current blended pose without a visible bind-pose reset. Explicit pause SHALL freeze clip and transition time, and explicit seek SHALL produce the corresponding pose without generating gameplay or audio events. Invalid clip identity or non-finite time SHALL be rejected. Independent instances SHALL share immutable assets and retain independent pose state. The same accepted elapsed time and commands SHALL produce equivalent poses regardless of render batching.

#### Scenario: Loop wraps and one-shot completes
- **WHEN** sample time reaches or passes a clip duration
- **THEN** loops wrap deterministically, while interact holds its last sample and does not repeatedly emit completion

#### Scenario: Transition is interrupted
- **WHEN** a second clip is requested during a blend
- **THEN** blending starts at the currently displayed pose and reaches the new target without first returning to either clip's start pose

#### Scenario: Paused pose is inspected
- **WHEN** the viewer pauses or seeks to a selected time
- **THEN** the visible result is stable and no route, footstep or interaction result is generated by inspection

### Requirement: Independent animation acceptance
An explicit development viewer SHALL select the prepared mannequin independently of level filenames and allow idle/walk/interact selection, restart, pause/resume and time inspection for one or four independently posed instances. Acceptance SHALL retain bind/clip/transition and lighting captures, visual observations, resource-failure/recovery results and release CPU active-work, GPU whole-frame/shadow and end-to-end frame timings against the same lighting scene without characters. It SHALL record actual hardware, driver, build, resolution, conditions and unavailable measurements. Passing automated checks alone SHALL NOT establish acceptable animation appearance or complete T2.

#### Scenario: Viewer is inspected under lighting
- **WHEN** one then four mannequins play the selected clips under the supported point lights
- **THEN** the record covers mesh deformation, normals, scale, loop/blend behavior, matching shadows, recovery and timing deltas

#### Scenario: Ordinary level is opened
- **WHEN** the game opens a file whose name resembles the viewer fixture
- **THEN** no development clip sequence is inferred from that name
