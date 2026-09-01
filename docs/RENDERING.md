# Rendering Architecture

## Goal

The renderer exists to render the FPS described in GAMEPLAY.md.

It is not a general-purpose graphics framework.

## API

Graphics API: Vulkan

Initial baseline: Vulkan 1.3

The renderer may assume a modern desktop Vulkan implementation.

There is no rendering backend abstraction.

Vulkan types remain inside `near_laugh_render`. The public runtime facade and
the normal window API do not include or exchange Vulkan or GLFW types. A narrow
internal GLFW/Vulkan bridge supplies instance extensions and creates the
presentation surface.

Do not create interfaces such as:

IRenderDevice
IGraphicsAPI
IVulkanBackend
IDirectXBackend

unless another graphics backend becomes an actual requirement.

The renderer IS a Vulkan renderer.

## Vulkan API Policy

Prefer Vulkan 1.3 functionality where it simplifies implementation.

Use Dynamic Rendering.

Use Synchronization 2.

Validation layers must be enabled in development builds when available.

Validation errors are treated as bugs.

Do not support legacy Vulkan synchronization APIs alongside
Synchronization 2 unless there is a demonstrated compatibility need.

## Initial Rendering Scope

The initial renderer should support:

opaque static meshes
opaque dynamic meshes
depth buffering
textures
basic materials
authored local lighting
additional point/spot lights as required by the game
basic shadow mapping
first-person weapon rendering
simple transparency where required
basic post-processing

The initial renderer does NOT require:

ray tracing
mesh shaders
virtual geometry
GPU-driven rendering
a render graph
asynchronous compute
multiple graphics queues
bindless rendering
virtual texturing
global illumination
arbitrary shader graphs
a material editor
multiple rendering backends

## Frame Model

Use a small fixed number of frames in flight.

Each frame owns its transient per-frame resources, including
the synchronization objects and command resources required for that frame.

CPU code must not modify resources still in use by the GPU.

Resource lifetime must always account for pending GPU work.

Prefer a simple and correct synchronization model over maximizing
CPU/GPU overlap.

The runtime sends one explicit frame request containing framebuffer extent and
resize state plus a standard-layout, column-major camera view-projection
matrix and at most one source-independent dynamic spot-light value. A zero
extent is skipped before GPU submission. Swapchain
out-of-date/suboptimal interpretation and recovery remain renderer-owned, and a
backend-neutral rendered/skipped/recovered outcome is returned to the runtime.
The runtime handles all three outcomes explicitly and retains ownership of the
next event-processing and lifetime decision.

Before creating or recreating a swapchain, the renderer verifies that the
surface supports color-attachment image usage. It prefers opaque composite
alpha and otherwise selects the first supported pre-multiplied,
post-multiplied, or inherited mode. Missing required usage or a usable
composite-alpha mode is reported before `vkCreateSwapchainKHR`.

Shader paths are resolved beneath `RuntimeConfig::resource_root` before renderer
construction. Renderer code never constructs a path relative to the process
working directory.

The current visible smoke output is a single immutable world-space triangle
stream expanded deterministically from the same `PrototypeLevel` solids used
by static physics collision. Every axis-aligned solid contributes six tinted
faces with explicit outward world-space normals, continuous local face UVs at
one repeat per metre, and one stable floor/boundary/obstacle/shooting-target
texture-array layer. The graphics pipeline reads position, packed vertex tint,
a floating-point normal, UVs, and an unsigned texture layer. The existing world
position also passes from the vertex stage to the fragment stage. A 128-byte
shared push constant carries the grounded player's current interpolated 4x4
camera matrix plus four aligned vectors describing one optional dynamic spot
light: position/range, direction/inner-cone cosine, color/intensity, and
outer-cone cosine/enabled state. The spot-light data contains no player or
flashlight type and can be supplied by another concrete runtime object.

The fragment stage samples one repeat/linear `sampler2DArray`, multiplies the
sampled sRGB color by the authored tint, and accumulates radius-bounded Lambert
diffuse illumination from exactly two immutable level-authored point lights
plus the optional finite-range spot light over a near-black ambient floor.
Spot lighting uses smooth distance falloff and a smooth inner-to-outer cone
transition. Accumulated RGB is clamped and alpha remains opaque. The cool spawn
pool and warmer destination pool leave an intentionally dark region between
their non-overlapping influences, which the flashlight can illuminate locally.
The scene remains one immutable vertex buffer and one draw call. It does not
introduce shadows, a visible or volumetric beam, a light registry, multiple
dynamic spot lights, general or file-defined materials, model assets,
per-object transforms, extra plate draws, fog, HDR post-processing,
collision/gameplay types, or a general scene framework. Without a shadow map,
the spot contribution does not account for occluding geometry.

The renderer decodes `prototype_floor.png`, `prototype_boundary.png`,
`prototype_obstacle.png`, and `prototype_shooting_target.png` during startup.
One concrete renderer-private RAII owner uploads their contiguous RGBA data to
a four-layer device-local `VK_FORMAT_R8G8B8A8_SRGB` image, generates the full
mip chain on the graphics queue with Synchronization 2 and `vkQueueSubmit2`,
and owns the array view, repeat/linear sampler, one-set descriptor layout,
pool, and immutable descriptor. The graphics pipeline borrows only the layout
and set handles. The texture owner is created after the Vulkan context, is not
rebuilt during swapchain recreation, and is destroyed after every pipeline but
before the logical device. There is no texture cache, streaming uploader,
asset discovery, bindless path, descriptor indexing, compression, anisotropy,
or hot reload.

A separate renderer-private RAII owner validates the level lighting and uploads
two `std140` point-light records plus the ambient scalar once to an 80-byte
host-visible, host-coherent uniform buffer. It owns one fragment-stage uniform
descriptor layout, one-set pool, and immutable descriptor. The owner is created
beside the sampled texture, survives swapchain recreation, is destroyed after
all dependent pipelines and before the logical device, and has explicit
partial-construction cleanup. It is not a light registry or per-frame lighting
path; dynamic spot-light data uses the existing per-draw push constant instead.

The renderer selects the first supported format from its small depth candidate
list and owns one device-local depth image, allocation, and view for every
swapchain image. Corresponding depth resources are created and destroyed with
the swapchain, including recreation and partial-construction cleanup. Each
recording transitions its color and depth image through Synchronization 2,
clears depth to 1.0, and supplies the depth view directly to Dynamic Rendering.
The opaque pipeline uses depth test/write with `LESS`; no render pass or legacy
synchronization path exists.

The Vulkan debug callback records decoded severity/category messages in a
thread-safe runtime-owned diagnostics sink and never throws or aborts. The sink
outlives Vulkan teardown; a smoke process exits unsuccessfully after orderly
cleanup if any error-severity validation message was counted.

## Descriptor Strategy

Start with explicit and simple descriptor layouts.

Do not introduce bindless descriptor architecture initially.

Descriptor indexing may be introduced only when a concrete renderer
requirement makes ordinary descriptor management significantly worse.

Descriptor updates must respect GPU lifetime and frame-in-flight rules.

The prototype binds two immutable descriptors once for the single scene draw:
the combined image sampler is set 0 binding 0, and the lighting uniform buffer
is set 1 binding 0. Each descriptor is updated once during renderer startup;
neither is rewritten per frame or recreated during swapchain recovery.
