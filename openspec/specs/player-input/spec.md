# player-input Specification

## Purpose

Defines the concrete platform-independent action state needed by the one local first-person player without introducing a general input framework.

## Requirements

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

### Requirement: First-person look delta
The input component SHALL report mouse look movement accumulated during the current processed event batch and SHALL reset that delta before the next batch without clearing held action state.

#### Scenario: Cursor moves within one event batch
- **WHEN** multiple cursor positions are reported during an event batch
- **THEN** the player input snapshot contains their accumulated relative movement

#### Scenario: A new event batch begins
- **WHEN** the runtime begins processing the next event batch
- **THEN** look delta starts at zero while held keyboard and mouse actions remain active

### Requirement: Platform-independent player input contract
Player-input consumers SHALL NOT include or exchange GLFW, native-key, or native-mouse constants; platform physical state SHALL be translated before the action snapshot reaches runtime consumers.

#### Scenario: Input consumer boundary is inspected
- **WHEN** gameplay-facing input headers and tests are inspected
- **THEN** they use only project-owned physical-input and player-action types

### Requirement: Concrete household input ownership
Empty-handed exploration SHALL retain current controls, with E additionally opening the selected document, acquiring a selected free box or toggling a selected radio. While carrying, E SHALL drop the held box and right mouse SHALL throw it; R and targeted door/switch/document/radio actions SHALL be consumed without dispatch. Left mouse SHALL retain independent flashlight behavior while exploration remains active. The carrying hint SHALL identify drop and throw controls. At most one physical command SHALL be pending for a fixed boundary; another conflicting edge SHALL be consumed with feedback rather than replacing or queuing a chain of commands.

#### Scenario: A door is targeted while carrying
- **WHEN** the player looks at a door and presses E while holding a box
- **THEN** the box is dropped, that press does not open the door, and a later released-and-repressed E can operate it

#### Scenario: Right mouse is pressed without a held box
- **WHEN** empty-handed exploration receives a secondary action press
- **THEN** the existing nearest-target knock policy applies and no box is thrown remotely

### Requirement: Reading mode and safe transitions
Reading SHALL own E/Escape for close and A/D for previous/next page, with visible controls and release-before-press navigation. Opening SHALL begin at page one, and close SHALL restore exploration without applying the same input to cursor release, movement, stance, camera, flashlight, lock, knock or interaction. While reading, player movement, jump, sprint, stance changes, look, flashlight and world interactions SHALL be suppressed; the current stance SHALL be retained subject to physical safety. Reading SHALL NOT itself pause physics, actors, doors, audio or caption time. Actual suspension/minimization SHALL retain the document/page and freeze the world through the existing suspension policy. Controls held across reading/carrying/capture transitions SHALL require an observed release before they can activate the new mode, including held movement controls on return to exploration. Opening or closing a document SHALL discard accumulated look deltas rather than replay them.

#### Scenario: E opens and remains held
- **WHEN** an eligible E press opens a readable document and stays down
- **THEN** the document remains open until a new eligible close press, and no action activates behind it

#### Scenario: Escape closes the document
- **WHEN** a fresh Escape press closes reading while the cursor was captured
- **THEN** exploration returns with captured cursor and neutral transition input, and that Escape does not also release the cursor

#### Scenario: Movement is held through reading
- **WHEN** W or another movement/stance control remains held while the document opens and closes
- **THEN** returning to exploration does not move or change stance until the relevant control has been released and pressed again

#### Scenario: Physics continues behind a document
- **WHEN** the player reads while a box is falling, an actor is walking or a radio is playing
- **THEN** the world and captions continue according to their active-time policies while player actions remain suppressed

#### Scenario: Reading is minimized and restored
- **WHEN** the window minimizes while page two is open and navigation keys are pressed during the wait
- **THEN** restoration retains page two and frozen world state without applying the waited navigation or world actions
