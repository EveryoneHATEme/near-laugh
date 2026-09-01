## MODIFIED Requirements

### Requirement: Built-in static scene
The executable SHALL present a deterministic built-in scene composed from one immutable prototype-level description whose solid structural dimensions are used to derive both opaque world-space renderer geometry and static physics collision. The description SHALL include a floor, enclosing or boundary geometry, multiple objects with visibly distinct textured appearances and overlapping depth from the initial player pose, one walkable low step, and one low-clearance structure that permits crouched but not standing passage. The scene SHALL use only its packaged shader and fixed surface-texture resources and SHALL NOT require a model, general material, collision, or level file.

#### Scenario: Prototype scene starts
- **WHEN** runtime, physics, and renderer initialization succeed
- **THEN** the first rendered player-camera frame shows multiple recognizable textured 3D surfaces at different distances rather than flat-colored or clip-space geometry

#### Scenario: Scene collision is constructed
- **WHEN** the immutable prototype level is supplied to rendering and physics initialization
- **THEN** visible and collidable floor, boundary, obstacle, step, and low-clearance structures are derived from matching structural dimensions

#### Scenario: Scene assets are packaged
- **WHEN** the executable is copied or launched from its executable-relative runtime layout
- **THEN** the built-in scene, fixed surface textures, and collision remain available without external model, general material, scene, or collision assets

### Requirement: Built-in shooting target geometry
The immutable prototype-level description SHALL include exactly three visually separated textured target plates with stable target descriptions that identify distinct solids. Each plate's authored placement and dimensions SHALL derive matching opaque world-space geometry and static physics collision, each plate SHALL select the fixed shooting-target surface texture, and the plates SHALL require no model, general material, collision, or level asset beyond the existing executable-relative shader and fixed texture resources.

#### Scenario: Target plates are constructed
- **WHEN** the built-in prototype level is created
- **THEN** it contains three valid target descriptions that identify three distinct plate solids assigned to the shooting-target surface texture

#### Scenario: Target geometry reaches rendering and physics
- **WHEN** renderer and physics initialization consume the same prototype level
- **THEN** each target plate has textured visible geometry and collidable geometry with matching placement and dimensions

#### Scenario: Shooting range is packaged
- **WHEN** the executable runs from its copied executable-relative resource layout
- **THEN** all target geometry and its fixed surface texture remain available without an additional target, model, material, or level asset
