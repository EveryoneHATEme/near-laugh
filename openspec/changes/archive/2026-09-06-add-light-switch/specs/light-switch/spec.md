## Purpose

Defines the first concrete authored world interaction: one nearby light switch that changes an environment light while preserving authored data and predictable player input.

## ADDED Requirements

### Requirement: Bounded authored light switch
A level SHALL contain zero or one light switch. When present, the switch SHALL define a finite world-space position and yaw, one linked point-light slot from the existing two slots, and an initial on/off state. Its visible plate and interaction bounds SHALL use the same placement and fixed dimensions. The switch SHALL be non-blocking decoration and SHALL NOT add character collision, animation, or arbitrary interaction actions.

#### Scenario: Switch is present
- **WHEN** a valid level containing a switch is loaded
- **THEN** the game renders a distinguishable opaque switch at its authored position and orientation and targets it at that same placement

#### Scenario: Level has no switch
- **WHEN** a valid level has no switch
- **THEN** both authored point lights start enabled and interaction presses have no effect

### Requirement: Nearby unobstructed view targeting
Interaction SHALL target the switch only when the forward ray from the player eye used for the current displayed camera intersects its plate bounds within 2 metres, inclusive. The player eye SHALL be outside those bounds. Existing static collision between the eye and the plate, including terrain, solids, and the authored prop proxy, SHALL prevent activation; collision at the target surface SHALL count as obstruction subject only to numerical tolerance. The player's own collision representation SHALL NOT obstruct the query. A rejected interaction SHALL leave light state unchanged.

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
- **WHEN** a target query begins inside blocking static collision or inside the switch plate
- **THEN** the switch cannot be activated by that query

### Requirement: Edge-triggered active interaction
One eligible sampled interaction press SHALL toggle at most once, independently of the number of fixed simulation steps or rendered frames. Interaction SHALL require a sampled release before arming at startup and between presses. Presses sampled while the cursor is released, during a capture transition, while minimized, or while closing SHALL be consumed without activation. A missed, out-of-range, obstructed, or inactive press SHALL NOT remain pending for later targeting or restoration.

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

### Requirement: Independent run-local light state
At application startup the linked point light SHALL use the switch's authored initial state and the other point light SHALL be enabled. Each accepted activation SHALL invert only the linked light's enabled state. Turning it off SHALL suppress its complete contribution while preserving authored intensity, color, radius, and position; ambient, the other point light, and flashlight state SHALL remain independent. Runtime interaction SHALL NOT modify or save the authored level.

#### Scenario: Initially off switch starts
- **WHEN** the application loads a switch authored with its initial state off
- **THEN** its linked point light contributes no illumination before any interaction and the other point light remains enabled

#### Scenario: Linked light is toggled off and on
- **WHEN** the player performs two eligible presses separated by a release
- **THEN** the linked light returns to its initial state with its original authored parameters, without changing the other light or flashlight

#### Scenario: Presentation is interrupted
- **WHEN** rendering skips a frame, recreates the swapchain, or resumes after minimization following a toggle
- **THEN** the next presented scene uses the current run-local light state without resetting or replaying the interaction

#### Scenario: Application is restarted
- **WHEN** the player restarts after changing the light state
- **THEN** the switch again uses the level's authored initial state and the level file retains its pre-interaction contents

### Requirement: Packaged playable example
The packaged prototype SHALL include one distinguishable switch on the player approach route controlling one existing point light that starts on. The switch SHALL be reachable using existing movement and SHALL allow a visible off/on lighting change without requiring new external art assets or a HUD.

#### Scenario: Example is inspected in play
- **WHEN** the player approaches the packaged switch and alternates eligible E presses
- **THEN** its associated light pool visibly disappears and returns while the plate remains at its authored placement
