# vulkan-renderer Specification

## Purpose

Defines the direct Vulkan 1.3 renderer foundation, including device selection, validation, frame synchronization, swapchain recovery, resource lifetime, and the initial triangle smoke output.

## Requirements

### Requirement: Vulkan 1.3 baseline
The renderer SHALL create and use a Vulkan 1.3 instance and device, SHALL use Dynamic Rendering and Synchronization 2, and SHALL NOT implement a legacy render-pass or legacy synchronization fallback.

#### Scenario: Required Vulkan capabilities are available
- **WHEN** a physical device supports Vulkan 1.3, Dynamic Rendering, Synchronization 2, presentation, and the required swapchain capabilities
- **THEN** the renderer creates a logical device with those capabilities enabled

#### Scenario: Required Vulkan capabilities are unavailable
- **WHEN** no physical device satisfies all required Vulkan 1.3 capabilities
- **THEN** renderer startup fails with an actionable message identifying the missing requirement

### Requirement: Presentation device selection
The renderer SHALL select a physical device and queue configuration that support both graphics commands and presentation to the active window surface.

#### Scenario: Separate graphics and presentation queue families
- **WHEN** graphics and presentation support are provided by different queue families
- **THEN** the renderer selects both families and configures swapchain sharing and submission correctly

#### Scenario: No present-capable device
- **WHEN** available Vulkan devices cannot present to the active surface
- **THEN** startup fails before frame resources are created

### Requirement: Runtime-controlled frame interface
The renderer SHALL consume explicit framebuffer and resize state from the runtime and SHALL report a backend-neutral frame outcome. It SHALL NOT poll or wait for platform events, inspect application close state, or determine application lifetime.

#### Scenario: Runtime requests a renderable frame
- **WHEN** the runtime provides a non-zero framebuffer extent and the surface is renderable
- **THEN** the renderer performs the frame attempt and returns a backend-neutral outcome

#### Scenario: Runtime reports zero framebuffer extent
- **WHEN** the runtime provides a zero-sized framebuffer
- **THEN** the renderer performs no event wait and submits no GPU work

#### Scenario: Swapchain recovery is required
- **WHEN** acquisition or presentation detects an out-of-date or suboptimal swapchain
- **THEN** the renderer performs or schedules safe recovery and reports the result without taking control of the application loop

### Requirement: Development validation
Development builds SHALL request the Khronos validation layer when it is available, install a Vulkan debug messenger, report validation messages with decoded severity and category, and retain the number of validation errors for verification. Validation errors SHALL be treated as defects and SHALL cause the validation smoke run to fail.

#### Scenario: Validation layer is available
- **WHEN** a development build starts on a system with the Khronos validation layer
- **THEN** validation and the debug messenger are enabled before device creation

#### Scenario: Validation layer is unavailable
- **WHEN** a development build starts without the Khronos validation layer
- **THEN** the application emits a clear warning and continues only if all non-validation Vulkan requirements are met

#### Scenario: Vulkan operation fails
- **WHEN** a required Vulkan operation returns an error result
- **THEN** the application reports the operation and decoded Vulkan result before performing orderly cleanup

#### Scenario: Validation reports an error
- **WHEN** the debug messenger receives a validation message with error severity during the smoke run
- **THEN** the error is counted and the smoke process exits unsuccessfully after orderly renderer cleanup

### Requirement: Explicit frame lifecycle
The renderer SHALL use a small fixed number of frames in flight, with each frame owning its command resources and synchronization objects. CPU reuse of a frame SHALL wait until the GPU has completed that frame.

#### Scenario: Frame slot is reused
- **WHEN** the renderer advances to a frame slot that was previously submitted
- **THEN** it waits for that slot's completion fence before resetting or modifying its resources

#### Scenario: Image is submitted and presented
- **WHEN** a swapchain image is acquired successfully
- **THEN** the renderer records commands with Dynamic Rendering, submits them using Synchronization 2-compatible synchronization, and presents the image with correct semaphore ordering

### Requirement: Swapchain lifecycle
The renderer SHALL validate the current surface capabilities required for swapchain image usage and composite alpha, SHALL create a swapchain only with supported values, and SHALL recreate swapchain-dependent resources after resize, out-of-date, or suboptimal presentation without recreating instance- or device-lifetime resources.

#### Scenario: Required surface configuration is supported
- **WHEN** the surface supports color-attachment swapchain images and at least one usable composite-alpha mode
- **THEN** the renderer selects supported values and creates the swapchain from the current surface capabilities

#### Scenario: Required image usage is unavailable
- **WHEN** the surface does not support color-attachment usage for swapchain images
- **THEN** renderer startup fails before swapchain creation with an actionable message identifying the missing color-attachment capability

#### Scenario: Composite alpha selection is required
- **WHEN** the preferred opaque composite-alpha mode is unavailable but another supported mode exists
- **THEN** the renderer selects a supported fallback mode without requesting an unsupported value

#### Scenario: Window framebuffer is resized
- **WHEN** the framebuffer receives a new non-zero extent
- **THEN** the renderer waits for affected GPU work and recreates the swapchain and dependent image views for that extent

