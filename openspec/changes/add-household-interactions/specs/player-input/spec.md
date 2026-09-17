## ADDED Requirements

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
