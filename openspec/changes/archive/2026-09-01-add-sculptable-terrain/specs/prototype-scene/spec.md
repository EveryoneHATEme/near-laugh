## MODIFIED Requirements

### Requirement: Built-in static scene
The executable SHALL present a deterministic built-in scene composed from one immutable prototype-level description whose finite heightfield terrain and solid structural dimensions are used to derive both opaque world-space renderer geometry and static physics collision. The description SHALL include a sculpted ground surface, enclosing or boundary geometry, multiple objects with visibly distinct textured appearances and overlapping depth from the initial player pose, one walkable low step, and one low-clearance structure that permits crouched but not standing passage. The scene SHALL use only its packaged shader and fixed surface-texture resources and SHALL NOT require a model, general material, collision, or level file.

#### Scenario: Prototype scene starts
- **WHEN** runtime, physics, and renderer initialization succeed
- **THEN** the first rendered player-camera frame shows multiple recognizable textured 3D surfaces at different distances, including non-planar ground, rather than flat-colored or clip-space geometry

#### Scenario: Scene collision is constructed
- **WHEN** the immutable prototype level is supplied to rendering and physics initialization
- **THEN** visible and collidable terrain, boundary, obstacle, step, and low-clearance structures are derived from matching level data

#### Scenario: Scene assets are packaged
- **WHEN** the executable is copied or launched from its executable-relative runtime layout
- **THEN** the built-in scene, fixed surface textures, and collision remain available without external model, general material, scene, or collision assets

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
