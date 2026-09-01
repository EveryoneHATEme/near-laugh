## Purpose

Defines the fixed tileable surface textures and deterministic mapping used to make the built-in opaque FPS prototype scene visually readable without introducing a general material or asset system.

## ADDED Requirements

### Requirement: Fixed prototype surface texture set
The built-in prototype level SHALL assign every solid exactly one texture role from a fixed set containing floor, boundary, obstacle, and shooting-target surfaces. Every role SHALL resolve to a distinct packaged opaque texture, and the assignment SHALL remain immutable for the run.

#### Scenario: Prototype level assigns surface roles
- **WHEN** the immutable prototype level is constructed
- **THEN** every floor, boundary, obstacle, movement-test structure, and shooting target has one valid fixed texture role

#### Scenario: Movement-test geometry is assigned
- **WHEN** the walkable step or low-clearance structure is prepared for rendering
- **THEN** it uses the fixed obstacle surface texture role without requiring another material definition

### Requirement: Deterministic tiled texture coordinates
Every generated prototype-solid face SHALL carry finite texture coordinates with a consistent world-space texel density, SHALL map both triangles of the face continuously, and SHALL repeat rather than stretch the full texture across differently sized surfaces.

#### Scenario: One planar box face is generated
- **WHEN** a prototype solid is expanded into the two triangles of one face
- **THEN** their shared positions have matching texture coordinates and the complete face has a continuous orientation-correct mapping

#### Scenario: Differently sized surfaces are generated
- **WHEN** a large floor face and a smaller obstacle face use the same texture role
- **THEN** their texture coordinates preserve the configured world-space scale and the larger face contains more repetitions

### Requirement: Filtered opaque texture sampling
Prototype textures SHALL repeat outside normalized coordinates, SHALL provide a complete mip chain, and SHALL use filtered sampling appropriate to an opaque static 3D scene. Texture alpha SHALL NOT introduce transparent prototype surfaces.

#### Scenario: Textured surface recedes from the camera
- **WHEN** a tileable surface occupies progressively fewer screen pixels
- **THEN** sampling can use progressively smaller mip levels rather than relying only on the full-resolution image

#### Scenario: Texture coordinates exceed one tile
- **WHEN** generated texture coordinates extend outside the normalized texture range
- **THEN** the selected surface texture repeats across the face

### Requirement: Textured prototype appearance
Each unaffected prototype surface SHALL combine its sampled texture color with its authored tint and the existing environment lighting. Target highlight and destroyed-dim presentation SHALL remain visibly dominant when active without changing the selected texture role or texture coordinates.

#### Scenario: Unaffected surface is rendered
- **WHEN** a textured prototype solid is absent from the highlighted and dimmed masks
- **THEN** it displays its fixed texture modulated by its authored tint and directional-plus-ambient lighting

#### Scenario: Textured target presentation changes
- **WHEN** a target becomes highlighted or persistently dimmed
- **THEN** the existing presentation treatment is applied to its textured appearance without changing immutable geometry or surface assignment
