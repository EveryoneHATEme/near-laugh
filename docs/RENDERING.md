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
- independent enabled values for the two authored point-light slots; and
- at most 192 source-independent opaque boxes for accepted door poses and feedback.

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

Generated terrain, solids and the optional switch form immutable world-space
triangle batches grouped by structural material. Each selected prop model is
decoded once per scene load and expanded at its authored placements into
immutable material batches. An empty prop collection requires no model files.

Generated faces carry position, authored tint, outward world-space normal and
continuous one-repeat-per-metre UVs. Each solid chooses a structural material
independently of its collision kind; present terrain chooses one material for
the whole surface. Props retain authored UVs and use the placement translation,
yaw and positive uniform scale. Collision remains authored local boxes and does
not depend on imported triangles.

The loader accepts a controlled binary glTF 2.0 profile: one default scene,
one mesh-bearing root without children, one mesh, and one non-empty triangle
primitive with finite `POSITION`, `NORMAL` and `TEXCOORD_0` data. The selected
material supports base-color factor, an optional embedded PNG, OPAQUE or MASK,
and supported repeat samplers. A constant material uses a white texel. Other
material inputs, extensions, external images and unsupported hierarchy are
rejected with asset context. The legacy chair keeps its explicit prototype
obstacle material. The finite catalog and preparation steps are documented in
[APARTMENT_ASSETS.md](../resources/models/APARTMENT_ASSETS.md).

## Textures, Descriptors, and Lighting

`SceneResources` owns only the selected scene's immutable material resources.
Each `SampledTexture` uploads a `VK_FORMAT_R8G8B8A8_SRGB` image, generates the
full mip chain on the graphics queue and owns its view, repeat sampler,
factor/alpha uniform, descriptor layout, pool and descriptor. Apartment assets
use nearest sampling; legacy prototype textures retain linear sampling.

`LightingResources` validates and uploads exactly two immutable authored point
lights plus ambient intensity to one 80-byte `std140` uniform buffer. The
fragment shader combines texture color and tint with radius-bounded Lambert
point lighting and the optional finite-range spot light over a near-black
ambient floor. Each point-light contribution is multiplied by its frame's
enabled value. Ambient and the spotlight remain independent. Spot distance
and cone transitions are smooth; accumulated RGB
is clamped and surviving fragments remain opaque. MASK compares sampled alpha
times factor alpha against the material cutoff and discards uncovered fragments
before color or depth writes. The phone cord uses cutoff 0.5; OPAQUE materials
such as the radio ignore source alpha for coverage.

The pipeline uses two descriptor sets:

- set 0, binding 0: combined base-color sampler;
- set 0, binding 1: base-color factor and alpha controls;
- set 1, binding 0: authored lighting uniform buffer.

A renderer-private 128-byte push constant carries the camera matrix, three
aligned spotlight vectors, and `(outer cosine, spot enabled, point 0 enabled,
point 1 enabled)`. The standalone `SpotLightFrame` retains its zeroed disabled
representation. Both packaged shader stages share the packed layout.
Point-light toggles require no resource rebuilds, descriptor updates, or GPU
waits. Descriptors are written once during startup and survive swapchain
recovery. Lighting and the push constant are shared across draws; each material
batch binds its immutable set 0. Generated doors and feedback use the explicit
opaque prototype-obstacle material.

## Ownership and Lifetime

`Renderer` owns the Vulkan context, static scene resources, lighting resources,
changing door geometry, swapchain resources and pipeline. Swapchain-independent
textures, lighting and static mesh uploads survive resize and presentation recovery.
The pipeline borrows descriptor handles and owns no geometry.

Teardown destroys pipelines before the descriptors and mesh buffers they use;
all GPU owners are released before the logical device. Partial-construction
paths clean up only resources that were successfully created. Per-frame command
and synchronization resources are not modified while still in GPU use.

Each existing frame slot owns one persistently mapped changing-geometry buffer
with capacity for 192 boxes. The renderer waits the slot fence before updating
it, omits empty draws and never rebuilds static resources for door movement or
feedback. Runtime boxes describe accepted physics poses without visual motion
ahead of collision. The editor uses the same geometry helpers at authored
initial poses.

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
Prop geometry, initial door presentation, lighting resources, textures and the
pipeline remain in place during sculpting. A failed replacement retains the
previous resources and reports that the preview is stale. Correction or undo
can install a new coherent preview. Static runtime scene batches are immutable.
An invalid editor interior with no solids, terrain, or switch has no generated
world buffer or world draw. Present props/doors, entry/light markers and UI remain
available. Replacement between absent and present world meshes uses the same
transactional resource path, including recovery after a failed upload.

The switch is a pale plate with a contrasting fixed rocker, generated with the
obstacle texture and opaque tints. Its shared yawed bounds are 0.18 by 0.26 by
0.04 metres. Both full and terrain-only editor rebuilds retain its geometry.
The editor supplies the authored initial light state, safely omitting an
unusable switch; changing/removing its link restores the previous slot.

Editor-only selection bounds, light/entry spheres, brush footprints, invalid
terrain triangle outlines, prop render/proxy bounds, door hinge/arc/bolt-side
guides and placement feedback are CPU-projected and clipped
to the Vulkan view volume. The editor renderer draws
these lines through the ImGui background draw list, above scene geometry and
below UI panels, using the existing Vulkan backend. They intentionally have no
scene depth test and do not alter runtime frame requests or level data.

## Current Limits

The current renderer deliberately implements only the bounded scene above. It
does not infer a general material system, asset discovery, arbitrary runtime transforms,
texture streaming, bindless descriptors, a light registry, multiple dynamic
spot lights, shadows, fog, HDR, or a render graph. Such features should be
introduced only for a concrete visual or gameplay requirement.
