## Context

The prototype level currently describes only axis-aligned boxes. Rendering expands every box into vertices for one opaque draw, while physics creates matching static box collision. See proposal.md for the motivation and the terrain requirements delta for the required behavior.

## Goals / Non-Goals

**Goals:**

- Add a concrete authorable terrain data shape that can later be edited as height samples.
- Keep visible terrain, collision, texture mapping, lighting, and spawn placement derived from one level description.
- Preserve the current Vulkan pipeline, one combined scene draw, fixed texture array, and main-thread physics model.

**Non-Goals:**

- Do not introduce a level editor, file format, runtime terrain editing, or serialization boundary.
- Do not support caves, overhangs, holes, destructible terrain, texture painting, terrain LOD, streaming, or arbitrary terrain materials.
- Do not generalize the prototype world into a reusable scene framework.

## Decisions

### A finite uniform height grid is the terrain source of truth

Add a concrete terrain member directly to the prototype-level description: an origin, fixed X/Z sample counts, one positive sample spacing, and an ordered height array. The initial authored patch is a 48 m by 48 m area sampled every 0.5 m (97 by 97 samples), with gentle hills, slopes, and a depression. Its data remains immutable for the process lifetime.

This mirrors the core editable value of a Unity-style terrain: a sculpting operation changes height samples. It is intentionally not an editor-facing interface or an asset format. A triangle soup would permit arbitrary shape now but would not supply the regular editable data the future editor needs; a voxel representation would add unsupported cave and destruction concerns.

### Render the terrain into the existing combined vertex buffer

Generate two consistently wound triangles for every terrain grid cell and append them beside the existing box triangles before the renderer creates its one vertex buffer. Generate a normalized upward-facing geometric normal per terrain triangle and derive UVs from world X/Z coordinates so the existing floor texture repeats once per metre. The existing vertex layout, texture layer, descriptor sets, shaders, and single draw remain unchanged.

Flat per-triangle normals keep generation simple and satisfy the existing lighting contract. Smoothed terrain normals can be considered later as a visual-only refinement without changing the heightfield data model.

### Use matching static heightfield collision

Build one static Jolt heightfield collision shape from the same terrain origin, spacing, dimensions, and samples, alongside the existing static boxes. The initial terrain data is validated for finite values and for every playable triangle's slope being at or below the current player supported-slope limit. Boundary solids extend below the minimum terrain elevation so terrain depressions cannot create a route underneath the enclosure.

Jolt's heightfield is selected over a manually maintained collision triangle list because the data is already a regular ground grid and has no holes or overhangs. Existing boundary, obstacle, step, roof, and plate boxes continue to use their current collision representation.

### Resolve spawn elevation from terrain

Keep the authored spawn horizontal position and yaw, but obtain its foot elevation from the terrain surface. Validation checks that the spawn lies within a walkable terrain cell and that the standing player volume does not overlap a blocking box. This prevents the camera and collision character from beginning above or below the newly sculpted ground.

### Maintain a readable ambient baseline

Raise the immutable environment ambient contribution from 0.03 to 0.12 and raise its authoring cap from 0.08 to 0.20. The two existing authored point lights, their world positions, and the optional player flashlight stay unchanged. This lifts unlit terrain and structure detail enough to make the prototype understandable while preserving local-light contrast and the existing bounded diffuse shader path.

## Risks / Trade-offs

- [Heightfield cannot represent caves or overhangs] -> Keep the terrain capability explicitly limited to a single-valued ground surface; add separate authored mesh features only when gameplay needs them.
- [Terrain triangles increase the combined vertex count] -> Start with the bounded 97 by 97 sample grid and preserve one startup upload and draw; measure before adding LOD or batching.
- [Terrain diagonals can make movement feel uneven] -> Use a consistent cell diagonal, gentle authoring, the current slope limit, and deterministic character movement tests across both triangle directions.
- [Render and collision could drift] -> Generate both from the same immutable terrain description and test sample positions, extents, and player grounding.
- [Static physics shape lifetime differs from box bodies] -> Keep the heightfield owner contained in the existing physics implementation and release it before the physics system.
- [Higher ambient can reduce local-light contrast] -> Use the modest 0.12 baseline and retain both authored point lights unchanged; verify the environment-light contract and Vulkan smoke test.

## Migration Plan

1. Replace the prototype's flat floor box with the authored terrain while retaining all non-ground solids.
2. Update deterministic world, rendering, physics, and source-boundary tests; run the Vulkan smoke path to verify validation-clean lifetime behavior.
3. Roll back by restoring the flat floor box and removing the terrain collision/vertex generation; no saved user data or compatibility migration is involved.
