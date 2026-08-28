# prototype-scene Specification

## Purpose

Defines the small built-in 3D environment used to validate static scene rendering and free camera inspection before asset loading or gameplay physics exist.

## Requirements

### Requirement: Built-in static scene
The executable SHALL present a deterministic built-in scene composed of opaque world-space geometry that includes a floor, enclosing or boundary geometry, and multiple objects with visibly distinct colors and overlapping depth from the initial camera pose. The scene SHALL start without requiring a model, texture, material, or level file outside the runtime's existing shader resources.

#### Scenario: Prototype scene starts
- **WHEN** runtime and renderer initialization succeed
- **THEN** the first rendered camera frame shows multiple recognizable 3D surfaces at different distances rather than the previous clip-space triangle

#### Scenario: Scene assets are packaged
- **WHEN** the executable is copied or launched from its executable-relative runtime layout
- **THEN** the built-in scene remains available without additional external scene assets

### Requirement: Camera-driven scene inspection
Every renderable frame SHALL depict the built-in scene from the current free-fly camera frame, and the scene SHALL impose no collision, gravity, or ground constraint on that camera.

#### Scenario: Camera pose changes
- **WHEN** free-fly input changes the camera position or orientation
- **THEN** the next rendered frame depicts the same static scene from the updated pose

#### Scenario: Camera crosses geometry
- **WHEN** free-fly movement carries the camera through a floor, wall, or object surface
- **THEN** movement remains unconstrained and no player collision or physics response occurs

### Requirement: Depth-correct opaque visibility
The prototype scene SHALL render opaque surfaces with depth testing so that the nearest visible surface wins independently of geometry submission order.

#### Scenario: Geometry overlaps in screen space
- **WHEN** two scene surfaces project onto the same framebuffer region at different depths
- **THEN** the nearer surface obscures the farther surface
