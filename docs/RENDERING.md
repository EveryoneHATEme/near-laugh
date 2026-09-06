# Rendering Architecture

## Purpose

The renderer presents the authored environments, lighting, darkness, and
player-carried light needed by near-laugh's first-person narrative horror
experience. It is a direct Vulkan implementation for this game, not a reusable
graphics framework.

## API Boundary

The renderer targets Vulkan 1.3 and uses Dynamic Rendering, Synchronization 2,
and `vkQueueSubmit2`. There is no rendering-backend abstraction. Vulkan types
remain in `near_laugh_render` and `near_laugh_editor_render`; the public runtime
facade, world data, player code, and normal window interface do not expose
Vulkan or GLFW types.

GLFW/Vulkan coupling is confined to an internal bridge that supplies required
instance extensions and creates the presentation surface. The bridge is a
concrete integration boundary, not an RHI.

Development builds enable Vulkan validation when available. Error-severity
validation messages are recorded in a diagnostics sink that outlives Vulkan
teardown and cause smoke tests to fail after orderly cleanup.

## Frame and Presentation Model

The runtime submits at most one `FrameRequest` per loop iteration. It contains:

- the current framebuffer extent and resize state;
- a standard-layout, column-major camera view-projection matrix; and
- at most one source-independent dynamic `SpotLightFrame`; and
- independent enabled values for the two authored point-light slots.

A zero extent is skipped before GPU submission. The renderer owns swapchain
out-of-date and suboptimal handling and returns a backend-neutral `Rendered`,
`Skipped`, or `Recovered` outcome. Event processing, simulation, camera policy,
and application lifetime remain runtime-owned.

Before swapchain creation, the renderer requires color-attachment image usage
and chooses a supported composite-alpha mode, preferring opaque. It owns one
device-local depth image and view per swapchain image. Recording transitions
color and depth with Synchronization 2, clears depth to 1.0, and uses depth
test/write with `LESS`.

## Authored Scene Input

Runtime composition resolves and validates the selected level (defaulting to
`resources/levels/prototype.level.json`) and entry before renderer construction. The
renderer receives an immutable `PrototypeLevel`; it does not parse JSON, save
documents, hot-reload levels, or select paths from level data.

The current scene uses two immutable world-space triangle streams:

- generated optional terrain, axis-aligned solids, and the optional yawed switch; and
- the one packaged chair flattened synchronously from
  `resources/models/prototype_chair.glb`.

Generated faces carry position, authored tint, outward world-space normal,
continuous one-repeat-per-metre UVs, and an unsigned texture layer. The stable
surface-to-layer order is floor (0), boundary (1), and obstacle (2). Movement
test structures and the chair use the obstacle layer.

The chair loader accepts one controlled binary glTF 2.0 profile: one default
scene, one mesh-bearing root without children, one mesh, and one non-empty
triangle-list primitive with finite `POSITION`, `NORMAL`, and `TEXCOORD_0`
data. It applies the root transform plus the level-authored translation, yaw,
and uniform scale. File materials and textures do not affect the result.

## Textures, Descriptors, and Lighting

At startup, `SampledTexture` decodes these opaque 256-by-256 PNGs in layer
order:

1. `resources/textures/prototype_floor.png`
2. `resources/textures/prototype_boundary.png`
3. `resources/textures/prototype_obstacle.png`

It uploads one three-layer device-local `VK_FORMAT_R8G8B8A8_SRGB` image,
generates the full mip chain on the graphics queue, and owns the array view,
repeat/linear sampler, descriptor layout, pool, and immutable descriptor.

`LightingResources` validates and uploads exactly two immutable authored point
lights plus ambient intensity to one 80-byte `std140` uniform buffer. The
fragment shader combines texture color and tint with radius-bounded Lambert
point lighting and the optional finite-range spot light over a near-black
ambient floor. Each point-light contribution is multiplied by its frame's
enabled value. Ambient and the spotlight remain independent. Spot distance
and cone transitions are smooth; accumulated RGB
is clamped and alpha remains opaque.

The pipeline binds two immutable descriptor sets once for the scene draws:

- set 0, binding 0: combined texture-array sampler;
- set 1, binding 0: authored lighting uniform buffer.

A renderer-private 128-byte push constant carries the camera matrix, three
aligned spotlight vectors, and `(outer cosine, spot enabled, point 0 enabled,
point 1 enabled)`. The standalone `SpotLightFrame` retains its zeroed disabled
representation. Both packaged shader stages share the packed layout.
Point-light toggles require no resource rebuilds, descriptor updates, or GPU
waits. Descriptors are written once during startup and survive swapchain
recovery. The pipeline binds them and the push constant once, then draws the
generated world followed by the chair.

## Ownership and Lifetime

`Renderer` owns the Vulkan context, sampled texture, lighting resources, world
mesh, chair mesh, swapchain resources, and pipeline. Swapchain-independent
textures, lighting, and mesh uploads survive resize and presentation recovery.
The pipeline borrows descriptor handles and owns no geometry.

Teardown destroys pipelines before the descriptors and mesh buffers they use;
all GPU owners are released before the logical device. Partial-construction
paths clean up only resources that were successfully created. Per-frame command
and synchronization resources are not modified while still in GPU use.

The editor reuses the narrow rendering helpers but owns a separate Vulkan
context and active-document resources. It records scene geometry first and
Dear ImGui last in the same Dynamic Rendering pass.

Editor preview consumes renderable document fields directly instead of
constructing a validated runtime level. Geometry generation is shared with
the runtime; gameplay validation failures therefore do not hide the editable
scene. Object changes install replacement scene resources transactionally after
GPU completion. Terrain stamps coalesce into one world-mesh rebuild per editor
frame: the full terrain and unchanged solid vertices are regenerated, both
in-flight frame fences are awaited, and a replacement buffer is installed.
Chair geometry, lighting resources, textures, and pipeline remain in place
during sculpting. A failed replacement retains the previous resources and
reports the error. This uses the existing immutable buffer owner for each
replacement; the game renderer and its meshes remain immutable.
An invalid editor interior with no solids, terrain, or switch has no generated
world buffer or world draw. Chair geometry, entry/light markers, and UI remain
available. Replacement between absent and present world meshes uses the same
transactional resource path, including recovery after a failed upload.

The switch is a pale plate with a contrasting fixed rocker, generated with the
obstacle texture and opaque tints. Its shared yawed bounds are 0.18 by 0.26 by
0.04 metres. Both full and terrain-only editor rebuilds retain its geometry.
The editor supplies the authored initial light state, safely omitting an
unusable switch; changing/removing its link restores the previous slot.

Editor-only selection bounds, light/entry spheres, brush footprints, invalid
terrain triangle outlines, and placement feedback are CPU-projected and clipped
to the Vulkan view volume. The editor renderer draws
these lines through the ImGui background draw list, above scene geometry and
below UI panels, using the existing Vulkan backend. They intentionally have no
scene depth test and do not alter runtime frame requests or level data.

## Current Limits

The current renderer deliberately implements only the bounded scene above. It
does not infer a general material system, asset discovery, runtime transforms,
texture streaming, bindless descriptors, a light registry, multiple dynamic
spot lights, shadows, fog, HDR, or a render graph. Such features should be
introduced only for a concrete visual or gameplay requirement.
