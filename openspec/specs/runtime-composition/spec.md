# runtime-composition Specification

## Purpose

Defines the backend-neutral runtime boundary that owns subsystem lifetime, explicit startup configuration, and main-thread coordination for the single-player FPS application.

## Requirements

### Requirement: Backend-neutral runtime boundary
Runtime and gameplay consumers SHALL be able to use the application and engine API without including, naming, or exchanging Vulkan, GLFW, Jolt, native-window, or other backend-specific types.

#### Scenario: Runtime consumer compiles
- **WHEN** a consumer translation unit includes only the public application or engine header
- **THEN** it compiles without directly including platform, rendering, or physics backend headers

#### Scenario: Dependency boundary is inspected
- **WHEN** project target dependencies and public headers are inspected
- **THEN** Vulkan, GLFW, and Jolt dependencies terminate at their implementation targets and are not part of the runtime consumer interface

### Requirement: Explicit runtime configuration
The application SHALL supply the runtime with an explicit resource root; all required runtime shader, fixed prototype-texture, static model, and level assets SHALL be resolved from that root independently of the process working directory; and the project launcher SHALL derive its default resource root from the actual executable location rather than from the textual form of the process invocation.

#### Scenario: Runtime starts from another working directory
- **WHEN** the executable starts with a valid resource root while the process working directory is elsewhere
- **THEN** the runtime finds the required level, shaders, fixed prototype textures, and packaged static GLB and starts normally

#### Scenario: Launcher is invoked through an indirect path
- **WHEN** the project launcher is started through a search path, alias, or invocation string that does not contain the executable directory
- **THEN** it derives the resource root from the actual executable location and finds the copied runtime assets

#### Scenario: Configured resource is missing
- **WHEN** a required level, shader, fixed prototype texture, or static GLB is absent beneath the configured resource root
- **THEN** startup fails with an actionable error containing the resolved missing asset path

### Requirement: Explicit subsystem lifetime
The runtime SHALL initialize its platform owner before creating a window, create the immutable prototype world before the physics state and renderer that consume it, and destroy those subsystems in reverse dependency order. A partially completed startup SHALL release every successfully initialized subsystem exactly once.

#### Scenario: Successful runtime lifetime
- **WHEN** the application starts and later shuts down normally
- **THEN** rendering is destroyed before physics, physics is destroyed before the prototype world, and the world and window are destroyed before platform termination

#### Scenario: Physics startup fails
- **WHEN** physics creation fails after the platform, window, and prototype world have initialized
- **THEN** the world, window, and platform are released in dependency-safe order without creating the renderer or entering the main loop

#### Scenario: Renderer startup fails
- **WHEN** renderer creation fails after platform, window, world, and physics initialization
- **THEN** physics, world, window, and platform are released in dependency-safe order without entering the main loop

### Requirement: Main-thread loop ownership
The runtime SHALL keep event processing, close decisions, minimized-window waiting, input sampling, bounded elapsed-time accumulation, fixed-step physics/player updates, look and cursor-capture updates, render-state interpolation, and render coordination under the Engine-owned main-thread loop. Physics SHALL NOT control events, rendering, or application lifetime, and rendering SHALL NOT poll or wait for platform events, interpret FPS actions, update simulation or camera state, or decide whether the application exits.

#### Scenario: Normal loop iteration
- **WHEN** the window is open and has a non-zero framebuffer extent
- **THEN** the runtime processes events and input, advances every complete bounded fixed simulation step, interpolates the player camera from the remaining fraction, and supplies the resulting backend-neutral camera frame before requesting at most one rendered frame for that iteration

#### Scenario: Close is requested
- **WHEN** event processing reports a window close request
- **THEN** the runtime stops updating simulation or requesting new rendered frames and begins shutdown

#### Scenario: Window is minimized
- **WHEN** event processing reports a zero-sized framebuffer
- **THEN** the runtime waits for platform events without updating simulation or submitting render work until a non-zero extent or close request is observed, and resets elapsed-time accumulation so the blocked duration is not applied after restoration

### Requirement: Renderer outcome coordination
Each frame request SHALL produce an engine-owned outcome that distinguishes rendered work, temporarily skipped work, and swapchain recovery. The runtime SHALL consume that outcome before deciding how the application loop proceeds and SHALL retain event processing and application lifetime ownership without inspecting Vulkan results.

#### Scenario: Frame is rendered
- **WHEN** image acquisition, submission, and presentation succeed
- **THEN** the runtime consumes a rendered outcome and proceeds to the next runtime-controlled loop iteration

#### Scenario: Rendering is temporarily unavailable
- **WHEN** rendering cannot proceed because the surface extent is zero or swapchain recovery is required
- **THEN** the runtime consumes a non-fatal skipped or recovered outcome and retains control of event processing and application lifetime

### Requirement: Gameplay-independent spot-light render request
The runtime SHALL supply at most one optional dynamic spot-light description containing only backend-neutral position, direction, range, cone, color, intensity, and enabled state. Frame requests and renderer interfaces SHALL NOT expose player, flashlight, weapon, target, health, damage, physics-hit, or other gameplay implementation types, and the renderer SHALL NOT infer a spot-light pose from the camera matrix.

#### Scenario: Active spot light is prepared for rendering
- **WHEN** runtime state selects an enabled spot light for a renderable frame
- **THEN** the frame request contains its current validated world-space lighting description without identifying its gameplay source

#### Scenario: No spot light is active
- **WHEN** runtime state selects no enabled dynamic spot light
- **THEN** the frame request represents no dynamic spot-light contribution without exposing gameplay state

#### Scenario: Runtime-render boundary is inspected
- **WHEN** frame and renderer-facing declarations are inspected
- **THEN** their spot-light contract contains only engine-owned scalar lighting data and no gameplay, physics-library, platform, or graphics-backend types
