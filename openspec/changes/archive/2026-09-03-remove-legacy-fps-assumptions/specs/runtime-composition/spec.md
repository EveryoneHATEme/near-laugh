## MODIFIED Requirements

### Requirement: Main-thread loop ownership
The runtime SHALL keep event processing, close decisions, minimized-window waiting, input sampling, bounded elapsed-time accumulation, fixed-step physics/player updates, look and cursor-capture updates, render-state interpolation, and render coordination under the runtime-owned main-thread loop. Physics SHALL NOT control events, rendering, or application lifetime, and rendering SHALL NOT poll or wait for platform events, interpret player actions, update simulation or camera state, or decide whether the application exits.

#### Scenario: Normal loop iteration
- **WHEN** the window is open and has a non-zero framebuffer extent
- **THEN** the runtime processes events and input, advances every complete bounded fixed simulation step, interpolates the player camera from the remaining fraction, and supplies the resulting backend-neutral camera frame before requesting at most one rendered frame for that iteration

#### Scenario: Close is requested
- **WHEN** event processing reports a window close request
- **THEN** the runtime stops updating simulation or requesting new rendered frames and begins shutdown

#### Scenario: Window is minimized
- **WHEN** event processing reports a zero-sized framebuffer
- **THEN** the runtime waits for platform events without updating simulation or submitting render work until a non-zero extent or close request is observed, and resets elapsed-time accumulation so the blocked duration is not applied after restoration
