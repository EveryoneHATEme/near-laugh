## Purpose

Defines the concrete keyboard and mouse action state needed by the one local first-person player without exposing platform-library key codes or building a general input framework.

## ADDED Requirements

### Requirement: FPS action snapshot
The input component SHALL expose a per-iteration snapshot containing movement, jump, sprint, crouch, menu, primary-action, secondary-action, and first-person look-delta state for exactly one local player.

#### Scenario: Movement and stance actions are sampled
- **WHEN** the player holds the configured movement, jump, sprint, or crouch controls during an event batch
- **THEN** the corresponding FPS actions are active in the next input snapshot

#### Scenario: Mouse actions are sampled
- **WHEN** the player holds the primary or secondary mouse control
- **THEN** the corresponding primary-action or secondary-action state is active in the next input snapshot

### Requirement: Default FPS controls
The runtime SHALL map W, A, S, and D to forward, left, backward, and right movement; Space to jump; Left Shift to sprint; Left Control to crouch; Escape to menu; and the left and right mouse buttons to primary and secondary actions.

#### Scenario: Default keyboard mapping
- **WHEN** physical W, Space, and Left Shift are down in the platform snapshot
- **THEN** forward movement, jump, and sprint are active in the FPS action snapshot

#### Scenario: Default mouse mapping
- **WHEN** the physical left mouse button is down
- **THEN** the primary FPS action is active

### Requirement: First-person look delta
The input component SHALL report mouse look movement accumulated during the current processed event batch and SHALL reset that delta before the next batch without clearing held action state.

#### Scenario: Cursor moves within one event batch
- **WHEN** multiple cursor positions are reported during an event batch
- **THEN** the FPS input snapshot contains their accumulated relative movement

#### Scenario: A new event batch begins
- **WHEN** the runtime begins processing the next event batch
- **THEN** look delta starts at zero while held keyboard and mouse actions remain active

### Requirement: Platform-independent input contract
FPS input consumers SHALL NOT include or exchange GLFW, native-key, or native-mouse constants; platform physical state SHALL be translated before the action snapshot reaches runtime consumers.

#### Scenario: Input consumer boundary is inspected
- **WHEN** gameplay-facing input headers and tests are inspected
- **THEN** they use only engine-owned physical and FPS action types

