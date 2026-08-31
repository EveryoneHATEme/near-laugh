## Context

See `proposal.md` for motivation. The renderer currently expands each immutable axis-aligned prototype solid into one host-visible non-indexed vertex buffer, binds a descriptor-free graphics pipeline, sends a 112-byte camera/light/presentation push constant, and issues one depth-tested opaque draw. Runtime resource resolution knows only the two scene shaders. The Vulkan context exposes the selected physical device, logical device, graphics queue, and graphics queue-family index, but there is no sampled-image upload path or startup transfer command path.

The change must preserve direct Vulkan 1.3, Dynamic Rendering, Synchronization 2, backend containment, explicit executable-relative resources, target highlighting/dimming, and swapchain-safe ownership. It must not turn the fixed prototype level into a general material, asset, or scene system.

## Goals / Non-Goals

**Goals:**

- Render four visibly distinct, tileable prototype surface textures through the existing opaque scene draw.
- Establish explicit and failure-safe ownership for one immutable sampled texture array and its descriptor resources.
- Produce deterministic per-face UVs with a common world scale and deterministic surface-layer selection.
- Keep texture resources independent of swapchain extent and format recreation.
- Make asset decoding, CPU/GPU layouts, mip calculation, resource resolution, and Vulkan lifetime behavior testable at their existing boundaries.

**Non-Goals:**

- A material class, material file format, texture cache, asset registry, streaming uploader, background loading, or hot reload.
- Per-instance or per-frame texture mutation, arbitrary texture counts, descriptor indexing, bindless resources, or multiple scene draws.
- Model UV import, normal/roughness/metallic maps, transparency, compression, anisotropic filtering, or texture authoring tools.
- Replacing the existing authored tints, lighting policy, target masks, or push-constant camera path.

## Decisions

### Use four fixed surface roles in the immutable prototype level

Add a small `PrototypeSurface` value with exactly `Floor`, `Boundary`, `Obstacle`, and `ShootingTarget` roles, and store one role on each `PrototypeSolid` beside its existing kind and tint. Floor, boundary, and shooting-target kinds map to their matching surface roles; ordinary obstacles, the walkable step, and the low-clearance structure use the obstacle role. Validate every role and keep the role order stable so it maps directly to texture-array layers.

Surface role remains separate from `PrototypeSolidKind`: kind continues to describe structural/gameplay purpose, while surface describes the fixed rendered appearance. A switch inside the renderer based on gameplay-oriented kinds was rejected because it would make rendering infer appearance. A string material name or arbitrary material identifier was rejected because four immutable prototype surfaces do not justify a material lookup model.

### Package four equal 256-by-256 tileable PNG images

Place one opaque RGBA PNG per fixed surface role beneath `resources/textures`. Resolve all four paths eagerly from the existing explicit resource root and require each file before renderer construction. Decode each to RGBA8 and reject decode failure, non-positive dimensions, dimensions other than 256 by 256, or a decoded byte count that cannot be represented safely.

Use a pinned revision of `stb_image` compiled in one private renderer source file with PNG as the only enabled decoder and forced RGBA output. Decoder declarations and allocations remain behind an engine-owned decoded-image value. A custom PNG decoder was rejected as unrelated complexity; a platform image API was rejected because it would add platform-specific behavior; raw RGBA files were rejected because their dimensions and inspection workflow would be awkward.

### Generate world-scaled UVs per box face

Extend the scene vertex with two floating-point texture coordinates and one unsigned texture-layer value. Generate each face from a deliberate pair of local face axes, assigning one texture repetition per world metre. Both triangles share identical coordinates at shared corners, and opposing faces use a consistent outward-viewed orientation. The texture layer is flat through the vertex/fragment interface so a primitive never interpolates between layers.

Local face coordinates were chosen over projecting all geometry from global world axes because each isolated box should have deterministic seams at its own edges and should not change appearance when moved. Mapping every face from zero to one was rejected because large floors would visibly stretch the same image used by small targets.

### Upload one sRGB 2D texture array with a complete mip chain

Decode the four images into one contiguous staging payload in stable surface-role order. Create one device-local `VK_FORMAT_R8G8B8A8_SRGB` 2D-array image with four layers, the full mip count derived from 256 pixels, and transfer-source, transfer-destination, and sampled usage. Before allocation, verify that optimal-tiling format support covers sampling, linear filtering, and the required blit operations.

Use one short-lived graphics-family command pool, primary command buffer, and fence during renderer startup. Transition every base layer from undefined to transfer destination, copy the staging payload, generate each smaller mip with Vulkan 1.3 copy/blit commands and Synchronization 2 barriers, then transition every mip and layer to shader-read-only layout. Submit with `vkQueueSubmit2` and wait on the upload fence before releasing staging memory and temporary command resources.

