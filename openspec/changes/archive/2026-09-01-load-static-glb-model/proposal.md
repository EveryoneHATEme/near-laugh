## Why

The prototype world is still limited to engine-generated boxes and terrain, so the renderer cannot consume authored 3D geometry. Loading one controlled static prop now establishes the smallest useful model boundary before later FPS work needs weapon, enemy, or environment assets.

## What Changes

- Add startup loading for one packaged binary glTF 2.0 (`.glb`) static prop through a deliberately narrow, validated asset profile.
- Place the prop in the immutable prototype level with one authored translation, yaw, uniform scale, fixed prototype surface role, and simple static box collision proxy.
- Render the loaded opaque triangles through the existing lit, textured Vulkan scene pipeline as a second immutable mesh draw.
- Resolve and package the model beneath the explicit executable-relative runtime resource root, with actionable startup failures for missing, malformed, or unsupported content.
- Add a pinned, renderer-private `cgltf` dependency for parsing while retaining the existing synchronous startup and explicit ownership model.
- Exclude general asset discovery, multiple model formats, file-defined materials, animation, skinning, morph targets, runtime transforms, mesh collision, streaming, caching, and hot reload.

## Capabilities

### New Capabilities

- `static-model-loading`: Defines the supported packaged GLB profile, deterministic conversion into one immutable world-space opaque mesh, and failure behavior for invalid or unsupported model assets.

### Modified Capabilities

- `prototype-scene`: Adds one visible static model prop and its independently authored collision proxy to the built-in scene.
- `runtime-composition`: Resolves and packages the required model from the explicit runtime resource root.
- `vulkan-renderer`: Uploads and draws the imported static mesh through the existing opaque scene pipeline with explicit GPU ownership.
- `scene-texturing`: Applies imported texture coordinates and one fixed prototype surface texture to the model without introducing file-defined materials.
- `scene-lighting`: Lights imported world-space positions and normals through the existing ambient, point-light, and optional spot-light path.
- `physics-simulation`: Builds matching simple static collision for the model prop from the prototype-level proxy rather than render triangles.

## Impact

- Affects runtime resource resolution and packaging, `PrototypeLevel`, renderer-private CPU geometry loading, opaque mesh buffer ownership and draw recording, static Jolt collision construction, shaders' existing vertex contract consumers, and deterministic/Vulkan smoke tests.
- Adds one pinned MIT-licensed parsing dependency to `near_laugh_render`; no dependency or model type leaks through the public runtime facade.
- Changes the prototype scene from one generated geometry draw to one generated-world draw plus one imported-prop draw while retaining one graphics pipeline, the fixed texture array, and synchronous main-thread startup.
