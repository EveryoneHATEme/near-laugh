## Purpose

Defines deterministic target selection and sampled input-edge handling shared by this game's concrete switch and door interactions.

## ADDED Requirements

### Requirement: One nearest authored interaction target
An action SHALL choose at most one target by the normalized forward ray from the eye used for the current displayed camera, with a maximum intersection distance of 2 metres inclusive. Candidates SHALL be the switch plate and current visible door leaves at the same poses used for presentation. The eye SHALL be outside target and blocker bounds. The nearest candidate SHALL govern even when it refuses or does not support the requested action; the action SHALL NOT fall through to another object. Equal-distance candidates SHALL use a deterministic ordering independent of container iteration or frame rate. Terrain, structural solids, prop proxies, and other doors SHALL block the segment; the selected door's own front surface SHALL permit targeting it, but no blocker behind that surface SHALL matter and no unrelated blocker at or before it SHALL be ignored. The player SHALL NOT obstruct its own query.

#### Scenario: Door hides a reachable switch
- **WHEN** a closed or moving door intersects the eye ray before an otherwise reachable switch
- **THEN** interaction targets only that door and does not toggle the switch

#### Scenario: Door clears the view
- **WHEN** the current leaf pose leaves a clear ray to an in-range switch
- **THEN** interaction can toggle the switch at its unchanged plate bounds

#### Scenario: Nearest target cannot accept an action
- **WHEN** a lock or knock action meets the switch before a door, or a locked door refuses opening before another target
- **THEN** neither the farther object nor a second action is activated

#### Scenario: Coincident candidates are tested repeatedly
- **WHEN** two candidate distances are equal within the defined numeric tie tolerance
- **THEN** repeated queries select the same target independently of storage order

#### Scenario: Ray is invalid or out of reach
- **WHEN** the eye is inside a target or blocker, direction is invalid, the target is missed, or its first intersection is beyond 2 metres
- **THEN** no authored state changes

### Requirement: Shared sampled action edges
Interaction, lock, and knock actions SHALL each require an observed release before the first eligible press and between presses. Each event batch SHALL be evaluated exactly once after its fixed steps, including a batch containing zero steps. Holding, missing, obstruction, inactive controls, cursor release or capture transitions, minimization, and closing SHALL consume presses without retaining a future activation. When multiple supported action edges occur in one batch, all SHALL be consumed and at most one SHALL dispatch with lock before interaction before knock priority; this priority SHALL apply before target eligibility, without fallback. Flashlight input SHALL retain its existing independent behavior.

#### Scenario: Held miss later acquires a target
- **WHEN** an action is held after missing and the player subsequently looks at a valid target
- **THEN** activation requires a release and a new eligible press

#### Scenario: Several fixed steps occur
- **WHEN** one event batch contains an eligible press and zero, one, or several fixed steps
- **THEN** it produces at most one dispatch using the resulting displayed view

#### Scenario: Cursor is recaptured with held controls
- **WHEN** interaction, lock, or knock is held through cursor release, recapture, or a minimized event wait
- **THEN** it does not activate during that transition or in later held batches

#### Scenario: Action keys are pressed together
- **WHEN** new lock and interaction presses occur in the same event batch
- **THEN** only lock is considered, interaction is consumed, and another interaction requires release and a new press
