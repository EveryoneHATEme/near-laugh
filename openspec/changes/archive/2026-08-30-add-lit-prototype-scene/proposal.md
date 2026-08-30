## Why

The prototype scene has depth-correct geometry but renders every surface as an unshaded flat color, so face orientation and spatial form are difficult to read. Adding a small, explicit lighting model now improves the playable prototype while establishing only the rendering data needed by this FPS.

## What Changes

- Add outward-facing normals to the generated prototype-scene vertices.
- Add one immutable directional environment light and an ambient contribution for the built-in level.
- Shade the opaque prototype scene in the fragment stage using its base colors and world-space normals.
- Define and test the CPU/GPU layout used to supply lighting data alongside the camera transform.
- Preserve the current single scene buffer, single opaque draw, Dynamic Rendering path, and depth behavior.
- Keep shadows, textures, general materials, multiple lights, fog, HDR post-processing, and model loading outside this change.

## Capabilities

### New Capabilities

- `scene-lighting`: Defines normal-driven directional and ambient lighting for the built-in opaque prototype scene.

### Modified Capabilities

None.

## Impact

- Affects prototype geometry expansion, the scene vertex format, graphics-pipeline input and push-constant layouts, prototype shaders, shader packaging, and renderer smoke coverage.
- Adds a small backend-neutral lighting description to the immutable prototype level; it does not alter physics geometry or gameplay-facing APIs.
- Introduces no new third-party dependency and no rendering architecture such as a render graph, material framework, or descriptor system.
