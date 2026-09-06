## MODIFIED Requirements

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


## ADDED Requirements

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
