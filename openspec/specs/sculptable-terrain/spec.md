# sculptable-terrain Specification

## Purpose

Defines the bounded sculptable ground surface used by the built-in game level while preserving one shared source for rendering and collision.

## Requirements

### Requirement: Immutable heightfield terrain description
An authored level SHALL contain zero or one immutable terrain surface. A present surface SHALL use the existing finite rectangular X/Z grid of height samples with positive uniform sample spacing. Each terrain cell SHALL form a continuous ground surface from two opaque triangles. Terrain SHALL have no holes, caves, overhangs, or runtime modifications. The packaged prototype SHALL retain its one terrain surface; a terrain-free interior SHALL NOT synthesize a hidden heightfield.

#### Scenario: Prototype terrain is created
- **WHEN** the packaged prototype level is constructed
- **THEN** it exposes one bounded terrain with a deterministic horizontal extent and a height at every grid sample

#### Scenario: Terrain is inspected at a cell boundary
- **WHEN** adjacent terrain cells are rendered or used for collision
- **THEN** their shared edge has matching world-space positions and does not create a gap

#### Scenario: Interior omits terrain
- **WHEN** a valid interior document has no terrain
- **THEN** neither visible terrain triangles nor terrain collision are created and structural floors provide the authored walkable surfaces

### Requirement: Terrain-derived render and collision surfaces
The terrain's visible surface and its static player-collision surface SHALL be derived from the same terrain origin, sample spacing, dimensions, and height samples. Terrain triangles SHALL provide finite world-space positions and finite unit normals to the existing opaque scene path.

#### Scenario: Player reaches a terrain feature
- **WHEN** the player walks, jumps, or falls onto a visible terrain slope or depression
- **THEN** collision is evaluated against the matching terrain surface rather than a separate flat floor

#### Scenario: Terrain is rendered
- **WHEN** a terrain triangle is visible in a renderable frame
- **THEN** it is depth-tested and lit by the existing opaque scene path using its world-space surface data

### Requirement: Terrain validation and traversable spawn
When terrain exists, its dimensions, sample spacing, and height samples SHALL be finite and valid before renderer or physics initialization, and every playable terrain triangle SHALL satisfy the player's supported-slope limit. Entry positions SHALL use the shared level support and clearance validation and SHALL be allowed on authored structural floors even when terrain is present below them or does not cover their horizontal position. Missing terrain SHALL NOT itself cause a validation error or suppress validation of structural entry support.

#### Scenario: Invalid terrain data is supplied
- **WHEN** a present terrain has non-finite samples, invalid dimensions or spacing, or an unsupported playable slope
- **THEN** level validation rejects it before runtime initialization

#### Scenario: Player starts on terrain
- **WHEN** a valid level starts at an entry authored on terrain
- **THEN** the player's foot position begins at that terrain-supported entry

#### Scenario: Player starts on a floor above terrain
- **WHEN** a valid entry is authored on an upper solid floor with terrain below
- **THEN** the entry remains at its authored floor height and does not have to match the terrain height
