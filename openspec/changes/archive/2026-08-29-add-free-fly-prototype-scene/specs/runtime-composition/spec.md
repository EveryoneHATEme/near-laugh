## MODIFIED Requirements

### Requirement: Main-thread loop ownership
The runtime SHALL keep event processing, close decisions, minimized-window waiting, input sampling, bounded frame timing, free-fly camera updates, and render coordination under the Engine-owned main-thread loop. Rendering SHALL NOT poll or wait for platform events, interpret FPS actions, update the camera, or decide whether the application exits.

#### Scenario: Normal loop iteration
- **WHEN** the window is open and has a non-zero framebuffer extent
- **THEN** the runtime processes events and input, updates the camera from a bounded elapsed interval, and supplies the resulting backend-neutral camera frame before requesting at most one rendered frame for that iteration

#### Scenario: Close is requested
- **WHEN** event processing reports a window close request
- **THEN** the runtime stops updating the camera or requesting new rendered frames and begins shutdown

#### Scenario: Window is minimized
- **WHEN** event processing reports a zero-sized framebuffer
- **THEN** the runtime waits for platform events without updating camera translation or submitting render work until a non-zero extent or close request is observed, and resets elapsed-time tracking so the blocked duration is not applied after restoration