GPU-generated mipmaps keep the packaged assets simple and exercise the Vulkan transfer path needed by later graphics work. Shipping only a base level was rejected because the large tiled floor and distant walls would alias. A permanent general upload context was rejected because no runtime streaming requirement exists.

### Give one concrete sampled-texture owner the composite Vulkan lifetime

Add one concrete renderer-private RAII owner for the prototype texture array. It owns the image, allocation, array view, repeat/linear sampler, descriptor-set layout, one-set descriptor pool, and immutable combined image-sampler descriptor set. Construction accepts the four resolved paths plus existing Vulkan device/queue context and unwinds every partial stage in reverse order. Destruction occurs after the scene pipeline and after pending GPU work is complete.

The sampler uses normalized UVs, repeat addressing, linear minification/magnification, linear mip interpolation, and the complete mip range. Anisotropy stays disabled so device feature selection does not expand in this change.

Keeping the descriptor with the sampled texture lets the descriptor and image lifetime remain stable while a swapchain-format change rebuilds only the graphics pipeline. Folding the image into `GraphicsPipeline` was rejected because current swapchain recovery can recreate that pipeline and would then decode and upload immutable textures unnecessarily. A reusable texture manager was rejected because there is exactly one fixed texture set.

### Bind the immutable descriptor without changing frame data or draw count

The scene pipeline layout gains set 0, binding 0 as one combined image sampler. The pipeline receives the sampled-texture descriptor layout and set as non-owning handles, binds the set with the pipeline, and retains the current push constant unchanged at 112 bytes. No texture or material state enters `FrameRequest`, and no descriptor is updated after startup.

The vertex shader forwards UV and flat texture layer. The fragment shader samples `sampler2DArray`, multiplies sampled RGB by the normalized authored vertex tint, applies highlight before dimming using the existing masks, and finally applies the existing directional-plus-ambient lighting. Alpha remains 1.0 and blending remains disabled. This order preserves current final-hit precedence and orientation-readable target feedback.

Separate descriptors or draws per surface were rejected because they would require draw partitioning or non-uniform descriptor indexing. A texture array keeps one descriptor and one draw while supporting the four concrete appearances.

### Keep texture resources executable-relative and swapchain-independent

Extend the internal runtime-resource bundle with the four resolved texture paths and pass them through the existing renderer resource value. Packaging copies both `resources/shaders` and `resources/textures` beside the FPS, Vulkan smoke, and resource-layout probe executables. The public `RuntimeConfig` remains unchanged.

Create the sampled texture after the Vulkan context and before the graphics pipeline. Swapchain recreation may rebuild swapchain views, depth attachments, and a format-dependent graphics pipeline, but it retains the sampled texture owner and reuses its descriptor handles. Startup errors include the resolved asset path or failed Vulkan operation; no fallback texture silently hides a packaging defect.

## Risks / Trade-offs

- [A texture layer is assigned to the wrong surface] -> Use one stable surface-role enumeration, validate every solid, and test all authored solids and generated vertex layer values.
- [Box faces contain mirrored, discontinuous, or stretched UVs] -> Define explicit axis mappings for all six faces and test corner coordinates, shared triangle vertices, world-scale repetition, and finite values.
- [Mip generation uses an unsupported format operation] -> Query the selected physical device before image creation and fail with a capability-specific diagnostic.
- [A layout transition misses a layer or mip] -> Centralize the exact subresource ranges in the concrete texture upload and exercise normal and injected partial-failure paths under Vulkan validation.
- [Descriptor or texture lifetime crosses pipeline/swapchain teardown incorrectly] -> Own the sampled texture outside the format-dependent pipeline, destroy the pipeline first, and extend lifecycle smoke checks across forced swapchain recreation.
- [sRGB sampling changes the perceived authored colors] -> Treat texture RGB as sRGB, retain the existing tint bytes as the prototype's artistic modulation, and visually inspect all four roles plus target highlight/dim states.
- [The fixed array becomes mistaken for a general material system] -> Keep roles, dimensions, count, descriptor count, and resource names concrete; require a separate OpenSpec change before arbitrary materials or texture counts.
- [Malformed image input expands decoder exposure] -> Enable only PNG decoding, load only project-packaged startup assets, validate dimensions and size arithmetic, and surface failures before the frame loop.

## Migration Plan

Add the decoder dependency, fixed surface data, PNG assets, resource paths, sampled-texture owner, descriptor-enabled pipeline, vertex/shader layout changes, and rebuilt SPIR-V as one atomic change so old and new CPU/GPU layouts cannot mix. No public application API or persistent data changes. Rollback reverts the change together and restores the shader-only resource layout and descriptor-free scene pipeline.
