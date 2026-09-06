## MODIFIED Requirements

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
