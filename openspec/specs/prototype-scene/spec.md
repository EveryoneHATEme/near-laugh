# prototype-scene Specification

## Purpose

Defines the small built-in 3D environment used to validate static scene rendering and free camera inspection before asset loading or gameplay physics exist.

## Requirements

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
