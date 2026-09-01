## Why

The prototype's global directional light and substantial ambient fill make every surface continuously readable, which works for a shooting range but cannot establish the pools of visibility and surrounding darkness needed by the game's new ambient-horror direction. The renderer now has textured opaque geometry, so a small authored local-lighting model is the next concrete visual foundation.

## What Changes

- Replace the built-in scene's global directional-light contribution with a small fixed set of level-authored point lights.
- Give each local light an explicit world position, color, intensity, and finite radius, with validation that rejects invalid authored values before rendering begins.
- Evaluate bounded distance attenuation and surface orientation per fragment so illuminated areas fall smoothly into darkness outside each light's influence.
- Reduce the scene-wide ambient contribution to a fixed near-black visibility floor rather than guaranteeing that every valid surface remains clearly readable.
- Extend the opaque scene vertex/shader contract with the world-space position needed for local lighting while preserving the existing textured, depth-tested, single-draw scene.
- Author the prototype lights and scene presentation to demonstrate a dim spawn area, an inviting destination light, and a dark transition between them.
- Keep the existing rifle and shooting-target behavior as temporary prototype scaffolding; this change does not add, expand, or remove combat gameplay.
- Exclude flashlights, shadows, animated or scripted light changes, fog, exposure adaptation, HDR post-processing, emissive materials, arbitrary light loading, and a general lighting framework.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `scene-lighting`: Replace the immutable directional-light model with a fixed authored local-point-light model and intentional darkness.
- `vulkan-renderer`: Render the existing opaque textured scene with world-position-aware local lighting while preserving the direct Vulkan ownership, frame, and draw boundaries.

## Impact

- Affects prototype-level lighting data and validation, the vertex-to-fragment shader interface, GPU lighting data, prototype scene shaders and committed SPIR-V, graphics-pipeline resources, deterministic rendering tests, and Vulkan smoke validation.
- Keeps the current single opaque scene vertex buffer, texture-array descriptor, depth path, and draw call; it does not add a render graph, deferred renderer, generic light registry, or new dependency.
- Builds on the completed `add-textured-prototype-scene` change and assumes its texture sampling and descriptor path remain present.
