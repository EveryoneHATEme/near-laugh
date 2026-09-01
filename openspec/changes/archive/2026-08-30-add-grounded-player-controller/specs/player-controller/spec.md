## Purpose

Defines the one local first-person player's collision-constrained movement, stance, gravity, jumping, air control, look orientation, and camera placement for the built-in FPS prototype environment.

## ADDED Requirements

### Requirement: Single grounded player
The runtime SHALL maintain exactly one local player represented by an upright gameplay-oriented capsule, SHALL spawn that player at a non-overlapping scene-facing position above the prototype floor, and SHALL derive grounded state from physics contacts rather than camera position.

#### Scenario: Player starts
- **WHEN** the prototype physics world and static level collision have initialized
- **THEN** one standing player capsule starts at the configured spawn without intersecting static geometry and settles onto the floor under gravity

### Requirement: Ground movement
While first-person control is active, the player SHALL move horizontally relative to current yaw from the forward, backward, left, and right FPS actions, SHALL use a faster configured speed while sprint is active, and SHALL normalize combined movement axes so diagonal input does not exceed the selected speed. Ground movement SHALL collide with and slide along static geometry, traverse the prototype's configured walkable step, and SHALL NOT pass through blocking structures.

#### Scenario: Player walks relative to view
- **WHEN** forward input remains active while the player faces away from the initial yaw and is supported on walkable ground
- **THEN** the player advances in the current horizontal forward direction at the configured walking speed

#### Scenario: Player sprints
- **WHEN** sprint and horizontal movement are active while the player is supported on walkable ground
- **THEN** the player moves at the configured sprint speed without changing collision behavior

#### Scenario: Diagonal movement is normalized
- **WHEN** two horizontal movement axes are active during the same simulation step
- **THEN** the requested horizontal speed does not exceed the selected walking or sprint speed

#### Scenario: Player reaches blocking geometry
- **WHEN** requested movement points into a floor-level wall or obstacle
- **THEN** the player remains outside the solid and preserves the valid component of movement along its surface

#### Scenario: Player reaches a walkable step
- **WHEN** grounded movement reaches the prototype's configured low step with sufficient clearance above it
- **THEN** the player traverses the step without jumping or becoming embedded

### Requirement: Gravity, jumping, and air movement
The player SHALL accelerate downward while unsupported, SHALL remain supported without accumulating downward speed on walkable ground, SHALL apply one jump impulse only when a new jump press is observed while grounded, and SHALL provide limited horizontal control while airborne. Holding jump SHALL NOT automatically produce another jump after landing until the action has first been released.

#### Scenario: Unsupported player falls
- **WHEN** the player is not supported by walkable ground
- **THEN** downward velocity increases under gravity until collision or another supported state changes it

#### Scenario: Grounded player jumps
- **WHEN** jump transitions from inactive to active while the player is grounded and standing is permitted
- **THEN** one upward impulse makes the player airborne

#### Scenario: Jump is held through landing
- **WHEN** jump remains active from an earlier jump until the player lands
- **THEN** the player does not jump again without a release followed by a new press

#### Scenario: Player steers in air
- **WHEN** horizontal movement input is active while the player is airborne
- **THEN** horizontal velocity moves toward the requested direction at the configured air-control rate without instantly acquiring full grounded acceleration

### Requirement: Hold-to-crouch stance
While crouch is active, the controller SHALL use a shorter capsule and a lower eye height. When crouch becomes inactive, the controller SHALL return to standing only if the standing capsule fits at the current position; otherwise it SHALL remain crouched until clearance becomes available.

#### Scenario: Player crouches
- **WHEN** crouch is active while the player can occupy the crouched shape
- **THEN** the collision capsule and camera eye height transition to the configured crouched stance without moving the player's grounded foot position through the floor

#### Scenario: Standing space is obstructed
- **WHEN** crouch is released beneath a structure that intersects the standing capsule but not the crouched capsule
- **THEN** the player remains crouched and does not penetrate the structure

#### Scenario: Standing space becomes clear
- **WHEN** crouch is inactive and the standing capsule fits after the player leaves the obstruction
- **THEN** the controller restores the standing capsule and standing eye height

### Requirement: First-person look and camera frame
The runtime SHALL maintain yaw and pitch from captured mouse look, SHALL clamp pitch before overturning, and SHALL produce the existing backend-neutral perspective camera frame from the current framebuffer aspect and the player's interpolated position plus stance eye height. Look orientation SHALL update once from each sampled event batch and SHALL NOT be multiplied by simulation-step count.

#### Scenario: Mouse look updates orientation
- **WHEN** captured cursor movement is sampled during an event batch
- **THEN** yaw and pitch change once according to the configured sensitivity and pitch remains within its configured limits

#### Scenario: Render occurs between simulation states
- **WHEN** a renderable iteration retains a fractional fixed-step remainder
- **THEN** the camera position is interpolated between the previous and current simulated player positions while look orientation reflects the latest sampled input

#### Scenario: Player changes stance
- **WHEN** the player enters or leaves crouch
- **THEN** the rendered camera uses the corresponding eye height without moving outside the current player capsule

### Requirement: Prototype cursor capture
The prototype runtime SHALL begin with the cursor captured, SHALL release it while the menu action is active, SHALL recapture it from the primary action, and SHALL reset relative-look tracking across each capture transition. While the cursor is released, player movement, jump, crouch, sprint, and look input SHALL be neutral while gravity and active physics simulation continue.

#### Scenario: Cursor is released
- **WHEN** the menu action is active while the cursor is captured
- **THEN** the cursor becomes available for normal desktop interaction and player control input becomes neutral

#### Scenario: Cursor is recaptured
- **WHEN** the primary action is active while the cursor is released
- **THEN** the cursor is captured again without applying cursor movement accumulated before or during the transition as player look

#### Scenario: Player is unsupported while released
- **WHEN** the cursor is released while the player is airborne
- **THEN** gravity and collision continue to update the player without applying movement or stance actions
