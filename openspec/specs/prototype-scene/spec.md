# prototype-scene Specification

## Purpose

Defines the small built-in 3D environment used to validate static scene rendering and free camera inspection before asset loading or gameplay physics exist.

## Requirements

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

### Requirement: Camera-driven scene inspection
Every renderable frame SHALL depict the built-in scene from the current collision-constrained first-person player camera frame. Static level collision SHALL prevent the player from crossing solid floors, boundaries, and obstacles while allowing traversal of the declared walkable step and crouched passage through the declared low-clearance structure.

#### Scenario: Camera pose changes
- **WHEN** grounded movement, jumping, gravity, crouching, or look input changes the player camera position or orientation
- **THEN** the next rendered frame depicts the same static scene from the updated collision-constrained pose

#### Scenario: Camera crosses geometry
- **WHEN** requested player movement would carry the camera through a floor, wall, or blocking object surface
- **THEN** collision prevents the player camera from crossing the corresponding solid structure

#### Scenario: Player uses movement-test geometry
- **WHEN** the player approaches the low step while standing or the low-clearance route while crouched
- **THEN** the visible environment provides matching collision that permits the intended traversal

### Requirement: Depth-correct opaque visibility
The prototype scene SHALL render opaque surfaces with depth testing so that the nearest visible surface wins independently of geometry submission order.

#### Scenario: Geometry overlaps in screen space
- **WHEN** two scene surfaces project onto the same framebuffer region at different depths
- **THEN** the nearer surface obscures the farther surface

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
