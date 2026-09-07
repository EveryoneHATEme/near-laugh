# runtime-composition Specification

## Purpose

Defines the backend-neutral runtime boundary that owns subsystem lifetime, explicit startup configuration, and main-thread coordination for the single-player game application.

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
The application SHALL supply the runtime with an explicit resource root and optional explicit level path and entry identifier using backend-neutral configuration. Required runtime shaders and the selected level's referenced models and materials SHALL resolve from that root independently of the process working directory. With no level override, the packaged prototype level SHALL resolve from that root; with an override, only the selected external level SHALL be required. The launcher SHALL derive its default resource root from the actual executable location rather than the textual form of the process invocation and SHALL resolve a relative level argument once before passing it to runtime composition. Entry resolution SHALL use the explicit identifier or the selected document's default and SHALL finish before level-dependent physics, player, or renderer construction. An invalid explicit selection SHALL NOT fall back to packaged content or another entry.

#### Scenario: Runtime starts from another working directory
- **WHEN** the executable starts with a valid resource root and either no level override or a resolved absolute level override while the process working directory is elsewhere
- **THEN** the runtime finds the selected level, shaders, and referenced packaged models/materials and starts normally at the resolved entry

#### Scenario: Launcher is invoked through an indirect path
- **WHEN** the project launcher is started through a search path, alias, or invocation string that does not contain the executable directory
- **THEN** it derives the resource root from the actual executable location and finds the copied runtime assets

#### Scenario: Configured resource is missing
- **WHEN** the selected level or a required shader, model or material resource is absent
- **THEN** startup fails with an actionable error containing the resolved missing asset path

#### Scenario: Unselected packaged level is absent
- **WHEN** a valid explicit external level is selected and the packaged prototype file is absent but all selected dependencies exist
- **THEN** startup succeeds without requiring or loading the unselected packaged prototype

#### Scenario: Entry selection fails
- **WHEN** the requested entry cannot be resolved in the validated selected document
- **THEN** startup reports the path and identifier, constructs no level-dependent consumer, and releases already-created runtime owners in dependency-safe order

#### Scenario: Unselected model is unavailable
- **WHEN** the catalog contains an unavailable model that the selected level does not reference
- **THEN** startup succeeds from its own complete dependency set without reading or requiring that model or the raw source pack

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
The runtime SHALL keep event processing, close decisions, minimized-window waiting, input sampling, bounded elapsed-time accumulation, fixed-step physics/player and door updates, look and cursor-capture updates, render-state interpolation, shared concrete interaction dispatch, run-local light and door state and feedback, and render coordination under the runtime-owned main-thread loop. Physics SHALL NOT control events, rendering, or application lifetime, and rendering SHALL NOT poll or wait for platform events, interpret player actions, update simulation or camera state, or decide whether the application exits.

#### Scenario: Normal loop iteration
- **WHEN** the window is open and has a non-zero framebuffer extent
- **THEN** the runtime processes events and input, advances every complete bounded fixed simulation step, interpolates the player camera from the remaining fraction, selects the current accepted door poses for both presentation and targeting, evaluates any eligible interaction press once using that view, and supplies the resulting backend-neutral camera, light, and changing opaque presentation before requesting at most one rendered frame for that iteration

#### Scenario: Close is requested
- **WHEN** event processing reports a window close request
- **THEN** the runtime stops updating simulation or requesting new rendered frames and begins shutdown

#### Scenario: Window is minimized
- **WHEN** event processing reports a zero-sized framebuffer
- **THEN** the runtime waits for platform events without updating simulation or submitting render work until a non-zero extent or close request is observed, consumes interaction presses from waited input batches without activating or retaining them, and resets elapsed-time accumulation so the blocked duration is not applied after restoration

### Requirement: Renderer outcome coordination
Each frame request SHALL produce a runtime-owned outcome that distinguishes rendered work, temporarily skipped work, and swapchain recovery. The runtime SHALL consume that outcome before deciding how the application loop proceeds and SHALL retain event processing and application lifetime ownership without inspecting Vulkan results.

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
- **THEN** their spot-light contract contains only project-owned scalar lighting data and no gameplay, physics-library, platform, or graphics-backend types

### Requirement: Gameplay-independent point-light enable request
For every scene frame, the runtime SHALL supply backend-neutral enabled state for every light in the loaded bounded authored set, initialized from each light's own initial value. The request SHALL NOT expose switch definitions, input actions, physics hits, gameplay controllers, or backend types. The number and order of submitted enables SHALL match the immutable light set exactly; missing or mismatched data SHALL be rejected before GPU submission, and the empty set SHALL be valid. The runtime SHALL retain light state across renderer outcomes; the renderer SHALL only present the supplied state.

#### Scenario: Switch changes a light
- **WHEN** runtime interaction changes the linked point-light state
- **THEN** the next frame request contains the resulting point-light enable values without identifying the gameplay source

