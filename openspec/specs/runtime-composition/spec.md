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

### Requirement: Gameplay-independent animated pose request
Animated presentation SHALL contain bounded backend-neutral model-instance selection, finite world placement and validated skeletal pose data for zero through four selected character instances. It SHALL contain no actor actions, route IDs, native physics objects or animation-loader/backend types. Its selected resource and pose sizes SHALL be validated together before submission, with no unknown, duplicate or mismatched instance data. Borrowed frame data SHALL be consumed synchronously without retaining caller storage. The caller SHALL own time and transitions; rendering outcomes SHALL NOT advance or restart animation.

#### Scenario: Caller supplies four poses
- **WHEN** four selected instances supply valid independent skeletal poses
- **THEN** rendering consumes each supplied placement/pose without interpreting route policy or sharing mutable playback state

#### Scenario: Supplied pose is incompatible
- **WHEN** a selected instance lacks its pose, has an invalid transform or references the wrong skeleton size
- **THEN** presentation fails before GPU submission with the affected instance identified

#### Scenario: Presentation is skipped
- **WHEN** a frame is skipped or recovered
- **THEN** the next request uses the caller's current pose without renderer-generated animation advancement or replay

### Requirement: Owned run-local character coordination
Runtime composition SHALL resolve validated character definitions and selected resources before simulation, own independent mutable actor action/pose state and release all users before their referenced owners. Character motion, turning, phase transitions and action-marker decisions SHALL advance on the existing bounded one-sixtieth-second simulation steps. Each step SHALL coordinate the player, actors and doors to preserve collision and presentation safety; action outcomes SHALL not depend on rendering frequency or collection order for equivalent fixed steps and inputs. Characters SHALL use current accepted feet/yaw for collision, rendering and audio with no separate actor placement interpolation ahead of collision. Light, door and actor presentation SHALL describe one coherent accepted state.

#### Scenario: Several simulation steps precede rendering
- **WHEN** one frame contains multiple fixed steps or another contains none
- **THEN** route decisions and contact identities follow completed simulation steps, while the frame presents the resulting accepted actor poses without duplicate action dispatch

#### Scenario: Actor initialization fails
- **WHEN** selected character preparation or the third actor's physics creation fails
- **THEN** runtime reports the model/actor context, enters no frame loop and releases all previously created resources safely

### Requirement: Character suspension and fresh-run semantics
Explicit development suspension and minimization SHALL freeze player/door/actor simulation, clip transitions, pending marker retries and cue time together, consume inactive input and discard suspended wall time. Resume SHALL retain accepted poses and action/cue identities without catch-up or duplicate markers. Cursor release alone SHALL retain the current ordinary game policy of allowing world and audio time to continue. Active long frame intervals SHALL retain the existing 100 ms simulation contribution cap; already-started P04 cues SHALL retain their uncapped active-time policy, and missing simulation time SHALL NOT generate overdue walking events. Fresh process entry SHALL reconstruct authored initial actors/routes once. Presentation recovery SHALL preserve running state; cancellation or completed actions SHALL not be replayed. Inspection controls SHALL remain a development entry, with no new player-facing pause menu required.

#### Scenario: Window is minimized at an interaction marker
- **WHEN** the actor is waiting for a cue or has just started it and the window minimizes
- **THEN** actor and cue time freeze together and restoration resumes the same marker/cue state once

#### Scenario: Active frame stalls
- **WHEN** a renderable interval exceeds the bounded simulation contribution
- **THEN** accepted movement advances by only the bounded fixed steps, existing sounds use their defined active time, and no discarded-motion footsteps or artificial arrivals are emitted

#### Scenario: Game starts again
- **WHEN** a run with moved or canceled actors exits and a new process loads the same level
- **THEN** the file is unchanged and authored initial actor poses and initial routes are restored

### Requirement: Owned physical household coordination
Runtime composition SHALL initialize validated household definitions and selected text/audio resources before their consumers, own concrete box holding/radio/reading state independently of the immutable level, and coordinate it on the existing main thread. The physics owner SHALL be the authority for actual box poses, velocities and sleeping state. Accepted pickup/drop/throw requests SHALL be consumed once and applied at the next fixed boundary before world advancement; they SHALL NOT repeat across render batches or survive suspension as delayed actions. Hold targets SHALL derive from the accepted simulation eye/look at each fixed boundary rather than the render-interpolated camera. A queued throw SHALL retain its accepted direction instead of resampling aim on execution. The player, actors and doors SHALL share a coherent view of current box collision. Targeting, visible geometry and shadows SHALL use the same accepted box pose, including full rotation, without separate visual motion ahead of collision.

#### Scenario: An action arrives in a zero-step batch
- **WHEN** a pickup or throw press is accepted in a renderable batch with no fixed step
- **THEN** it is retained at most once for the next fixed boundary, its eligibility is rechecked there, and presentation shows actual current state until the action is applied

#### Scenario: Suspension interrupts a pending request
- **WHEN** a pending household command has not reached its fixed boundary before controls are suspended, minimized or closed
- **THEN** the ordinary input command is discarded without a later throw, acquisition or duplicate state transition; an already-required safety release remains owed before the next advancing step

#### Scenario: Rendering is skipped after a physics action
- **WHEN** physics has applied a throw but presentation is skipped or recovered
- **THEN** subsequent frames consume that same body's current state without applying another impulse

### Requirement: Household frame and teardown boundary
Frame presentation SHALL contain only bounded resolved geometric/text data, with no household controllers, durable gameplay IDs, body handles, throw requests or document-navigation actions. The renderer SHALL consume borrowed data synchronously without retaining caller storage. Household radio changes SHALL reach audio before frame captions are resolved. Hold constraints and pending commands SHALL be released before referenced physics bodies; audio users and reading views SHALL end before their content owners. Partial startup and failed physical hold acquisition SHALL release acquired resources exactly once, with no half-installed ownership or main-loop use of invalid bodies.

#### Scenario: Radio stops in a displayed batch
- **WHEN** an accepted radio-off action is presented
- **THEN** that frame's resolved feedback and captions agree with the canceled source

#### Scenario: Holding resource creation fails
- **WHEN** hold setup fails after some dependent resources are acquired
- **THEN** the free box remains at its physical pose with no ownership transfer, partial resources are released and a later eligible retry remains possible

### Requirement: Mandatory safety release across suspension
A release already required by loss of active carrying controls SHALL remain distinct from pending input commands. It SHALL complete at the actual body pose before the next advancing physics step, even if a zero-step batch, minimization, suspension or recapture intervenes. Suspension without an earlier release obligation SHALL preserve the held body and hold relationship. Completing a safety release SHALL never add a throw impulse or reacquire a box automatically.

#### Scenario: Cursor loss precedes minimization without a step
- **WHEN** cursor release requires a drop, no fixed step runs, the window minimizes and capture is restored before the next advancing step
- **THEN** the previously owed safe release still completes once before physics advances, with no throw or automatic reacquisition
