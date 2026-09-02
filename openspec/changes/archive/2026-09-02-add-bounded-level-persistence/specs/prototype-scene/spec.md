## MODIFIED Requirements

### Requirement: Built-in static scene
The executable SHALL present a deterministic built-in scene composed from one immutable prototype-level description loaded from the required packaged level asset, whose finite heightfield terrain, solid structural dimensions, and one static model-prop placement with a simple collision proxy are consumed by rendering and physics. The description SHALL include a sculpted ground surface, enclosing or boundary geometry, multiple objects with visibly distinct textured appearances and overlapping depth from the initial player pose, one walkable low step, one low-clearance structure that permits crouched but not standing passage, and one recognizable imported static prop. The scene SHALL require only its packaged level, model, shader, and fixed surface-texture resources and SHALL NOT require a general material or separate collision asset.

#### Scenario: Prototype scene starts
- **WHEN** level loading, runtime, physics, model loading, and renderer initialization succeed
- **THEN** the first rendered player-camera frame shows non-planar ground, generated structures, and one recognizable imported static prop at different depths rather than only engine-generated or clip-space geometry

#### Scenario: Scene collision is constructed
- **WHEN** the immutable loaded prototype level is supplied to rendering and physics initialization
- **THEN** visible and collidable terrain, boundary, obstacle, step, and low-clearance structures derive from matching level data, and the imported prop receives its declared simple static collision proxy

#### Scenario: Scene assets are packaged
- **WHEN** the executable is copied or launched from its executable-relative runtime layout
- **THEN** the required level asset, static GLB, fixed surface textures, and collision descriptions remain available without an external material or separate collision asset

### Requirement: Built-in inert plate geometry
The immutable loaded prototype-level description SHALL include exactly three visually separated inert textured plates. Each plate's authored placement and dimensions SHALL derive matching opaque world-space geometry and static physics collision, each plate SHALL select the fixed shooting-target surface texture, and the plates SHALL require no gameplay target description, mutable health, model, general material, or separate collision asset beyond the packaged level, existing executable-relative shaders, and fixed textures.

#### Scenario: Target plates are constructed
- **WHEN** the packaged prototype level is loaded and validates successfully
- **THEN** it contains three distinct inert plate solids assigned to the shooting-target surface texture without target gameplay descriptions

#### Scenario: Target geometry reaches rendering and physics
- **WHEN** renderer and physics initialization consume the same loaded prototype level
- **THEN** each plate has textured visible geometry and collidable geometry with matching placement and dimensions

#### Scenario: Prototype plates are packaged
- **WHEN** the executable runs from its copied executable-relative resource layout
- **THEN** all plate descriptions and their fixed surface texture remain available without an additional target, model, material, or collision asset
