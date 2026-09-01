## MODIFIED Requirements

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