#### Scenario: Swapchain becomes out of date
- **WHEN** image acquisition or presentation reports an out-of-date swapchain
- **THEN** the current frame is abandoned safely and swapchain recreation occurs before the next rendered frame

#### Scenario: Swapchain becomes suboptimal
- **WHEN** image acquisition or presentation reports a suboptimal swapchain
- **THEN** the renderer completes safe work and schedules swapchain recreation without terminating the application

### Requirement: Camera-transformed prototype scene rendering
The renderer SHALL load the scene SPIR-V shaders, fixed prototype surface textures, and validated static model mesh from the explicit runtime resources; upload correctly described immutable opaque vertex and sampled texture data; bind the immutable texture and lighting descriptors; apply the runtime-owned camera frame in the vertex stage; clear the swapchain image; and render the selected level's generated world mesh followed by the imported prop mesh through the same Vulkan graphics pipeline. World geometry SHALL include authored solid structures and the optional switch, and terrain triangles only when terrain exists. The editor SHALL use the same optional-terrain geometry behavior for structurally safe document previews without requiring successful gameplay validation. An empty generated stream in an invalid editable document SHALL omit the world draw safely while retaining other renderable scene content and UI.

#### Scenario: First visible scene frame
- **WHEN** renderer initialization succeeds with valid runtime resources and the window has a non-zero framebuffer extent
- **THEN** the application presents the selected textured generated world and imported static prop from the supplied camera pose over the configured clear color

#### Scenario: Camera frame changes
- **WHEN** the renderer receives a different valid camera frame
- **THEN** the next submitted world and prop draws use the new view/projection transform without recreating the graphics pipeline, immutable mesh buffers, sampled texture resources, or swapchain

#### Scenario: Scene shader asset is unavailable
- **WHEN** a required scene SPIR-V shader cannot be read beneath the configured resource root or cannot be used to create a shader module
- **THEN** startup fails with an error identifying the resolved shader asset path and releases all previously created Vulkan resources

#### Scenario: Scene texture asset is unavailable or invalid
- **WHEN** a required fixed prototype texture cannot be read or decoded beneath the configured resource root
- **THEN** startup fails with an error identifying the resolved texture asset path and releases all previously created Vulkan resources

#### Scenario: Static model asset is unavailable or invalid
- **WHEN** the required static GLB cannot be read, validated, converted, or uploaded
- **THEN** startup fails with an error identifying the resolved model asset path and releases all previously created Vulkan resources

#### Scenario: Interior has no terrain
- **WHEN** a valid interior is rendered in the game or editor
- **THEN** its slabs, walls, stairs, chair, and any switch appear through the existing depth-tested textured lighting path without an invented terrain surface

#### Scenario: Invalid interior has no generated geometry
- **WHEN** an editable terrain-free document has no solids and no switch
- **THEN** the editor still displays its chair, markers, validation feedback, and UI without allocating a zero-sized world buffer or submitting an invalid draw

### Requirement: Immutable opaque mesh buffer ownership
The renderer SHALL give each non-empty generated world mesh and imported prop mesh an explicit renderer-lifetime buffer owner. Each owner SHALL keep its vertex buffer and allocation valid until every dependent draw is complete, SHALL release partial construction exactly once, and SHALL survive swapchain recreation when the graphics pipeline format remains compatible. An absent generated stream SHALL require neither an empty allocation nor a world draw. Editor document replacement between terrain-bearing and terrain-free scenes SHALL install replacement resources transactionally after dependent GPU work completes and SHALL retain the previous usable resources if replacement fails.

#### Scenario: Opaque scene frame is recorded
- **WHEN** both immutable mesh buffers have initialized successfully
- **THEN** the renderer binds and draws each buffer once through the same opaque pipeline and descriptors

#### Scenario: Swapchain is recreated without a format change
- **WHEN** resize, out-of-date acquisition, or suboptimal presentation recreates swapchain-dependent resources with the existing format
- **THEN** neither immutable mesh buffer is rebuilt or re-uploaded

#### Scenario: Model buffer creation fails partway
- **WHEN** imported mesh buffer creation, memory allocation, binding, mapping, or upload fails
- **THEN** startup reports the failed operation and releases every successfully created model and renderer resource exactly once

#### Scenario: Editor switches scene terrain presence
- **WHEN** the editor replaces a terrain-bearing document with an interior or reverses that replacement
- **THEN** successful preview replacement reflects the current terrain presence and authored geometry without stale terrain, leaked resources, or mutation of a running game's immutable scene

#### Scenario: Interior presentation recovers
- **WHEN** the game or editor resizes, minimizes and restores, or recovers its swapchain while showing an interior
- **THEN** current interior geometry and lighting state remain available after recovery without Vulkan validation errors

### Requirement: Depth attachment lifecycle
The renderer SHALL select a supported depth format, SHALL create a depth image and view for each concurrently usable swapchain image, SHALL clear and use the corresponding attachment for depth testing during Dynamic Rendering, and SHALL keep each depth resource valid until dependent GPU work is complete.

