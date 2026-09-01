## Context

See `proposal.md` for motivation and the delta specs for required behavior. The textured prototype currently sends a 112-byte push constant containing the camera, one directional-light vector/intensity, one ambient scalar, and target presentation masks. The fragment shader has a world-space normal but not the interpolated world-space position required for point-light distance and direction. The texture array is an immutable renderer-lifetime descriptor at set 0, and the format-dependent graphics pipeline is recreated with the swapchain while the sampled texture survives recreation.

The change must preserve direct Vulkan 1.3, Dynamic Rendering, Synchronization 2, explicit ownership, the immutable built-in level, two frames in flight, one texture descriptor, and one opaque scene draw. The local lights are immutable for this milestone, but their data must remain level-authored rather than compiled into GLSL.

## Goals / Non-Goals

**Goals:**

- Represent exactly two concrete point lights and one near-black ambient scalar in the immutable prototype level.
- Provide those lights to fragment shading once at renderer initialization with explicit, failure-safe Vulkan ownership.
- Produce bounded, radius-limited diffuse lighting from world-space fragment position and normal.
- Keep target highlight/dim presentation and sampled texture composition in the existing draw.
- Keep lighting resources alive across swapchain recreation and make their CPU/GPU layouts deterministic and testable.

**Non-Goals:**

- Mutable, animated, triggered, flickering, player-carried, or dynamically spawned lights.
- Shadows, visibility tests, light volumes, clustered/deferred lighting, HDR, tone mapping, exposure adaptation, fog, bloom, or emissive materials.
- Loading light data from files or introducing a light registry, scene graph, material system, render graph, or general renderer abstraction.
- Removing or redesigning the rifle, shooting targets, input, player controller, or prototype collision geometry.

## Decisions

### Replace the directional environment value with exactly two point lights

Replace `PrototypeEnvironmentLight`'s direction/directional intensity with an array of exactly two `PrototypePointLight` values plus one ambient intensity. Each light stores world position, linear RGB color, scalar intensity, and influence radius. Validation requires finite values, non-negative color and ambient values, positive intensity and radius, and an ambient value no greater than a small fixed upper bound suitable for intentional darkness.

Author one restrained, cooler light near the initial player area and one more prominent, warmer destination light deeper in the scene. Tune their radii so the intended route contains a region outside both influences while existing target feedback remains inspectable. Use a concrete count because this prototype needs two compositional light pools; a variable light collection or arbitrary maximum would add a lighting system before the game requires one.

A single local light was rejected because it cannot independently communicate temporary safety and a destination. Retaining a directional fill was rejected because it continues to flatten darkness across the whole level.

### Store immutable lighting in a dedicated uniform-buffer owner

Add one renderer-private RAII value for the prototype lighting resources. It owns a small host-visible, host-coherent uniform buffer and memory plus a one-binding descriptor-set layout, one-set descriptor pool, and immutable descriptor set. The CPU upload layout contains two 16-byte-aligned records, each represented by `position_and_radius` and `color_and_intensity` vectors, followed by one aligned ambient vector. Static assertions cover size, alignment, and offsets. Construction copies the validated level lighting once and unmaps it; no frame updates or per-frame copies occur.

Bind this descriptor as set 1 while the existing texture array remains set 0. The graphics pipeline receives both descriptor layouts and sets as non-owning handles and binds both before the existing draw. The lighting owner is created after the Vulkan context and before the format-dependent pipeline, survives swapchain recreation, and is destroyed after every dependent pipeline but before the device.

Using the push constant was rejected because the camera and temporary target masks already consume 80 bytes, while two clear point-light records and ambient data would exceed Vulkan's guaranteed 128-byte push-constant capacity. Combining texture and light ownership into one generalized scene-resource class was rejected because they have distinct data and failure paths. Per-frame uniform buffers were rejected because the lights are immutable in this milestone.

### Remove static lighting from the per-frame push constant

