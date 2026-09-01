# scene-texturing Specification

## Purpose

Defines the fixed tileable surface textures and deterministic mapping used to make the built-in opaque FPS prototype scene visually readable without introducing a general material or asset system.

## Requirements

### Requirement: Fixed prototype surface texture set
The built-in prototype level SHALL assign every solid exactly one texture role from a fixed set containing floor, boundary, obstacle, and shooting-target surfaces. Every role SHALL resolve to a distinct packaged opaque texture, and the assignment SHALL remain immutable for the run.

#### Scenario: Prototype level assigns surface roles
- **WHEN** the immutable prototype level is constructed
- **THEN** every floor, boundary, obstacle, movement-test structure, and shooting target has one valid fixed texture role

#### Scenario: Movement-test geometry is assigned
- **WHEN** the walkable step or low-clearance structure is prepared for rendering
- **THEN** it uses the fixed obstacle surface texture role without requiring another material definition

### Requirement: Deterministic tiled texture coordinates
Every generated prototype-solid face and terrain triangle SHALL carry finite texture coordinates with a consistent world-space texel density, SHALL map shared surface positions continuously, and SHALL repeat rather than stretch the full texture across differently sized surfaces.

#### Scenario: One planar box face is generated
- **WHEN** a prototype solid is expanded into the two triangles of one face
- **THEN** their shared positions have matching texture coordinates and the complete face has a continuous orientation-correct mapping

#### Scenario: Terrain cell is generated
- **WHEN** one terrain cell is expanded into its two triangles
- **THEN** their shared positions have matching floor texture coordinates derived from world-space horizontal position

#### Scenario: Differently sized surfaces are generated
- **WHEN** a large terrain region and a smaller obstacle face use the same texture role
- **THEN** their texture coordinates preserve the configured world-space scale and the larger surface contains more repetitions

### Requirement: Filtered opaque texture sampling
Prototype textures SHALL repeat outside normalized coordinates, SHALL provide a complete mip chain, and SHALL use filtered sampling appropriate to an opaque static 3D scene. Texture alpha SHALL NOT introduce transparent prototype surfaces.

#### Scenario: Textured surface recedes from the camera
- **WHEN** a tileable surface occupies progressively fewer screen pixels
- **THEN** sampling can use progressively smaller mip levels rather than relying only on the full-resolution image

#### Scenario: Texture coordinates exceed one tile
- **WHEN** generated texture coordinates extend outside the normalized texture range
- **THEN** the selected surface texture repeats across the face

### Requirement: Textured spot-lit prototype appearance
Every prototype surface SHALL combine its sampled texture color with its authored tint, the immutable point-light and ambient environment, and any active dynamic spot-light contribution. Inert target plates SHALL use the same ordinary textured-lighting path as other prototype solids without per-target highlight or dimming state.

#### Scenario: Prototype surface is rendered
- **WHEN** a textured prototype solid is visible in a renderable frame
- **THEN** it displays its fixed texture modulated by its authored tint and the bounded point-plus-optional-spot lighting

#### Scenario: Inert plate is rendered
- **WHEN** one of the three textured plates is visible
- **THEN** it retains its authored appearance without requiring mutable target presentation masks
