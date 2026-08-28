## ADDED Requirements

### Requirement: Camera-transformed prototype scene rendering
The renderer SHALL load the prototype scene SPIR-V shaders from the explicit runtime resource root, upload correctly described opaque scene vertex data, apply the engine-owned camera frame in the vertex stage, clear the swapchain image, and render the built-in static scene through a Vulkan graphics pipeline.

#### Scenario: First visible scene frame
- **WHEN** renderer initialization succeeds with a valid resource root and the window has a non-zero framebuffer extent
- **THEN** the application presents the built-in 3D scene from the supplied camera pose over the configured clear color

#### Scenario: Camera frame changes
- **WHEN** the renderer receives a different valid camera frame
- **THEN** the next submitted scene frame uses the new view/projection transform without recreating the graphics pipeline or swapchain

#### Scenario: Scene shader asset is unavailable
- **WHEN** a required prototype scene SPIR-V shader cannot be read beneath the configured resource root or cannot be used to create a shader module
- **THEN** startup fails with an error identifying the resolved shader asset path and releases all previously created Vulkan resources

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

## REMOVED Requirements

### Requirement: Triangle smoke rendering
**Reason**: The clip-space triangle is superseded by the camera-transformed, depth-buffered prototype scene as the renderer's visible smoke output.

**Migration**: Replace triangle-specific shaders, vertex fixtures, resource names, and smoke expectations with their prototype-scene equivalents while retaining executable-relative shader resolution and failure diagnostics.
