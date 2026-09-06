## MODIFIED Requirements

### Requirement: Single grounded player
The runtime SHALL maintain exactly one local player represented by an upright gameplay-oriented capsule, SHALL initialize that player at the validated selected entry's foot position and yaw, and SHALL derive grounded state from physics contacts rather than camera position. Starting on an upper floor or in a terrain-free interior SHALL NOT change the player movement or stance policy.

#### Scenario: Player starts
- **WHEN** physics and static collision for the selected level have initialized
- **THEN** one standing player capsule starts at the resolved entry without intersecting static geometry and establishes grounded state from its supporting surface under gravity

#### Scenario: Alternate entry is selected
- **WHEN** the same level is started with a different valid named entry
- **THEN** the first player view uses that entry's position and yaw while retaining the same walking, step, gravity, and stance behavior
