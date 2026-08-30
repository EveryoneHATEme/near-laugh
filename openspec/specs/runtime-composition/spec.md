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
The application SHALL supply the runtime with an explicit resource root, all required runtime assets SHALL be resolved from that root independently of the process working directory, and the project launcher SHALL derive its default resource root from the actual executable location rather than from the textual form of the process invocation.

#### Scenario: Runtime starts from another working directory
- **WHEN** the executable starts with a valid resource root while the process working directory is elsewhere
- **THEN** the renderer finds the required shader assets and starts normally

#### Scenario: Launcher is invoked through an indirect path
- **WHEN** the project launcher is started through a search path, alias, or invocation string that does not contain the executable directory
- **THEN** it derives the resource root from the actual executable location and finds the copied runtime assets

#### Scenario: Configured resource is missing
- **WHEN** a required shader is absent beneath the configured resource root
- **THEN** startup fails with an actionable error containing the resolved asset path

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

### Requirement: Fixed-step shooting-range coordination
The Engine-owned main-thread loop SHALL sample rifle input through the same first-person control-active decision used by player movement, SHALL retain trigger edges until a fixed step can process them, and SHALL coordinate player movement, recoil recovery, shot emission, closest-static-hit resolution, target damage, and feedback timing during each complete fixed simulation step. Rendering SHALL receive only the resulting camera and prototype-scene presentation after all complete steps for that iteration.

#### Scenario: Complete simulation step accepts a shot
- **WHEN** first-person controls are active, the rifle is ready, and a complete fixed step processes active or pending primary input
- **THEN** the runtime resolves one shot from the current simulated player aim, applies its closest hit to shooting-range state, applies recoil for subsequent aim, and exposes the resulting camera and target presentation to the next frame request

#### Scenario: Iteration has no complete simulation step
- **WHEN** primary input begins during a render-loop iteration that retains only a fractional fixed-step remainder
- **THEN** the runtime does not resolve a partial-step shot and retains the trigger press for a later complete step

#### Scenario: First-person controls are inactive
- **WHEN** the cursor is released or is being recaptured
- **THEN** movement and rifle input are neutral while physics, recoil recovery, target feedback timing, and other active fixed simulation continue

#### Scenario: Multiple fixed steps precede rendering
- **WHEN** one bounded loop interval produces multiple complete fixed steps
- **THEN** shooting and target state advance once per step in order and only the final resulting presentation is supplied to the iteration's single frame request

### Requirement: Gameplay-free render request
The runtime SHALL convert target hit and destruction state into a backend-neutral prototype-scene presentation containing only highlighted-solid and dimmed-solid masks. Frame requests and renderer interfaces SHALL NOT expose rifle state, target health, damage values, physics hits, or gameplay implementation types.

#### Scenario: Target state is prepared for rendering
- **WHEN** shooting-range state contains highlighted or destroyed targets
- **THEN** the frame request identifies their associated prototype solids through presentation masks without including gameplay state

#### Scenario: Runtime-render boundary is inspected
- **WHEN** frame and renderer-facing declarations are inspected
- **THEN** they contain no weapon, health, damage, or physics-library types
