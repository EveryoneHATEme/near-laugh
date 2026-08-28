# runtime-composition Specification

## Purpose

Defines the backend-neutral runtime boundary that owns subsystem lifetime, explicit startup configuration, and main-thread coordination for the single-player FPS application.

## Requirements

### Requirement: Backend-neutral runtime boundary
Runtime and gameplay consumers SHALL be able to use the application and engine API without including, naming, or exchanging Vulkan, GLFW, native-window, or other backend-specific types.

#### Scenario: Runtime consumer compiles
- **WHEN** a consumer translation unit includes only the public application or engine header
- **THEN** it compiles without directly including platform or rendering backend headers

#### Scenario: Dependency boundary is inspected
- **WHEN** project target dependencies and public headers are inspected
- **THEN** Vulkan and GLFW dependencies terminate at their implementation targets and are not part of the runtime consumer interface

### Requirement: Explicit runtime configuration
The application SHALL supply the runtime with an explicit resource root, and all required runtime assets SHALL be resolved from that root independently of the process working directory.

#### Scenario: Runtime starts from another working directory
- **WHEN** the executable starts with a valid resource root while the process working directory is elsewhere
- **THEN** the renderer finds the required shader assets and starts normally

#### Scenario: Configured resource is missing
- **WHEN** a required shader is absent beneath the configured resource root
- **THEN** startup fails with an actionable error containing the resolved asset path

### Requirement: Explicit subsystem lifetime
The runtime SHALL initialize its platform owner before creating a window, create rendering only after the window exists, and destroy those subsystems in reverse dependency order. A partially completed startup SHALL release every successfully initialized subsystem exactly once.

#### Scenario: Successful runtime lifetime
- **WHEN** the application starts and later shuts down normally
- **THEN** rendering is destroyed before the window, and the window is destroyed before platform termination

#### Scenario: Renderer startup fails
- **WHEN** renderer creation fails after the platform and window have been initialized
- **THEN** the window and platform are released in dependency-safe order without entering the main loop

### Requirement: Main-thread loop ownership
The runtime SHALL keep event processing, close decisions, minimized-window waiting, input sampling, and render coordination under the Engine-owned main-thread loop. Rendering SHALL NOT poll or wait for platform events or decide whether the application exits.

#### Scenario: Normal loop iteration
- **WHEN** the window is open and has a non-zero framebuffer extent
- **THEN** the runtime processes events and input before requesting at most one rendered frame for that iteration

#### Scenario: Close is requested
- **WHEN** event processing reports a window close request
- **THEN** the runtime stops requesting new rendered frames and begins shutdown

#### Scenario: Window is minimized
- **WHEN** event processing reports a zero-sized framebuffer
- **THEN** the runtime waits for platform events without submitting render work until a non-zero extent or close request is observed

### Requirement: Renderer outcome coordination
Each frame request SHALL produce an engine-owned outcome that distinguishes rendered work, temporarily skipped work, and swapchain recovery so the runtime can coordinate the next loop iteration without inspecting Vulkan results.

#### Scenario: Frame is rendered
- **WHEN** image acquisition, submission, and presentation succeed
- **THEN** the runtime receives a rendered outcome

#### Scenario: Rendering is temporarily unavailable
- **WHEN** rendering cannot proceed because the surface extent is zero or swapchain recovery is required
- **THEN** the runtime receives a non-fatal outcome and retains control of event processing and application lifetime
