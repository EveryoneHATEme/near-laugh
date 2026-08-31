## ADDED Requirements

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

## MODIFIED Requirements

### Requirement: Camera-transformed prototype scene rendering
The renderer SHALL load the prototype scene SPIR-V shaders and fixed prototype surface textures from the explicit runtime resource root, upload correctly described opaque scene vertex and sampled texture data, bind the immutable texture descriptor, apply the engine-owned camera frame in the vertex stage, clear the swapchain image, and render the built-in static scene through one Vulkan graphics pipeline draw.

#### Scenario: First visible scene frame
- **WHEN** renderer initialization succeeds with a valid resource root and the window has a non-zero framebuffer extent
- **THEN** the application presents the textured built-in 3D scene from the supplied camera pose over the configured clear color

#### Scenario: Camera frame changes
- **WHEN** the renderer receives a different valid camera frame
- **THEN** the next submitted scene frame uses the new view/projection transform without recreating the graphics pipeline, sampled texture resources, or swapchain

#### Scenario: Scene shader asset is unavailable
- **WHEN** a required prototype scene SPIR-V shader cannot be read beneath the configured resource root or cannot be used to create a shader module
- **THEN** startup fails with an error identifying the resolved shader asset path and releases all previously created Vulkan resources

#### Scenario: Scene texture asset is unavailable or invalid
- **WHEN** a required fixed prototype texture cannot be read or decoded beneath the configured resource root
- **THEN** startup fails with an error identifying the resolved texture asset path and releases all previously created Vulkan resources

### Requirement: Prototype solid-state presentation
The renderer SHALL accept backend-neutral highlighted-solid and dimmed-solid masks with each frame request and SHALL apply those states only to the corresponding authored prototype surfaces. Highlighting SHALL take visible precedence while active, dimming SHALL remain visible after highlighting ends, and unaffected surfaces SHALL retain their sampled texture, authored tint, and existing directional-plus-ambient lighting. Updating these masks SHALL NOT recreate or rewrite immutable scene geometry, update or recreate sampled texture resources or their descriptor, recreate the graphics pipeline or swapchain, or add another scene draw.

#### Scenario: Solid is highlighted
- **WHEN** a renderable frame identifies a textured prototype solid in the highlighted mask
- **THEN** the opaque scene draw presents that solid with the fixed visible highlight treatment

#### Scenario: Solid is dimmed
- **WHEN** a renderable frame identifies a textured prototype solid only in the dimmed mask
- **THEN** the opaque scene draw presents that solid with the fixed destroyed treatment

#### Scenario: Solid is highlighted and dimmed
- **WHEN** the same textured solid is present in both masks during its final-hit feedback interval
- **THEN** the highlight treatment takes precedence for that frame

#### Scenario: Presentation changes between frames
- **WHEN** highlighted or dimmed masks change in a later frame request
- **THEN** the next submitted scene draw uses the new presentation without changing immutable vertex data, texture resources, descriptors, or renderer-lifetime resources

#### Scenario: Target presentation uses opaque depth and lighting
- **WHEN** highlighted, dimmed, and unaffected textured target surfaces are rendered
- **THEN** they remain in the existing depth-tested opaque scene draw and preserve orientation-readable lighting
