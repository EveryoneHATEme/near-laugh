# authored-interaction Specification

## Purpose

Defines deterministic target selection and sampled input-edge handling shared by this game's concrete switch and door interactions.

## Requirements

### Requirement: One nearest authored interaction target
An action SHALL choose at most one target by the normalized forward ray from the eye used for the current displayed camera, with a maximum intersection distance of 2 metres inclusive. During empty-handed exploration, candidates SHALL be all authored switch plates, current visible door leaves, free household boxes at their accepted full physical poses, readable document panels and radio prop bounds at the same poses used for presentation. The eye SHALL be outside target and blocker bounds. The nearest candidate SHALL govern even when it refuses or does not support the requested action; the action SHALL NOT fall through to another object. Candidates within 0.1 mm of the true nearest intersection SHALL be ordered by type, door before switch before physical box before document before radio, and then lexicographic durable ID within the type, independently of container iteration or frame rate. Terrain, structural solids, prop proxies, current scripted actor proxies, current physical boxes, and other doors SHALL block the segment; the selected door or physical box SHALL permit targeting its own front surface, but no blocker behind that surface SHALL matter and no unrelated blocker at or before it SHALL be ignored. The player SHALL NOT obstruct its own query.

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

#### Scenario: Multiple switch candidates tie
- **WHEN** switch plates have distances within the tie interval and their definitions are reordered
- **THEN** the same switch ID wins, while a door in that interval retains type priority and at most one action dispatches

#### Scenario: Character stands in front of a switch
- **WHEN** an accepted actor proxy intersects the segment before an otherwise reachable switch or door
- **THEN** the target is obstructed and the actor does not become an invented interaction target

#### Scenario: Rotated box is the nearest target
- **WHEN** a free physical box has tumbled in front of an otherwise reachable switch
- **THEN** the actual oriented box is selected at its front surface, its own body does not reject that surface, and no action reaches the switch through it

#### Scenario: Unrelated blocker meets the selected box
- **WHEN** unrelated geometry intersects the segment at or before the selected box's front surface
- **THEN** pickup is refused without ignoring that blocker along with the box's own body

### Requirement: Shared sampled action edges
Interaction, lock, and knock actions SHALL each require an observed release before the first eligible press and between presses. Each event batch SHALL be evaluated exactly once after its fixed steps, including a batch containing zero steps. Holding, missing, obstruction, inactive controls, cursor release or capture transitions, minimization, and closing SHALL consume presses without retaining a future activation. In empty-handed exploration, when multiple supported action edges occur in one batch, all SHALL be consumed and at most one SHALL dispatch with lock before interaction before knock priority; this priority SHALL apply before target eligibility, without fallback. While carrying, E SHALL request drop and secondary action SHALL request throw; simultaneous edges SHALL be consumed with drop before throw, and lock and other targeted actions SHALL be consumed without dispatch. Reading SHALL consume conflicting world actions as defined by player-input. Flashlight SHALL retain its existing independent behavior in exploration and carrying, and SHALL be suppressed during reading. Physical commands SHALL follow the once-only fixed-boundary handoff in runtime-composition; other interactions SHALL retain their sampled-batch timing.

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
- **WHEN** new lock and interaction presses occur in the same event batch during empty-handed exploration
- **THEN** only lock is considered, interaction is consumed, and another interaction requires release and a new press

#### Scenario: Throw and drop arrive together
- **WHEN** a held box receives fresh drop and throw edges in the same active event batch
- **THEN** only drop is requested, both edges are consumed and no throw occurs after another box is acquired

#### Scenario: Pickup loses eligibility before the fixed boundary
- **WHEN** a queued box pickup is no longer reachable or visible when physics is ready to apply it
- **THEN** acquisition is refused once without changing the free box or choosing a replacement target