#### Scenario: Scene frame uses depth
- **WHEN** a scene frame is recorded for a swapchain image
- **THEN** Dynamic Rendering uses a compatible depth attachment and the graphics pipeline performs depth testing and depth writes for opaque geometry

#### Scenario: Swapchain is recreated
- **WHEN** resize, out-of-date acquisition, or suboptimal presentation causes swapchain recreation
- **THEN** the renderer waits for affected work, destroys the old depth resources, and creates compatible depth resources for the replacement swapchain extent before the next scene submission

#### Scenario: Depth resources cannot be created
- **WHEN** no supported depth format or suitable device memory is available
- **THEN** renderer initialization or recovery fails with an actionable error and releases all successfully created dependent resources exactly once

### Requirement: Vulkan resource ownership
Every Vulkan handle SHALL have one explicit owner, non-owning references SHALL be distinguishable from owners, and destruction SHALL occur only after dependent GPU work and child resources are complete.

#### Scenario: Normal renderer shutdown
- **WHEN** the application shuts down after submitting frames
- **THEN** pending device work is completed and Vulkan resources are destroyed once in reverse dependency order

#### Scenario: Partial initialization fails
- **WHEN** renderer construction fails after creating some Vulkan resources
- **THEN** all successfully created resources are released exactly once without requiring the fully initialized renderer destructor

#### Scenario: Validation-assisted smoke run
- **WHEN** the debug executable creates the renderer, renders frames, recreates the swapchain once, and shuts down
- **THEN** the Vulkan validation layer reports no errors

### Requirement: Sampled prototype texture lifetime
The renderer SHALL decode the fixed prototype texture set before entering the frame loop, SHALL upload it into device-local sampled image storage with a complete mip chain, and SHALL own the image memory, image view, sampler, descriptor layout, descriptor pool, and descriptor set explicitly. The sampled texture resources SHALL remain immutable after startup and SHALL outlive every draw or descriptor that refers to them.

#### Scenario: Textured renderer starts
- **WHEN** all fixed texture assets decode successfully and the selected device supports their required sampled and transfer operations
- **THEN** renderer initialization uploads a complete sampled texture resource and binds one immutable texture descriptor for the opaque scene pipeline

#### Scenario: Texture upload fails partway
- **WHEN** decoding, staging, image allocation, transfer, mip generation, image-view creation, sampler creation, or descriptor creation fails
- **THEN** startup reports the failed texture operation and releases every successfully created staging and persistent Vulkan resource exactly once

#### Scenario: Swapchain is recreated
- **WHEN** resize, out-of-date acquisition, or suboptimal presentation recreates swapchain-dependent resources
- **THEN** the immutable sampled texture image, view, sampler, and decoded surface assignment remain valid without being decoded or uploaded again

#### Scenario: Required texture format operation is unavailable
- **WHEN** the selected physical device cannot sample, transfer, or generate the required mip chain for the fixed texture format
- **THEN** renderer startup fails with an actionable message identifying the missing texture-format capability

### Requirement: Per-frame spot-light presentation
The renderer SHALL accept at most one valid optional source-independent spot-light description with each frame request and SHALL apply changes in its pose, parameters, or enabled state to the next submitted opaque scene draw. Updating the spot light SHALL NOT recreate the graphics pipeline, swapchain, immutable scene geometry, sampled textures, or immutable point-light resources; SHALL NOT add another scene draw; and SHALL remain safe for the current frames-in-flight model.

#### Scenario: Spot light changes between frames
- **WHEN** a later frame supplies a different valid spot-light pose, parameters, or enabled state
- **THEN** the next submitted scene draw uses the new value without recreating renderer-lifetime or swapchain resources

#### Scenario: Spot light is disabled
- **WHEN** a frame supplies no enabled dynamic spot light
- **THEN** the existing immutable point-light and ambient scene shading is rendered without a dynamic cone contribution

#### Scenario: Spot-light source is not the player
- **WHEN** a valid frame supplies a spot-light pose produced by another runtime object
- **THEN** the renderer shades from that supplied pose without requiring a camera-mounted or flashlight-specific path

### Requirement: Per-frame authored point-light enable state
Each successfully presented scene frame SHALL apply its requested enabled state independently to both authored point lights. Disabling a point light SHALL remove its contribution from generated world, switch, and imported-prop surfaces without changing authored lighting resources, immutable geometry, ambient, texture behavior, or the optional spotlight. Changes SHALL remain valid with multiple frames in flight and SHALL NOT require additional device capabilities beyond the current Vulkan baseline.

#### Scenario: Light state alternates across frames
- **WHEN** successive frame requests alternate which authored point light is enabled
- **THEN** each presented frame uses its own requested state without stale values or modifying resources still used by an earlier frame

#### Scenario: Flashlight is disabled
- **WHEN** the frame contains no enabled spotlight and disables one authored point light
- **THEN** the other authored point light and ambient retain their contributions without depending on spotlight enable state

#### Scenario: Presentation recovers after a toggle
- **WHEN** resize, swapchain recovery, or minimize/restore occurs after a point light is disabled
- **THEN** the next presented frame uses the requested current light state and retained immutable scene resources without validation errors