#### Scenario: Frame is skipped or recovered
- **WHEN** a request after a toggle results in skipped work or presentation recovery
- **THEN** the runtime retains its current light state and includes it in the next requested frame without replaying the press

#### Scenario: Default frame has no overrides
- **WHEN** a caller supplies no point-light enable data for a nonempty authored light set
- **THEN** the boundary rejects the incomplete request before submission; runtime startup must explicitly initialize enables from the authored light values

#### Scenario: Enable data does not match the scene
- **WHEN** a frame supplies fewer or more enable values than the immutable light set
- **THEN** the boundary reports the mismatch before submission rather than enabling a different light or reading outside the data

### Requirement: Explicit run-local door coordination
The runtime SHALL own mutable door motion, lock state, and temporary feedback independently from immutable authored definitions and renderer/physics resources. It SHALL advance motion and feedback only within the bounded fixed simulation steps, dispatch each sampled action batch once after simulation, and provide the same accepted leaf pose to collision, target queries, and presentation. Player camera interpolation SHALL remain safe with these current leaf poses. Minimized waits SHALL pause door motion and feedback and SHALL NOT accumulate catch-up or delayed actions. Presentation outcomes SHALL NOT reset state or replay concrete results.

#### Scenario: Frame has no fixed step
- **WHEN** an eligible door action occurs in a renderable batch containing zero fixed steps
- **THEN** its request is consumed once without immediate angular movement and later fixed steps advance that request

#### Scenario: Multiple doors move
- **WHEN** more than one authored door requests movement in a fixed step
- **THEN** accepted poses are deterministic and collision and presentation agree without modifying authored definitions

#### Scenario: Interpolated player view trails movement
- **WHEN** a door could move into space between the player's previous and current presentation poses
- **THEN** accepted motion preserves a clear displayed player view rather than putting its interpolated eye inside the leaf

### Requirement: Gameplay-independent changing opaque request
Frame data SHALL contain only bounded backend-neutral geometric presentation data for changing opaque content, without door actions, locks, durable gameplay identifiers, controllers, physics hits, native bodies, or Vulkan types. Gameplay SHALL resolve temporary feedback to presentation before submission; rendering SHALL only consume that presentation. Authored immutable scene resources SHALL remain separately owned.

#### Scenario: Door frame is prepared
- **WHEN** runtime state changes a door pose or temporary visual feedback
- **THEN** the next frame request describes the resulting geometry and appearance without asking the renderer to interpret the gameplay result

### Requirement: Owned audio and cue coordination
Runtime composition SHALL own audio playback, cue time, source overrides, caption selection and mute/suspension state independently of immutable level definitions. Audio backend types SHALL remain outside public runtime, world and frame boundaries. The listener SHALL use the current displayed eye pose and transmission SHALL use accepted door poses. Selected audio/caption/font dependencies SHALL resolve from the executable-relative resource root and validate before playback, preserving native Unicode paths. Device status SHALL NOT control application lifetime or story ordering. Partial startup and normal shutdown SHALL release audio users before their referenced data and stop device activity before destruction.

#### Scenario: Current view and door state are submitted
- **WHEN** a renderable iteration has an interpolated view and accepted door state
- **THEN** audio uses those same poses while the renderer receives only resolved text presentation and existing geometric/light data

#### Scenario: Resource path contains Unicode
- **WHEN** the selected level and executable resources reside under paths with non-ASCII characters
- **THEN** selected clips, captions and font load using their literal resolved paths and failures identify those paths

#### Scenario: Startup fails after audio initialization
- **WHEN** renderer initialization fails after audio resources have initialized
- **THEN** all playback activity stops and audio resources and prior owners are released safely without entering the main loop

### Requirement: Audio coordination across waits and presentation outcomes
Before entering a minimized event wait, runtime composition SHALL suspend audio and cue time, retaining cue offsets and identities. Restoration SHALL resume without counting blocked time. Existing inactive-input consumption SHALL prevent delayed interaction or fixture actions. Rendered, skipped and recovered outcomes SHALL NOT replay cues, reset source motion or determine cue completion. Close SHALL prevent further cue starts and begin orderly teardown. Cursor release SHALL retain its existing control behavior without becoming a session pause.

#### Scenario: Window waits while a cue is active
- **WHEN** the framebuffer becomes zero during speech
- **THEN** audio and captions suspend before the event wait and restoration continues the same cue from the retained offset

#### Scenario: Swapchain recovers after a completed cue
- **WHEN** presentation recovery follows a completed one-shot
- **THEN** the next frame contains only currently active text and the completed audio is not started again

#### Scenario: Close occurs while minimized
- **WHEN** a close event ends a minimized wait with suspended voices
- **THEN** shutdown releases those voices without resuming them or replaying waited input
