## Context

See `proposal.md` for motivation. The renderer currently expands every axis-aligned `PrototypeSolid` into non-indexed world-space triangles containing position and packed color, uploads the resulting immutable stream once, and draws it with one opaque depth-tested pipeline. A 64-byte camera matrix is the pipeline's only push constant, and the fragment shader returns interpolated vertex color unchanged.

The level is immutable, all geometry is world-space, and there are no per-object transforms or dynamic lights. The solution must preserve the direct Vulkan 1.3 architecture, Dynamic Rendering, Synchronization 2, explicit ownership, and the existing single-draw prototype path.

## Goals / Non-Goals

**Goals:**

- Make planar orientation and object volume readable with deterministic diffuse and ambient shading.
- Keep light authoring backend-neutral and colocated with the immutable prototype environment.
- Keep the CPU/GPU lighting contract small, explicit, and testable.
- Preserve the current renderer lifetime, frame synchronization, scene-buffer ownership, and draw count.

**Non-Goals:**

- Specular or physically based material response.
- Shadow maps, image descriptors, textures, samplers, model loading, fog, HDR targets, or post-processing.
- Runtime-editable lights, point or spot lights, per-object transforms, or a general scene/material abstraction.
- A render graph, bindless resources, extra queues, or asynchronous compute.

## Decisions

### Generate explicit world-space normals with prototype vertices

The prototype vertex format will add a three-component floating-point normal. Box expansion will assign one constant axis-aligned outward normal to all six vertices of each face. World-space normals require no transform or inverse-transpose matrix because the generated scene has no per-object transforms.

Floating-point normals favor clarity over packing; the complete prototype stream is tiny and no measurement justifies a compressed format. Deriving normals from screen-space derivatives was rejected because it makes the geometry contract implicit and is less useful to later opaque mesh work. Converting the scene to indexed geometry is unrelated to lighting and remains out of scope.

### Author one environment light in `PrototypeLevel`

The immutable world description will own a small backend-neutral environment-light value containing a direction-to-light vector, directional intensity, and ambient intensity. Construction validation will require finite values, a normalized non-zero direction, and bounded non-negative intensities. The renderer consumes this value; physics ignores it.

This follows the existing prototype choice to colocate structural dimensions and display colors in the level description. Hard-coding the light only in GLSL was rejected because it would hide level authoring data inside a compiled resource. Adding a general light collection was rejected because the prototype needs exactly one environment light.

### Pass static lighting beside the camera through push constants

The scene draw will use one standard-layout push-constant payload containing the existing column-major view-projection matrix plus aligned directional and ambient values. The payload will remain below Vulkan's guaranteed 128-byte minimum push-constant capacity. Its range will be visible to both vertex and fragment stages: the vertex stage consumes the matrix, while lighting values reach the fragment stage.

Push constants preserve the descriptor-free, one-draw design and are appropriate for less than 128 bytes submitted once per frame. A uniform buffer and descriptor set were rejected because they add allocation, update, binding, and lifetime machinery without serving another requirement. Shader literals were rejected because the level owns the light.

### Use a bounded Lambert diffuse term plus ambient fill

The vertex shader will forward base color and the face normal. The fragment shader will normalize the interpolated normal, compute a clamped Lambert term against the direction to the light, combine it with the configured ambient contribution, and modulate the opaque base color. Lighting factors will be selected so the built-in colors remain distinguishable without requiring tone mapping.

No specular term is included because the prototype has no view position, roughness, or material representation. Those inputs should arrive with a later concrete material change rather than being approximated here. The existing sRGB surface-format preference and base-color convention remain unchanged; a broader color-management pipeline is not introduced by this change.

### Validate data contracts deterministically and appearance through Vulkan smoke coverage

CPU tests will verify vertex stride/attributes, outward unit normals for all six box faces, a valid immutable light, and a push-constant layout below the required size limit. Shader paths and packaged SPIR-V remain covered by resource tests. The Vulkan smoke run will exercise pipeline creation, rendering, forced swapchain recreation, shutdown, injected failure cleanup, and validation diagnostics with the expanded layout.

Pixel-reference tests are not introduced: cross-device rasterization and presentation differences make them brittle for this milestone. Deterministic geometry/layout tests plus a validation-clean smoke run cover the new contracts, while the executable provides the practical visual check required by the development workflow.

## Risks / Trade-offs

- [Incorrect face winding or normal sign makes surfaces shade backwards] -> Test every face normal against the solid center and authored bounds, and inspect the running prototype.
- [C++ and GLSL push-constant layout diverges] -> Use explicitly aligned standard-layout fields, compile-time offset/size assertions, vertex/push-layout tests, and Vulkan validation.
- [Diffuse lighting clips colors or leaves back-facing surfaces too dark] -> Keep intensities bounded, retain a non-zero ambient term, and tune only the immutable prototype light values.
- [Adding light data to the world increases rendering influence on `PrototypeLevel`] -> Keep the value concrete and environment-specific; do not introduce renderer handles, Vulkan types, inheritance, or a generic light hierarchy.
- [The flat Lambert model looks stylized rather than realistic] -> Accept it as the smallest useful lighting baseline; textures, shadows, and richer materials remain separate measured milestones.

## Migration Plan

Update the level lighting value, generated vertex format, pipeline layout, GLSL sources, and committed SPIR-V assets as one atomic change so no old/new CPU-GPU layout combination is runnable. Existing external runtime APIs and saved data require no migration. Rollback consists of reverting those files together; no persistent state is changed.
