## MODIFIED Requirements

### Requirement: Main-thread loop ownership
The runtime SHALL keep event processing, close decisions, minimized-window waiting, input sampling, bounded elapsed-time accumulation, fixed-step physics/player updates, look and cursor-capture updates, render-state interpolation, concrete switch interaction and run-local light state, and render coordination under the runtime-owned main-thread loop. Physics SHALL NOT control events, rendering, or application lifetime, and rendering SHALL NOT poll or wait for platform events, interpret player actions, update simulation or camera state, or decide whether the application exits.

#### Scenario: Normal loop iteration
- **WHEN** the window is open and has a non-zero framebuffer extent
- **THEN** the runtime processes events and input, advances every complete bounded fixed simulation step, interpolates the player camera from the remaining fraction, evaluates any eligible interaction press once using that view, and supplies the resulting backend-neutral camera frame before requesting at most one rendered frame for that iteration

#### Scenario: Close is requested
- **WHEN** event processing reports a window close request
- **THEN** the runtime stops updating simulation or requesting new rendered frames and begins shutdown

#### Scenario: Window is minimized
- **WHEN** event processing reports a zero-sized framebuffer
- **THEN** the runtime waits for platform events without updating simulation or submitting render work until a non-zero extent or close request is observed, consumes interaction presses from waited input batches without activating or retaining them, and resets elapsed-time accumulation so the blocked duration is not applied after restoration

## ADDED Requirements

### Requirement: Gameplay-independent point-light enable request
For every scene frame, the runtime SHALL supply backend-neutral enabled state for each of the two authored point-light slots, defaulting to enabled. The request SHALL NOT expose switch definitions, input actions, physics hits, gameplay controllers, or backend types. The runtime SHALL retain light state across renderer outcomes; the renderer SHALL only present the supplied state.

#### Scenario: Switch changes a light
- **WHEN** runtime interaction changes the linked point-light state
- **THEN** the next frame request contains the resulting point-light enable values without identifying the gameplay source

#### Scenario: Frame is skipped or recovered
- **WHEN** a request after a toggle results in skipped work or presentation recovery
- **THEN** the runtime retains its current light state and includes it in the next requested frame without replaying the press

#### Scenario: Default frame has no overrides
- **WHEN** a caller supplies no explicit point-light enable overrides
- **THEN** both authored point lights are enabled independently of whether a spotlight is present
