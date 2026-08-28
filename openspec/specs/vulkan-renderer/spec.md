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

### Requirement: Triangle smoke rendering
The renderer SHALL load the project SPIR-V shaders from the explicit runtime resource root, upload correctly described vertex data, clear the swapchain image, and render the existing colored triangle through a Vulkan graphics pipeline.

#### Scenario: First visible frame
- **WHEN** renderer initialization succeeds with a valid resource root and the window has a non-zero framebuffer extent
- **THEN** the application presents a frame containing the colored triangle over the configured clear color

#### Scenario: Shader asset is unavailable
- **WHEN** a required SPIR-V shader cannot be read beneath the configured resource root or cannot be used to create a shader module
- **THEN** startup fails with an error identifying the resolved shader asset path and releases all previously created Vulkan resources

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
