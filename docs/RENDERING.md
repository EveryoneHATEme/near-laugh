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
- a standard-layout, column-major camera view-projection matrix;
- at most one source-independent dynamic `SpotLightFrame`;
- a borrowed span containing exactly one 0/1 enable per authored point light;
- at most 192 source-independent opaque boxes for accepted door poses and feedback;
- borrowed resolved foreground/ambience captions; and
- a borrowed exact set of zero through four selected character palettes and
  finite world translation/yaw, identified by render handles and skeleton tags.

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

Generated terrain, solids and switch plates form immutable world-space
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

`LightingResources` validates zero to eight immutable authored point lights
and ambient in [0, 0.20]. Each fenced frame slot owns a 1936-byte `std140`
uniform containing light parameters/enables/layer indices, ambient/count/depth
tolerance, and up to 24 shadow cameras. The
fragment shader combines texture color and tint with radius-bounded Lambert
point lighting and the optional finite-range spot light over an authored
ambient floor, which may be zero. Each point-light contribution uses its frame's
enabled value. Ambient and the spotlight remain independent. Spot distance
and cone transitions are smooth; accumulated RGB
is clamped and surviving fragments remain opaque. MASK compares sampled alpha
times factor alpha against the material cutoff and discards uncovered fragments
before color or depth writes. The phone cord uses cutoff 0.5; OPAQUE materials
such as the radio ignore source alpha for coverage.

The pipeline uses two descriptor sets:

- set 0, binding 0: combined base-color sampler;
- set 0, binding 1: base-color factor and alpha controls;
- set 1, binding 0: frame-slot lighting uniform buffer;
- set 1, binding 1: one sampled 2D depth array for point-light shadows.

A renderer-private 128-byte push constant carries the camera matrix, three
aligned spotlight vectors, and `(outer cosine, spot enabled, 0, 0)`.
The standalone `SpotLightFrame` retains its zeroed disabled
representation. Both packaged shader stages share the packed layout.
Point-light toggles update the existing mapped uniform after the slot fence;
they require no resource rebuilds or descriptor changes. Descriptors are
written once during scene creation and survive swapchain
recovery. Lighting and the push constant are shared across draws; each material
batch binds its immutable set 0. Generated doors and feedback use the explicit
opaque prototype-obstacle material.

## Interior Point Shadows

Up to four configured point lights own six 512x512 depth layers each per frame
slot, including initially disabled sources. Shadow radii are [0.25, 20] metres;
the near plane is 0.01 m. D32 is preferred; D16 is a fallback only when sampled
depth attachment features and all image limits support the profile. One
cleared dummy layer keeps the descriptor valid with no casters. Failure to
create the profile is reported instead of disabling shadows.

Each enabled source renders six ordinary Dynamic Rendering depth passes before
the color pass. Every static material batch, the current accepted changing boxes
and selected deformed character ranges participate; editor preview uses the
authored initial door mesh.
Both sides cast shadows, OPAQUE ignores alpha and MASK uses the same material
texture/factor/cutoff as the color pass. Render triangles cast shadows even
when a prop has no collision proxies. Player capsules and editor overlays do
not cast shadows.

The fragment shader selects the dominant-axis face and performs nine nearest
depth comparisons. Edge taps reproject onto the neighboring face. Each tap
compares the receiver plane at its actual texel center, with a small
world-space slope/contact bias and half a UNORM unit tolerance for D16.
Interior taps share the face transformation. This avoids repeated face math
without reducing the filter or face resolution. Disabled, back-facing and
out-of-radius contributions skip shadow sampling.

Synchronization 2 barriers order prior fragment reads, early/late depth writes
and subsequent fragment reads of each frame slot's array. Fences protect host
uniform/geometry writes and slot reuse. Shadows regenerate each submitted
frame; there is no culling, static cache, render graph or asynchronous queue.
Finite resolution limits fine cutouts and contact precision, particularly at
long distances with D16. The supported blocking exercise uses emitters at
least 5 cm from occluders and walls/door leaves at least 5 cm thick. Fixed-view
readbacks and measured T1 results are recorded in the change validation record.

## Ownership and Lifetime

`Renderer` owns the Vulkan context, static scene resources, lighting resources,
changing door and character geometry, swapchain resources and pipeline.
Swapchain-independent textures, lighting, static mesh uploads and character
resources survive resize and presentation recovery.
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
An invalid editor interior with no solids, terrain, or switches has no generated
world buffer or world draw. Present props/doors, entry/light markers and UI remain
available. Replacement between absent and present world meshes uses the same
transactional resource path, including recovery after a failed upload.

