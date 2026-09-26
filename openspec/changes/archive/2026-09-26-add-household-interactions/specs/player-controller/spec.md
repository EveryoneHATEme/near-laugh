## MODIFIED Requirements

### Requirement: Prototype cursor capture
Outside document reading, the prototype runtime SHALL begin with the cursor captured, SHALL release it while the menu action is active, SHALL recapture it from the primary action, and SHALL reset relative-look tracking across each capture transition. Document reading SHALL consume its close/cursor input under player-input rather than applying both policies to the same press. While the cursor is released, player movement, jump, crouch, sprint, and look input SHALL be neutral while gravity and active physics simulation continue.

#### Scenario: Cursor is released
- **WHEN** the menu action is active while the cursor is captured outside document reading
- **THEN** the cursor becomes available for normal desktop interaction and player control input becomes neutral

#### Scenario: Cursor is recaptured
- **WHEN** the primary action is active while the cursor is released
- **THEN** the cursor is captured again without applying cursor movement accumulated before or during the transition as player look

#### Scenario: Player is unsupported while released
- **WHEN** the cursor is released while the player is airborne
- **THEN** gravity and collision continue to update the player without applying movement or stance actions

## ADDED Requirements

### Requirement: Player contact with physical household boxes
Free and held household boxes SHALL block traversal and standing clearance, including contact with a stationary standing/crouched player. Box motion SHALL NOT propel the player or become valid player ground, stair or jump support. Controlled player motion SHALL be able to push free boxes with bounded physical response. Falling onto an isolated box SHALL preserve collision and slide toward clear space rather than allow sustained supported standing or penetration. Confinement SHALL NOT be resolved by teleporting the player or box; arbitrary box arrangements SHALL NOT promise automatic escape. The physics-visible player representation SHALL agree with accepted position and stance, and existing explicit player-envelope queries SHALL NOT count it twice or let it obscure the player's own targeting ray.

#### Scenario: Player attempts to step onto a box
- **WHEN** the player walks into or attempts to jump from a supported household box
- **THEN** it is not used as a stair/ground/jump surface, contact remains nonpenetrating and any box push stays bounded

#### Scenario: Player lands on a rotated box
- **WHEN** the player falls onto an isolated free or held box at an arbitrary accepted rotation
- **THEN** contact prevents penetration, does not establish valid box support and redirects toward available clear space

#### Scenario: A wall confines the landing
- **WHEN** no lateral escape from contact exists because nearby geometry confines the player
- **THEN** collision safety takes precedence over forced departure and neither participant is teleported or permitted to penetrate
