## Why

The prototype environment is built from a flat floor box, so it cannot test or present the hills, slopes, and depressions needed by the intended FPS spaces. A small authored terrain representation should establish the data shape a future level editor can sculpt without introducing editor tooling or a general asset system now.

## What Changes

- Add one finite, immutable, grid-based heightfield terrain to the built-in prototype level.
- Derive the terrain's opaque triangles, normals, tiled floor appearance, and static physics collision from the same authored height samples.
- Keep existing box solids for boundaries, obstacles, the walkable step, the low-clearance structure, and inert plates.
- Validate terrain bounds, finite samples, spawn placement, and player-traversable slopes before runtime initialization.
- Keep terrain static for a run and retain the existing single opaque scene draw and fixed texture set.
- Raise the immutable environment's ambient baseline so the prototype terrain and structures remain readable away from the two authored local lights.
- Explicitly exclude terrain editor UI, terrain-file loading/saving, runtime sculpting, caves/overhangs, texture painting, and general materials.

## Capabilities

### New Capabilities

- `sculptable-terrain`: Defines the bounded heightfield terrain data and its shared render/collision derivation for the built-in FPS level.

### Modified Capabilities

- `prototype-scene`: The built-in scene gains terrain while retaining its required static solid structures and collision-constrained traversal.
- `physics-simulation`: Static world collision gains terrain collision derived from the level's height samples.
- `scene-texturing`: Terrain receives deterministic world-scaled tiled floor texture coordinates.
- `scene-lighting`: Terrain triangles provide outward-facing normals and world positions to the existing opaque lighting path.

## Impact

- Affects `PrototypeLevel`, prototype-scene vertex generation, static Jolt collision creation, spawn validation, and the corresponding deterministic tests.
- Reuses the existing Vulkan pipeline, single scene vertex buffer/draw model, fixed texture array, two authored local lights, and contained Jolt dependency; no public API or additional dependency is introduced.
