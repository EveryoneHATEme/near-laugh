## MODIFIED Requirements

### Requirement: One nearest authored interaction target
An action SHALL choose at most one target by the normalized forward ray from the eye used for the current displayed camera, with a maximum intersection distance of 2 metres inclusive. Candidates SHALL be all authored switch plates and current visible door leaves at the same poses used for presentation. The eye SHALL be outside target and blocker bounds. The nearest candidate SHALL govern even when it refuses or does not support the requested action; the action SHALL NOT fall through to another object. Candidates within 0.1 mm of the true nearest intersection SHALL be ordered by type, door before switch, and then lexicographic durable ID within the type, independently of container iteration or frame rate. Terrain, structural solids, prop proxies, current scripted actor proxies, and other doors SHALL block the segment; the selected door's own front surface SHALL permit targeting it, but no blocker behind that surface SHALL matter and no unrelated blocker at or before it SHALL be ignored. The player SHALL NOT obstruct its own query.

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
