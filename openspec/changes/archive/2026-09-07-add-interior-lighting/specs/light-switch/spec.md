## MODIFIED Requirements

### Requirement: Bounded authored light switch
A level SHALL contain zero through sixteen light switches. Each switch SHALL define a unique durable ID, finite world-space position and yaw, and exactly one reference to an authored point light by its durable ID. Switch IDs SHALL follow the 1-64 character lowercase ASCII identifier grammar used for entries and SHALL be unique within their collection. Several switches SHALL be allowed to reference the same light. Initial enabled state SHALL belong only to the light. Visible plates and interaction bounds SHALL share the existing fixed dimensions and placement. Switches SHALL remain non-blocking decoration without character collision, animation or arbitrary interaction actions. Invalid identities and unresolved light references SHALL prevent saving and runtime handoff while safe definitions remain editable.

#### Scenario: Switch is present
- **WHEN** a valid level containing a switch is loaded
- **THEN** the game renders a distinguishable opaque switch at its authored position and orientation and targets it at that same placement

#### Scenario: Level has no switch
- **WHEN** a valid level has no switch
- **THEN** each authored light starts in its own initial state and switch interaction cannot change point lights and any door action follows authored-interaction

#### Scenario: Switch references a missing light
- **WHEN** a switch names a light absent from the light collection
- **THEN** validation identifies the switch and unresolved light ID, prevents saving/play, and allows repair without retargeting

### Requirement: Nearby unobstructed view targeting
Interaction SHALL target the switch only when the forward ray from the player eye used for the current displayed camera intersects its plate bounds within 2 metres, inclusive. The player eye SHALL be outside those bounds. Collision between the eye and the plate, including terrain, solids, the authored prop proxy, and current door leaves, SHALL prevent activation; collision at the target surface SHALL count as obstruction subject only to numerical tolerance. The player's own collision representation SHALL NOT obstruct the query. A rejected interaction SHALL leave light state unchanged. The switch SHALL participate in authored-interaction nearest-target arbitration, so an interaction consumed or refused by a nearer door SHALL NOT toggle it.

#### Scenario: Switch is within reach
- **WHEN** the player looks at an unobstructed switch with a ray-to-plate distance of at most 2 metres and presses interaction
- **THEN** the linked light toggles once

#### Scenario: Switch is outside reach or missed
- **WHEN** the player presses interaction while the view ray misses the switch or reaches its plate beyond 2 metres
- **THEN** no point light changes state

#### Scenario: Static geometry hides the switch
- **WHEN** a wall, terrain surface, or static prop collision proxy blocks the view segment to an otherwise in-range switch
- **THEN** pressing interaction does not activate the switch through that collision

#### Scenario: Switch is mounted outside a wall
- **WHEN** the plate is just in front of its supporting wall and its front is visible within reach
- **THEN** the wall behind the plate does not prevent interaction

#### Scenario: Eye is inside blocking geometry
- **WHEN** a target query begins inside blocking static or door collision or inside the switch plate
- **THEN** the switch cannot be activated by that query

### Requirement: Independent run-local light state
At application startup each light SHALL use its own authored initial enabled state, whether referenced by zero, one or several switches. Each accepted switch activation SHALL invert only its referenced light's shared run-local enabled value. Turning a light off SHALL suppress its complete contribution while preserving authored intensity, color, radius, position and shadow configuration; ambient, all other lights and flashlight state SHALL remain independent. Relinking or removing a switch in authored data SHALL NOT redefine a light's initial state. Runtime interaction SHALL NOT modify or save the authored level. Recovery SHALL preserve current enables, and a new run SHALL restore the authored values.

#### Scenario: Initially off switch starts
- **WHEN** the application loads an initially-off light referenced by a switch
- **THEN** its linked point light contributes no illumination before any interaction and all other lights use their own initial values

#### Scenario: Linked light is toggled off and on
- **WHEN** the player performs two eligible presses separated by a release
- **THEN** the linked light returns to its initial state with its original authored parameters, without changing any other light or flashlight

#### Scenario: Presentation is interrupted
- **WHEN** rendering skips a frame, recreates the swapchain, or resumes after minimization following a toggle
- **THEN** the next presented scene uses the current run-local light state without resetting or replaying the interaction

#### Scenario: Application is restarted
- **WHEN** the player restarts after changing the light state
- **THEN** each light again uses its own authored initial state and the level file retains its pre-interaction contents

#### Scenario: Two switches share a light
- **WHEN** the player toggles a light with one switch and then uses another switch referencing the same light
- **THEN** the second action inverts the same current value, with no stale switch-local state or change to another light
