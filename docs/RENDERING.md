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
directional lighting
point/spot lights as required by the game
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
resize state. A zero extent is skipped before GPU submission. Swapchain
out-of-date/suboptimal interpretation and recovery remain renderer-owned, and a
backend-neutral rendered/skipped/recovered outcome is returned to the runtime.

Shader paths are resolved beneath `RuntimeConfig::resource_root` before renderer
construction. Renderer code never constructs a path relative to the process
working directory.

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
