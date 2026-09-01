## Purpose

Defines the single automatic hitscan rifle used to aim, fire, and produce deterministic recoil in the built-in FPS shooting-range prototype.

## ADDED Requirements

### Requirement: Automatic prototype rifle
The runtime SHALL maintain exactly one prototype hitscan rifle with unlimited ammunition, SHALL accept the existing primary action only while first-person controls are active, SHALL fire as soon as a newly observed trigger press can be processed by a fixed simulation step, and SHALL repeat fire while the trigger remains held no faster than its fixed configured interval. A trigger press observed during an iteration with no complete simulation step SHALL remain pending until one step processes it, and releasing then pressing the trigger SHALL NOT bypass an active fire interval.

#### Scenario: Trigger press reaches the next fixed step
- **WHEN** the captured primary action becomes active during an iteration that executes no complete simulation step
- **THEN** the rifle retains the press and fires once when the next complete step can accept a shot

#### Scenario: Trigger remains held
- **WHEN** first-person controls remain active and the primary action is held across multiple fixed steps
- **THEN** the rifle fires repeatedly at its configured interval without consuming ammunition

#### Scenario: Trigger is pressed during cooldown
- **WHEN** the primary action is released and pressed again before the current fire interval expires
- **THEN** the rifle does not fire another shot until the interval has expired

#### Scenario: Cursor is recaptured with the primary action
- **WHEN** the cursor is released and the primary action causes the existing capture transition
- **THEN** that action captures the cursor without queuing or firing a rifle shot

### Requirement: Authoritative hitscan shot
Every accepted shot SHALL originate at the current simulated player eye position, SHALL travel along the current first-person aim direction including previously accumulated weapon recoil, SHALL have one fixed maximum range, and SHALL resolve only the closest static collision surface along that segment. The recoil caused by the accepted shot SHALL affect subsequent aim and presentation rather than changing the shot already emitted.

#### Scenario: A static surface is in range
- **WHEN** an accepted shot intersects one or more static prototype solids within its maximum range
- **THEN** only the closest intersected solid is reported for hit processing

#### Scenario: No static surface is in range
- **WHEN** an accepted shot reaches its maximum range without intersecting static prototype collision
- **THEN** the shot produces no target damage or hit feedback

#### Scenario: A shot applies recoil
- **WHEN** the rifle accepts a shot from the current aim direction
- **THEN** that shot retains the pre-kick direction and the resulting recoil changes the aim used by later shots and frames

### Requirement: Bounded rifle recoil
Each accepted shot SHALL add a fixed upward pitch offset, accumulated recoil SHALL remain within a configured bound, and the offset SHALL recover toward zero during later fixed simulation steps without changing base mouse-look yaw or pitch. The effective first-person pitch SHALL continue to obey the player's existing pitch limits.

#### Scenario: Multiple shots accumulate recoil
- **WHEN** the rifle fires repeatedly faster than recoil fully recovers
- **THEN** upward pitch offset accumulates only to the configured maximum and never overturns the camera

#### Scenario: Firing stops
- **WHEN** fixed simulation continues without an accepted shot
- **THEN** rifle recoil moves toward zero at the configured recovery rate without changing yaw