Each switch is a pale plate with a contrasting fixed rocker, generated with the
obstacle texture and opaque tints. Its shared yawed bounds are 0.18 by 0.26 by
0.04 metres. Both full and terrain-only editor rebuilds retain its geometry.
The editor supplies each light's authored initial state independently of
switch links. Relinking/removing plates never rewrites those values. An unsafe
lighting edit retains the whole prior scene/lighting/shadow preview and its
matching enable vector with an explicit stale diagnostic; correction or undo
installs the new coherent set. Play preflights the saved lighting profile and
shadow shaders on the editor's device before launching the game.

Editor-only selection bounds, light/entry spheres, brush footprints, invalid
terrain triangle outlines, selected light ranges/switch links, prop render/proxy bounds, door hinge/arc/bolt-side
guides and placement feedback are CPU-projected and clipped
to the Vulkan view volume. The editor renderer draws
these lines through the ImGui background draw list, above scene geometry and
below UI panels, using the existing Vulkan backend. They intentionally have no
scene depth test and do not alter runtime frame requests or level data.

## Prepared character presentation

The separate animation target decodes the controlled mannequin profile: one
skin, at most 65 joints/80 nodes, an identity mesh with one or two indexed
primitives, 10,000 source vertices/50,000 indices, and exactly idle/walk/interact.
Inputs are embedded GLB up to 16 MiB with at most 32 MiB decoded CPU data and a
separate 32 MiB parser allocation budget. Only finite unit-scale TR,
LINEAR translation/rotation channels and constant diffuse
two-sided OPAQUE materials are accepted. Static loading keeps its own profile.
See [character preparation and calibration](../resources/characters/README.md).

Selected instances share immutable assets and primitive materials. Each supplied
palette contains model-relative global joint matrices in the asset's skin order;
inverse binds are applied during deformation, followed by world translation/yaw
once. Local pose sampling uses node order, including constant ancestors. The
renderer checks the complete selected instance set, skeleton identity, palette
size and finite transforms before submission and retains no caller frame storage.
CPU skinning evaluates and uploads each source vertex once per instance into
the existing vertex layout, capped at 40,000 vertices across four instances.
Each distinct asset shares one immutable uint32 index range and its primitive
materials. Indexed draws preserve declared triangle order, using checked
first-index/count and per-instance signed vertex offsets. The aggregate draw
limit is 200,000 indices, counting every instance even when indices are shared.
Normal deformation is normalized after weighted
rotation and one placement transform. The same ranges feed color and every
enabled point-shadow face, including off-screen silhouettes.

Character resources own the immutable index buffer, initialized once through
coherent host memory, and separate persistently mapped coherent vertex buffers
for the two existing frame slots. CPU validation/deformation finishes before
image acquisition; writes wait for the slot fence. Empty selection allocates
no character GPU buffers/materials. Swapchain and attachment-format recovery
retain character resources. Editor replacement builds a complete candidate
scene/lighting/character set and installs it only after success; failure retains
the previous set and matching pose contract. Pipelines and dependent GPU work
finish before character buffers, materials and immutable CPU owners are released.

Runtime character palettes come from fixed-step route playback. World placement
uses current collision-accepted ground feet/yaw, including on slopes, while
physics privately offsets its upright capsule. Color and shadow passes consume
the same accepted pose. No renderer clock or separate placement interpolation
can advance an actor through a blocker. The editor prepares initial idle poses
without route autoplay and transactionally retains their CPU/GPU owners through
replacement failures. Character reference diagnostics use its existing overlay.

## Russian captions

The game draws resolved foreground and optional ambience text after the scene
using a private alpha-blended Vulkan pipeline with depth testing/writes disabled.
White glyphs have dark backing panels, 5% safe margins, word wrapping and separate
four-line foreground/two-line ambience lanes. Supported framebuffer sizes run
from 800x600 through 3840x2160. Smaller windows receive bounded best-effort layout.
No ImGui code is linked into game rendering.

The packaged Noto Sans Regular font is checked against its pinned SHA-256 before
stb_truetype parsing. Glyph coverage and fit of selected Russian captions validate
before playback. Latin, Cyrillic including Ё/ё, and selected punctuation are
baked at 24/32/48/64 pixels into one immutable atlas. Size follows framebuffer
scale with a width cap so narrow, tall windows retain full text. The renderer
uploads the atlas on the first nonempty caption, retains it through recovery,
and rewrites a frame slot's bounded glyph buffer only after its fence. Empty
captions clear that slot's draw count. Color/depth attachment format changes
recreate the text pipeline; partial allocation and final destruction release
all acquired resources before the device.

The editor loads the same trusted font for Cyrillic properties, diagnostics and
its audition panel. Source markers, room wireframes and selected connection
links use the existing clipped editor overlay; audio volumes never create
collision geometry.

## Current Limits

The current renderer deliberately implements only the bounded scene above. It
does not infer a general material system, asset discovery, arbitrary runtime transforms,
texture streaming, bindless descriptors, a light registry, multiple dynamic
spot lights, shadowed spot lights, fog, HDR, or a render graph. Such features should be
introduced only for a concrete visual or gameplay requirement.
