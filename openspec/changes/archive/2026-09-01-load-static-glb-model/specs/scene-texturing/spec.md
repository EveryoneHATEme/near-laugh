## ADDED Requirements

### Requirement: Fixed imported-prop appearance
The imported static prop SHALL preserve its finite `TEXCOORD_0` values, SHALL use opaque white vertex tint and the fixed obstacle surface texture, and SHALL be sampled through the same immutable repeating, filtered texture array as generated prototype geometry. File-defined glTF materials, images, samplers, vertex colors, and texture transforms SHALL NOT change its appearance.

#### Scenario: Imported prop is rendered
- **WHEN** a fragment from the loaded model is shaded
- **THEN** it samples the obstacle texture at the model-provided coordinates and combines that color with the existing scene lighting path

#### Scenario: Model contains material metadata
- **WHEN** the otherwise-supported GLB associates its primitive with material or texture metadata
- **THEN** the prototype prop retains the fixed obstacle-texture appearance without loading or binding the referenced material resources

