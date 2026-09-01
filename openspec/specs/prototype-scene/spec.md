# prototype-scene Specification

## Purpose

Defines the small built-in 3D environment used to validate static scene rendering and free camera inspection before asset loading or gameplay physics exist.

## Requirements

### Requirement: Built-in static scene
The executable SHALL present a deterministic built-in scene composed from one immutable prototype-level description whose finite heightfield terrain, solid structural dimensions, and one static model-prop placement with a simple collision proxy are consumed by rendering and physics. The description SHALL include a sculpted ground surface, enclosing or boundary geometry, multiple objects with visibly distinct textured appearances and overlapping depth from the initial player pose, one walkable low step, one low-clearance structure that permits crouched but not standing passage, and one recognizable imported static prop. The scene SHALL require only its packaged model, shader, and fixed surface-texture resources and SHALL NOT require a general material, separate collision, or level file.

#### Scenario: Prototype scene starts
- **WHEN** runtime, physics, model loading, and renderer initialization succeed
- **THEN** the first rendered player-camera frame shows non-planar ground, generated structures, and one recognizable imported static prop at different depths rather than only engine-generated or clip-space geometry

#### Scenario: Scene collision is constructed
- **WHEN** the immutable prototype level is supplied to rendering and physics initialization
- **THEN** visible and collidable terrain, boundary, obstacle, step, and low-clearance structures derive from matching level data, and the imported prop receives its declared simple static collision proxy

#### Scenario: Scene assets are packaged
- **WHEN** the executable is copied or launched from its executable-relative runtime layout
- **THEN** the built-in scene, required static GLB, fixed surface textures, and collision remain available without an external material, collision, or level asset

### Requirement: Built-in imported static prop
The immutable prototype-level description SHALL declare exactly one static model prop with a finite translation, finite yaw, positive finite uniform scale, fixed obstacle surface role, and positive finite box-collision half extents. The prop SHALL remain fixed for the run and SHALL carry no gameplay identity, health, interaction, animation, or dynamic-body state.

#### Scenario: Prototype level is constructed
- **WHEN** the built-in prototype level validates successfully
- **THEN** it exposes one valid static model placement and one valid box collision proxy without containing a filesystem path or model-library type

#### Scenario: Imported prop is inspected in play
- **WHEN** the player views or approaches the prop
- **THEN** the loaded model remains at its authored world placement and the player is blocked by its declared simple static collision volume

### Requirement: Camera-driven scene inspection
Every renderable frame SHALL depict the built-in scene from the current collision-constrained first-person player camera frame. Static level collision SHALL prevent the player from crossing terrain, solid floors where present, boundaries, and obstacles while allowing traversal of the declared walkable step and crouched passage through the declared low-clearance structure.

#### Scenario: Camera pose changes
- **WHEN** grounded movement, jumping, gravity, crouching, or look input changes the player camera position or orientation
- **THEN** the next rendered frame depicts the same static scene from the updated collision-constrained pose

#### Scenario: Camera crosses geometry
- **WHEN** requested player movement would carry the camera through a terrain surface, floor, wall, or blocking object surface
- **THEN** collision prevents the player camera from crossing the corresponding solid structure

#### Scenario: Player uses movement-test geometry
- **WHEN** the player approaches a terrain slope, the low step while standing, or the low-clearance route while crouched
- **THEN** the visible environment provides matching collision that permits the intended traversal

### Requirement: Depth-correct opaque visibility
The prototype scene SHALL render opaque surfaces with depth testing so that the nearest visible surface wins independently of geometry submission order.

#### Scenario: Geometry overlaps in screen space
- **WHEN** two scene surfaces project onto the same framebuffer region at different depths
- **THEN** the nearer surface obscures the farther surface

### Requirement: Built-in inert plate geometry
The immutable prototype-level description SHALL include exactly three visually separated inert textured plates. Each plate's authored placement and dimensions SHALL derive matching opaque world-space geometry and static physics collision, each plate SHALL select the fixed shooting-target surface texture, and the plates SHALL require no gameplay target description, mutable health, model, general material, collision, or level asset beyond the existing executable-relative shader and fixed texture resources.

#### Scenario: Target plates are constructed
- **WHEN** the built-in prototype level is created
- **THEN** it contains three distinct inert plate solids assigned to the shooting-target surface texture without target gameplay descriptions

#### Scenario: Target geometry reaches rendering and physics
- **WHEN** renderer and physics initialization consume the same prototype level
- **THEN** each plate has textured visible geometry and collidable geometry with matching placement and dimensions

#### Scenario: Prototype plates are packaged
- **WHEN** the executable runs from its copied executable-relative resource layout
- **THEN** all plate geometry and its fixed surface texture remain available without an additional target, model, material, or level asset