Shrink `ScenePushConstant` to the 64-byte camera plus the aligned 16-byte target presentation masks. `makeScenePushConstant` no longer accepts environment lighting, and `GraphicsPipeline::bindAndDraw` receives only per-frame camera and presentation data. The immutable light descriptor supplies all environment-light data.

This keeps frame requests gameplay-neutral and avoids repeatedly sending static values. Retaining unused directional fields or packing point-light values around mask bits was rejected because either preserves obsolete concepts or creates a brittle GPU layout.

### Pass existing vertex position to the fragment stage

The vertex buffer already stores each generated vertex in world space, so its CPU layout does not change. Add one interpolated `vec3` vertex-to-fragment output containing `inPosition` and consume it as the fragment world position. Keep the existing normal, solid mask, UV, and texture-layer varyings at stable matching locations, adding the world position at a new location to minimize CPU/GPU contract churn.

Duplicating world position in the vertex structure was rejected because the position attribute already contains the required value. Reconstructing position from depth was rejected as unnecessary complexity for a forward opaque draw.

### Use simple radius-bounded Lambert lighting

For each point light, the fragment shader computes the vector and distance from the fragment to the light, safely normalizes the direction, evaluates `max(dot(normal, direction_to_light), 0)`, and applies a smooth bounded falloff that reaches exactly zero at the authored radius. A concrete falloff of `(1 - clamp(distance / radius, 0, 1)^2)^2` provides a smooth edge without inverse-square singularities. Each contribution is multiplied by the light's linear RGB color and intensity.

Start the accumulated lighting from the near-black ambient value, add both point-light contributions, and clamp the RGB result to `[0, 1]` before multiplying the already sampled, tinted, highlighted-or-dimmed surface color. Retain opaque alpha, depth testing, the texture array, and the single draw.

Inverse-square attenuation was rejected because it would require extra tuning and singularity handling without improving this small stylized prototype. Tone mapping was rejected because the bounded low-dynamic-range model is sufficient and automatic exposure would undermine authored darkness.

### Accept bounded light leakage for the first local-light milestone

Point lights do not test occlusion in this change. Keep their radii local and author their positions around the current geometry so illumination through distant walls is not prominent from the intended route. Treat visible leakage that defeats the dark transition as a placement/radius defect during visual validation.

Shadow mapping, per-room light masks, and geometric light volumes were rejected because each adds substantial machinery before the basic lighting composition is proven. If unavoidable leakage remains after authored tuning, shadows can be proposed separately with evidence from this scene.

## Risks / Trade-offs

- [Point light visibly illuminates through an intervening wall] -> Keep both radii bounded, tune placement from the intended route, and record any remaining objectionable case as evidence for a later shadow change.
- [Near-black ambient crushes every texture or varies by display] -> Keep a small nonzero floor, inspect the packaged executable on the Vulkan smoke device, and tune only the concrete authored value rather than adding exposure controls.
- [CPU uniform layout differs from GLSL `std140`] -> Use explicit 16-byte-aligned vector records, static offset/size assertions, and deterministic layout tests.
- [Lighting descriptor lifetime crosses pipeline or device teardown] -> Own it beside the sampled texture, destroy every pipeline first, and extend lifecycle/failure-injection smoke coverage.
- [A future scripted light needs mutation] -> Add a later change for explicit light presentation or per-frame resources once a concrete scripted scene defines the required transitions; do not pre-build that path here.
- [Temporary target feedback becomes unreadable in darkness] -> Tune the two authored light pools and visually inspect live, highlighted, and dimmed target states without adding combat-specific lighting exceptions.

## Migration Plan

Replace the level light data, point-light validation, immutable GPU lighting owner, pipeline descriptor layouts, push-constant layout, shader interface, shaders, committed SPIR-V, and affected tests as one atomic change so incompatible CPU and GPU layouts cannot mix. Existing texture resources and frame requests remain compatible except for renderer-internal lighting arguments. Rollback restores the prior directional/ambient push-constant fields and shaders together; there is no persistent data migration or public API change.
