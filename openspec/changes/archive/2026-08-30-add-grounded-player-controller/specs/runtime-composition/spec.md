## MODIFIED Requirements

### Requirement: Backend-neutral runtime boundary
Runtime and gameplay consumers SHALL be able to use the application and engine API without including, naming, or exchanging Vulkan, GLFW, Jolt, native-window, or other backend-specific types.

#### Scenario: Runtime consumer compiles
- **WHEN** a consumer translation unit includes only the public application or engine header
- **THEN** it compiles without directly including platform, rendering, or physics backend headers

#### Scenario: Dependency boundary is inspected
- **WHEN** project target dependencies and public headers are inspected
- **THEN** Vulkan, GLFW, and Jolt dependencies terminate at their implementation targets and are not part of the runtime consumer interface

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

