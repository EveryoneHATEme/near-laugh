## MODIFIED Requirements

### Requirement: Player action snapshot
The input component SHALL expose a per-iteration snapshot containing movement, jump, sprint, crouch, menu, interact, lock, primary-action, secondary-action, and first-person look-delta state for exactly one local player.

#### Scenario: Movement and stance actions are sampled
- **WHEN** the player holds the configured movement, jump, sprint, or crouch controls during an event batch
- **THEN** the corresponding player actions are active in the next input snapshot

#### Scenario: Mouse actions are sampled
- **WHEN** the player holds the primary or secondary mouse control
- **THEN** the corresponding primary-action or secondary-action state is active in the next input snapshot

#### Scenario: Interaction is sampled
- **WHEN** the player holds the configured interaction key during an event batch
- **THEN** the interact state is active in the next player snapshot without changing either mouse action

### Requirement: Default player controls
The runtime SHALL map W, A, S, and D to forward, left, backward, and right movement; Space to jump; Left Shift to sprint; Left Control to crouch; Escape to menu; E to interact; R to lock; and the left and right mouse buttons to primary and secondary actions.

#### Scenario: Default keyboard mapping
- **WHEN** physical W, Space, and Left Shift are down in the platform snapshot
- **THEN** forward movement, jump, and sprint are active in the player action snapshot

#### Scenario: Default mouse mapping
- **WHEN** the physical left mouse button is down
- **THEN** the primary player action is active

#### Scenario: Default interaction mapping
- **WHEN** physical E is down in the platform snapshot
- **THEN** the player interact action is active independently of the flashlight's primary action


#### Scenario: Default door controls
- **WHEN** physical E, R, or the right mouse button is sampled
- **THEN** it produces interaction, lock, or secondary action respectively; gameplay uses secondary action for a targeted knock while primary action remains independent
