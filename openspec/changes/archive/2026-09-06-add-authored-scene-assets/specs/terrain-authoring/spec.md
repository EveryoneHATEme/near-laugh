## MODIFIED Requirements

### Requirement: Heightfield-only authoring boundary
Terrain brushes SHALL modify only height samples in the single bounded heightfield and SHALL preserve all authored terrain/solid material assignments and unrelated props and doors. The separate terrain material property SHALL choose one packaged opaque structural material for the whole terrain and SHALL NOT become a texture-paint brush. It SHALL NOT create holes, caves, overhangs, additional terrain surfaces, voxel data, texture paint, procedural or erosion output, or runtime-deformable terrain.

#### Scenario: Terrain tools are inspected
- **WHEN** the terrain-authoring controls and saved level data are inspected
- **THEN** brushes expose only heightfield operations, any separate material choice applies uniformly, and the saved heightfield layout remains bounded
