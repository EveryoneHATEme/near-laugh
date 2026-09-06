# prototype-scene Specification

## Purpose

Defines the packaged prototype environment and authored scene inputs used to validate traversal, collision, static asset placement, lighting and rendering for this game.

## Requirements

### Requirement: Built-in static scene
Without an explicit level selection, the executable SHALL present the deterministic packaged prototype scene. Rendering and physics SHALL consume one immutable description of the selected level's optional terrain, finite solid structures, static placements with authored collision boxes, switch, and door definitions. The packaged prototype fixture SHALL include a sculpted ground surface, enclosing or boundary geometry, multiple objects with visibly distinct textured appearances and overlapping depth from the initial player pose, one walkable low step, one low-clearance structure permitting crouched but not standing passage, and at least one recognizable imported prop. These fixture contents SHALL NOT be prerequisites for validating other authored levels. A selected interior SHALL require only its saved level, shaders and referenced packaged models/materials; collision SHALL remain in the authored level without a separate collision asset.

#### Scenario: Prototype scene starts
- **WHEN** the default packaged level and its runtime, physics, model, and renderer initialize successfully
- **THEN** the first rendered player-camera frame shows non-planar ground, generated structures, and one recognizable imported static prop at different depths rather than only engine-generated or clip-space geometry

#### Scenario: Scene collision is constructed
- **WHEN** the immutable selected level is supplied to rendering and physics initialization
- **THEN** its visible and collidable terrain when present and its solid structures derive from matching level data, and each imported placement receives its declared simple static boxes and door collision retains its authored/run-local separation

#### Scenario: Scene assets are packaged
- **WHEN** the executable is copied or launched from its executable-relative runtime layout
- **THEN** the packaged prototype and interior acceptance levels, selected model/material resources, and collision descriptions remain available without the raw source pack or a separate collision asset

#### Scenario: Authored interior omits prototype fixtures
- **WHEN** a valid interior contains structural floors and walls but no terrain or low-clearance movement-test fixture
- **THEN** it loads, renders, and supplies collision without requiring those omitted prototype contents

### Requirement: Built-in imported static prop
The immutable selected-level description SHALL declare zero through 128 static model placements as defined by authored-scene-assets. Each placement SHALL use a stable identity, finite translation and yaw, positive finite uniform scale, a known packaged model identity, and independently authored local box proxies. Placements SHALL remain fixed for the run and SHALL carry no health, interaction, animation or dynamic-body state. The packaged prototype SHALL retain its recognizable legacy chair appearance and collision after migration.

#### Scenario: Prototype level is constructed
- **WHEN** the built-in prototype level validates successfully
- **THEN** it exposes the authored valid static placements and boxes without containing a filesystem path or model-library type

#### Scenario: Imported prop is inspected in play
- **WHEN** the player views or approaches a collidable prop
- **THEN** the loaded model remains at its authored world placement and the player is blocked by its declared simple static collision volume

### Requirement: Camera-driven scene inspection
Every renderable frame SHALL depict the selected static scene from the current collision-constrained first-person player camera frame, initially using the resolved entry pose. Static level collision SHALL prevent the player from crossing terrain when present, solid floors, boundaries, and obstacles. Where authored, walkable steps and low-clearance passages SHALL retain their existing traversal behavior.

#### Scenario: Camera pose changes
- **WHEN** grounded movement, jumping, gravity, crouching, or look input changes the player camera position or orientation
- **THEN** the next rendered frame depicts the same selected static scene from the updated collision-constrained pose

#### Scenario: Camera crosses geometry
- **WHEN** requested player movement would carry the camera through a terrain surface, floor, wall, or blocking object surface
- **THEN** collision prevents the player camera from crossing the corresponding solid structure

#### Scenario: Player uses movement-test geometry
- **WHEN** the player approaches an authored terrain slope, the prototype low step while standing, or the prototype low-clearance route while crouched
- **THEN** the visible environment provides matching collision that permits the intended traversal

#### Scenario: Player changes floors
- **WHEN** the player walks between the apartment and lower landing using the acceptance scene's stairs
- **THEN** the camera follows the grounded player continuously through the same connected authored scene

### Requirement: Depth-correct opaque visibility
The selected scene SHALL render opaque surfaces with depth testing so that the nearest visible surface wins independently of geometry submission order.

#### Scenario: Geometry overlaps in screen space
- **WHEN** two scene surfaces project onto the same framebuffer region at different depths
- **THEN** the nearer surface obscures the farther surface
