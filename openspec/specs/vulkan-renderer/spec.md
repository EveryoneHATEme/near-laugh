# vulkan-renderer Specification

## Purpose

Defines the direct Vulkan 1.3 presentation of authored static geometry, bounded materials and changing door geometry, including validation, frame synchronization, swapchain recovery and explicit resource lifetime.

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
The renderer SHALL load scene SPIR-V shaders and required selected model/material resources from the explicit package root, upload correctly described static triangle and sampled material data, apply the runtime camera and current bounded lighting, clear the swapchain image, and render all selected generated geometry and static placements with correct depth and OPAQUE/MASK coverage. World geometry SHALL include authored solids and the optional switch, terrain only when present, and separately supplied P03 changing opaque presentation. Materials SHALL NOT change door motion, targeting or frame ownership. The editor SHALL preview renderable safe document fields without requiring successful gameplay validation. Empty generated or prop streams SHALL omit their allocations/draws safely while retaining other renderable content and UI. Aggregate expanded vertex/image byte counts and draw ranges SHALL be checked for overflow and supported allocation/draw limits before upload.

#### Scenario: First visible scene frame
- **WHEN** renderer initialization succeeds with valid runtime resources and the window has a non-zero framebuffer extent
- **THEN** the application presents the selected material-assigned generated world, static placements and supplied doors from the supplied camera pose over the configured clear color

#### Scenario: Camera frame changes
- **WHEN** the renderer receives a different valid camera frame
- **THEN** the next submitted world and prop draws use the new view/projection transform without recreating the graphics pipeline, immutable mesh buffers, sampled texture resources, or swapchain

#### Scenario: Scene shader asset is unavailable
- **WHEN** a required scene SPIR-V shader cannot be read beneath the configured resource root or cannot be used to create a shader module
- **THEN** startup fails with an error identifying the resolved shader asset path and releases all previously created Vulkan resources

#### Scenario: Scene texture asset is unavailable or invalid
- **WHEN** a required selected material texture cannot be read or decoded beneath the configured resource root
- **THEN** startup fails with an error identifying the resolved texture asset path and releases all previously created Vulkan resources

#### Scenario: Static model asset is unavailable or invalid
- **WHEN** any required selected static GLB cannot be read, validated, converted, or uploaded
- **THEN** startup fails with an error identifying the resolved model asset path and releases all previously created Vulkan resources

#### Scenario: Interior has no terrain
- **WHEN** a valid interior is rendered in the game or editor
- **THEN** its slabs, walls, stairs, authored static placements, switch and doors appear through the existing depth-tested textured lighting path without an invented terrain surface

#### Scenario: Invalid interior has no generated geometry
- **WHEN** an editable terrain-free document has no solids and no switch
- **THEN** the editor still displays any renderable props/doors, markers, validation feedback, and UI without allocating a zero-sized world buffer or submitting an invalid draw

### Requirement: Immutable opaque mesh buffer ownership
Every non-empty static generated or model-placement mesh SHALL have an explicit resource owner whose buffers and allocations remain valid until dependent draws finish. Static resource creation SHALL release partial construction exactly once and survive swapchain recreation when pipeline format remains compatible. Empty static streams SHALL require neither empty allocations nor draws. Model/material resources shared by repeated placements SHALL outlive all their users. Editor replacement between scene contents SHALL install resources transactionally after dependent GPU work completes and retain prior usable resources on failure. P03 changing geometry SHALL retain its separate frame-slot lifetime and SHALL NOT turn static meshes into per-frame mutable resources.

#### Scenario: Opaque scene frame is recorded
- **WHEN** the selected non-empty static mesh resources have initialized successfully
- **THEN** the renderer presents all intended static ranges with their correct material bindings and current bounded scene lighting

#### Scenario: Swapchain is recreated without a format change
- **WHEN** resize, out-of-date acquisition, or suboptimal presentation recreates swapchain-dependent resources with the existing format
- **THEN** no immutable static mesh is rebuilt or re-uploaded

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
The renderer SHALL decode only textures required by the selected scene before entering the frame loop, upload their validated OPAQUE/MASK base-color data into device-local sampled storage with complete mip chains, and own images, memory, views, samplers and material descriptors explicitly. Sampled material resources SHALL remain immutable during runtime and outlive every draw or descriptor that refers to them, including repeated placements and generated doors. Camera, door, point-light and spotlight updates SHALL NOT require texture decoding, texture upload or mutable material descriptors. Material data SHALL fit the existing Vulkan baseline without increasing mandatory push-constant capacity.

#### Scenario: Textured renderer starts
- **WHEN** all required selected material textures decode successfully and the selected device supports their required sampled and transfer operations
- **THEN** renderer initialization uploads complete sampled textures and binds the correct immutable material descriptors for the corresponding scene ranges

#### Scenario: Texture upload fails partway
- **WHEN** decoding, staging, image allocation, transfer, mip generation, image-view creation, sampler creation, or descriptor creation fails
- **THEN** startup reports the failed texture operation and releases every successfully created staging and persistent Vulkan resource exactly once

#### Scenario: Swapchain is recreated
- **WHEN** resize, out-of-date acquisition, or suboptimal presentation recreates swapchain-dependent resources
- **THEN** the immutable sampled texture images, views, samplers, and decoded material assignments remain valid without being decoded or uploaded again

#### Scenario: Required texture format operation is unavailable
- **WHEN** the selected physical device cannot sample, transfer, or generate the required mip chain for the selected supported texture format
- **THEN** renderer startup fails with an actionable message identifying the missing texture-format capability

#### Scenario: Cutout overlaps geometry during recovery
- **WHEN** the phone cord overlaps a contrasting surface while the game or editor resizes or restores
- **THEN** MASK-discarded fragments write no depth and surviving fragments retain correct occlusion, material sampling and lighting after recovery

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

### Requirement: Bounded changing opaque presentation
The renderer SHALL present the bounded changing opaque geometry supplied for each scene frame, including this milestone's generated door leaves and temporary feedback geometry, with correct world-space normals, opaque depth visibility, existing textures, and the current point-light, ambient, and optional spotlight state. Changing a door pose or feedback SHALL NOT rebuild or reupload the immutable world or prop meshes or recreate textures, immutable lighting resources, pipeline, or swapchain. The renderer SHALL NOT infer movement, locks, targeting, or feedback timing from gameplay. Per-frame changing geometry SHALL remain within the existing Vulkan baseline and frames-in-flight lifetime guarantees.

#### Scenario: Doors move repeatedly
- **WHEN** consecutive frames contain changing accepted leaf poses and feedback
- **THEN** each submitted frame uses its own supplied geometry with correct lighting and depth while static resources remain valid

#### Scenario: No doors are authored
- **WHEN** a scene frame has no changing opaque geometry
- **THEN** no empty allocation or invalid door draw is required and the static scene remains available

#### Scenario: Frame slot is reused
- **WHEN** a frame slot carrying earlier door vertices is reused
- **THEN** the earlier GPU work completes before those vertices or their allocations can be overwritten or destroyed

#### Scenario: Presentation recovers during door motion
- **WHEN** repeated motion spans resize, minimize/restore, out-of-date acquisition, or suboptimal presentation
- **THEN** the next submitted frame uses current supplied poses without stale vertices, resetting runtime state, or validation errors

#### Scenario: Changing geometry allocation fails
- **WHEN** allocation, mapping, upload, or scene replacement for door presentation fails
- **THEN** the failure is actionable, partial owners are released exactly once, and failed editor replacement retains the preceding usable resources
