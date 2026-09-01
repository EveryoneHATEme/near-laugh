## Purpose

Defines the bounded sculptable ground surface used by the built-in FPS level while preserving one shared source for rendering and collision.

## ADDED Requirements

### Requirement: Immutable heightfield terrain description
The built-in prototype level SHALL contain exactly one immutable terrain surface defined by a finite rectangular X/Z grid of height samples with positive uniform sample spacing. Each terrain cell SHALL form a continuous ground surface from two opaque triangles. The terrain SHALL have no holes, caves, overhangs, or runtime modifications.

#### Scenario: Prototype terrain is created
- **WHEN** the built-in prototype level is constructed
- **THEN** it exposes one bounded terrain with a deterministic horizontal extent and a height at every grid sample

#### Scenario: Terrain is inspected at a cell boundary
- **WHEN** adjacent terrain cells are rendered or used for collision
- **THEN** their shared edge has matching world-space positions and does not create a gap

### Requirement: Terrain-derived render and collision surfaces
The terrain's visible surface and its static player-collision surface SHALL be derived from the same terrain origin, sample spacing, dimensions, and height samples. Terrain triangles SHALL provide finite world-space positions and finite unit normals to the existing opaque scene path.

#### Scenario: Player reaches a terrain feature
- **WHEN** the player walks, jumps, or falls onto a visible terrain slope or depression
- **THEN** collision is evaluated against the matching terrain surface rather than a separate flat floor

#### Scenario: Terrain is rendered
- **WHEN** a terrain triangle is visible in a renderable frame
- **THEN** it is depth-tested and lit by the existing opaque scene path using its world-space surface data

### Requirement: Terrain validation and traversable spawn
Terrain dimensions, sample spacing, and height samples SHALL be finite and valid before renderer or physics initialization. Every terrain triangle that is part of the playable ground SHALL satisfy the player's supported-slope limit, and the configured player spawn SHALL resolve to a supported terrain position without overlapping a blocking static structure.

#### Scenario: Invalid terrain data is supplied
- **WHEN** a terrain has non-finite samples, invalid dimensions or spacing, or an unsupported playable slope
- **THEN** level validation rejects it before runtime initialization

#### Scenario: Player starts on terrain
- **WHEN** a valid terrain level starts
- **THEN** the player's foot position begins on the terrain surface at the configured spawn location
