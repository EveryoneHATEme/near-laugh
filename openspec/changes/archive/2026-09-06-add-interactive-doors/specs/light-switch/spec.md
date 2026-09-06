## MODIFIED Requirements

### Requirement: Bounded authored light switch
A level SHALL contain zero or one light switch. When present, the switch SHALL define a finite world-space position and yaw, one linked point-light slot from the existing two slots, and an initial on/off state. Its visible plate and interaction bounds SHALL use the same placement and fixed dimensions. The switch SHALL be non-blocking decoration and SHALL NOT add character collision, animation, or arbitrary interaction actions.

#### Scenario: Switch is present
- **WHEN** a valid level containing a switch is loaded
- **THEN** the game renders a distinguishable opaque switch at its authored position and orientation and targets it at that same placement

#### Scenario: Level has no switch
- **WHEN** a valid level has no switch
- **THEN** both authored point lights start enabled and interaction cannot change point lights and any door action follows authored-interaction

### Requirement: Nearby unobstructed view targeting
Interaction SHALL target the switch only when the forward ray from the player eye used for the current displayed camera intersects its plate bounds within 2 metres, inclusive. The player eye SHALL be outside those bounds. Collision between the eye and the plate, including terrain, solids, the authored prop proxy, and current door leaves, SHALL prevent activation; collision at the target surface SHALL count as obstruction subject only to numerical tolerance. The player's own collision representation SHALL NOT obstruct the query. A rejected interaction SHALL leave light state unchanged. The switch SHALL participate in authored-interaction nearest-target arbitration, so an interaction consumed or refused by a nearer door SHALL NOT toggle it.

#### Scenario: Switch is within reach
- **WHEN** the player looks at an unobstructed switch with a ray-to-plate distance of at most 2 metres and presses interaction
- **THEN** the linked light toggles once

#### Scenario: Switch is outside reach or missed
- **WHEN** the player presses interaction while the view ray misses the switch or reaches its plate beyond 2 metres
- **THEN** neither point light changes state

#### Scenario: Static geometry hides the switch
- **WHEN** a wall, terrain surface, or static prop collision proxy blocks the view segment to an otherwise in-range switch
- **THEN** pressing interaction does not activate the switch through that collision

#### Scenario: Switch is mounted outside a wall
- **WHEN** the plate is just in front of its supporting wall and its front is visible within reach
- **THEN** the wall behind the plate does not prevent interaction

#### Scenario: Eye is inside blocking geometry
- **WHEN** a target query begins inside blocking static or door collision or inside the switch plate
- **THEN** the switch cannot be activated by that query

### Requirement: Edge-triggered active interaction
One eligible sampled interaction press selected for the switch by authored-interaction SHALL toggle at most once, independently of the number of fixed simulation steps or rendered frames. Interaction SHALL require a sampled release before arming at startup and between presses. Presses sampled while the cursor is released, during a capture transition, while minimized, or while closing SHALL be consumed without activation. A missed, out-of-range, obstructed, or inactive press SHALL NOT remain pending for later targeting or restoration.

#### Scenario: Interaction remains held
- **WHEN** the player keeps interaction held after a valid activation
- **THEN** the light retains its new state until a release and another eligible press

#### Scenario: Held input later acquires a target
- **WHEN** the player presses interaction while looking away and then aims at the switch without releasing interaction
- **THEN** the switch does not activate until interaction is released and pressed again

#### Scenario: Cursor is recaptured with interaction held
- **WHEN** interaction is held while the cursor is released and the player recaptures the cursor
- **THEN** neither the recapture batch nor subsequent held batches activate the switch

#### Scenario: Interaction is pressed while minimized
- **WHEN** an interaction press is sampled while the framebuffer is zero-sized
- **THEN** it does not toggle the light while minimized or become a delayed activation on restoration

#### Scenario: Simulation batch size varies
- **WHEN** an eligible interaction press occurs in a renderable iteration with zero, one, or several fixed simulation steps
- **THEN** the runtime evaluates that press once using the iteration's player view


#### Scenario: Door blocks the plate
- **WHEN** a current visible leaf blocks an otherwise in-range switch
- **THEN** the switch does not activate until the leaf clears the view and a new eligible press targets the plate

#### Scenario: Lock or knock targets the switch
- **WHEN** lock or knock action selects the switch as the nearest target
- **THEN** neither its light nor an object behind it changes and the action is consumed
