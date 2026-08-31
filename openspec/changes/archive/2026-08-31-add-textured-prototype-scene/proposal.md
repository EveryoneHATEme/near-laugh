## Why

The prototype scene now validates lit opaque geometry and shooting feedback, but its flat per-vertex colors leave every surface visually synthetic and do not exercise the Vulkan image, sampler, descriptor, or texture-coordinate path required by the FPS. Adding a small fixed set of tileable prototype textures establishes that graphics foundation while the scene still fits one immutable draw.

## What Changes

- Add a fixed set of packaged tileable PNG textures for the prototype floor, boundaries, obstacles, and shooting targets.
- Extend the immutable prototype-level surface description and generated vertices with deterministic texture selection and UV coordinates.
- Decode the fixed PNG assets during renderer startup and upload them as one device-local, mipmapped 2D texture array.
- Add one immutable combined image-sampler descriptor and sample the selected texture layer in the existing opaque scene draw.
- Combine sampled surface color with the existing authored tint, directional and ambient lighting, and target highlight/dim presentation.
- Preserve one immutable scene vertex buffer, one opaque scene draw, executable-relative resources, explicit Vulkan ownership, and swapchain-independent texture lifetime.
- Keep general materials, runtime asset discovery or streaming, model loading, normal/roughness/metallic maps, transparency, texture compression, descriptor indexing, bindless rendering, and hot reload outside this change.

## Capabilities

### New Capabilities

- `scene-texturing`: Defines the fixed texture set, deterministic surface mapping and UV tiling, sampled-color composition, and packaged texture behavior for the built-in prototype scene.

### Modified Capabilities

- `prototype-scene`: Changes the built-in scene from shader-only colored geometry to geometry that requires packaged surface textures and carries explicit surface texturing data.
- `scene-lighting`: Changes opaque base color from authored vertex color alone to sampled texture color modulated by the authored tint before existing lighting and target presentation are applied.
- `runtime-composition`: Adds the required prototype texture assets to explicit executable-relative runtime resource resolution and startup failure reporting.
- `vulkan-renderer`: Adds texture image upload, mipmaps, a sampler and immutable descriptor, textured vertex/shader inputs, and swapchain-independent texture ownership to the existing single-draw renderer.

## Impact

- Affects prototype-level surface data, procedural box vertex generation and layout, runtime resource resolution and packaging, scene shaders and committed SPIR-V, Vulkan startup uploads, pipeline layout and descriptor ownership, renderer cleanup and failure paths, deterministic layout/resource tests, and Vulkan smoke validation.
- Adds one pinned image-decoding dependency restricted to the packaged PNG texture path; decoder details remain private to the renderer/resource implementation.
- Adds fixed raster assets beneath the executable-relative resource tree without introducing a generic asset manager or exposing Vulkan or decoder types outside `near_laugh_render`.
