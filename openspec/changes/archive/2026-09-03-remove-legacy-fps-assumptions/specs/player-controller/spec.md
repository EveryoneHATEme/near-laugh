## MODIFIED Requirements

### Requirement: Ground movement
While first-person control is active, the player SHALL move horizontally relative to current yaw from the forward, backward, left, and right player actions, SHALL use a faster configured speed while sprint is active, and SHALL normalize combined movement axes so diagonal input does not exceed the selected speed. Ground movement SHALL collide with and slide along static geometry, traverse the prototype's configured walkable step, and SHALL NOT pass through blocking structures.

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
