## MODIFIED Requirements

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
