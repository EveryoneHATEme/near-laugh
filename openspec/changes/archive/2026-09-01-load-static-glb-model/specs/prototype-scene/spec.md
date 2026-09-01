## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Built-in imported static prop
The immutable prototype-level description SHALL declare exactly one static model prop with a finite translation, finite yaw, positive finite uniform scale, fixed obstacle surface role, and positive finite box-collision half extents. The prop SHALL remain fixed for the run and SHALL carry no gameplay identity, health, interaction, animation, or dynamic-body state.

#### Scenario: Prototype level is constructed
- **WHEN** the built-in prototype level validates successfully
- **THEN** it exposes one valid static model placement and one valid box collision proxy without containing a filesystem path or model-library type

#### Scenario: Imported prop is inspected in play
- **WHEN** the player views or approaches the prop
- **THEN** the loaded model remains at its authored world placement and the player is blocked by its declared simple static collision volume

