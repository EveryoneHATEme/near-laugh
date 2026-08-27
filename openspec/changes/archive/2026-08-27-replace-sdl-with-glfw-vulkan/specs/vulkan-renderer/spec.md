## Purpose

Defines the direct Vulkan 1.3 renderer foundation, including device selection, validation, frame synchronization, swapchain recovery, resource lifetime, and the initial triangle smoke output.

## ADDED Requirements

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

### Requirement: Development validation
Development builds SHALL request the Khronos validation layer when it is available, install a Vulkan debug messenger, and report validation messages with severity and category. Validation errors SHALL be treated as defects.

#### Scenario: Validation layer is available
- **WHEN** a development build starts on a system with the Khronos validation layer
- **THEN** validation and the debug messenger are enabled before device creation

#### Scenario: Validation layer is unavailable
- **WHEN** a development build starts without the Khronos validation layer
- **THEN** the application emits a clear warning and continues only if all non-validation Vulkan requirements are met

#### Scenario: Vulkan operation fails
- **WHEN** a required Vulkan operation returns an error result
- **THEN** the application reports the operation and decoded Vulkan result before performing orderly cleanup

### Requirement: Explicit frame lifecycle
The renderer SHALL use a small fixed number of frames in flight, with each frame owning its command resources and synchronization objects. CPU reuse of a frame SHALL wait until the GPU has completed that frame.

#### Scenario: Frame slot is reused
- **WHEN** the renderer advances to a frame slot that was previously submitted
- **THEN** it waits for that slot's completion fence before resetting or modifying its resources

#### Scenario: Image is submitted and presented
- **WHEN** a swapchain image is acquired successfully
- **THEN** the renderer records commands with Dynamic Rendering, submits them using Synchronization 2-compatible synchronization, and presents the image with correct semaphore ordering

### Requirement: Swapchain lifecycle
The renderer SHALL create a swapchain from current surface capabilities and SHALL recreate swapchain-dependent resources after resize, out-of-date, or suboptimal presentation without recreating instance- or device-lifetime resources.

#### Scenario: Window framebuffer is resized
- **WHEN** the framebuffer receives a new non-zero extent
- **THEN** the renderer waits for affected GPU work and recreates the swapchain and dependent image views for that extent

#### Scenario: Swapchain becomes out of date
- **WHEN** image acquisition or presentation reports an out-of-date swapchain
- **THEN** the current frame is abandoned safely and swapchain recreation occurs before the next rendered frame

#### Scenario: Swapchain becomes suboptimal
- **WHEN** image acquisition or presentation reports a suboptimal swapchain
- **THEN** the renderer completes safe work and schedules swapchain recreation without terminating the application

### Requirement: Triangle smoke rendering
The renderer SHALL load the project SPIR-V shaders, upload correctly described vertex data, clear the swapchain image, and render the existing colored triangle through a Vulkan graphics pipeline.

#### Scenario: First visible frame
- **WHEN** renderer initialization succeeds and the window has a non-zero framebuffer extent
- **THEN** the application presents a frame containing the colored triangle over the configured clear color

#### Scenario: Shader asset is unavailable
- **WHEN** a required SPIR-V shader cannot be read or used to create a shader module
- **THEN** startup fails with an error identifying the shader asset and releases all previously created Vulkan resources

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

