## ADDED Requirements

### Requirement: Shadows follow supplied animated geometry
Supported animated OPAQUE geometry SHALL cast and receive the existing bounded point-light shadows using the same deformed triangles and world placement as its visible presentation, including when outside the camera view. Collision proxies SHALL NOT substitute for character silhouettes. Pause, clip transitions, disabled lights and presentation recovery SHALL preserve matching character color/shadow state. Existing static/door shadow, ambient, unshadowed light and flashlight behavior SHALL remain intact.

#### Scenario: Interaction moves an arm
- **WHEN** a supplied interaction pose moves the arm between a shadowed point light and a receiver
- **THEN** the receiver shadow follows that arm's deformed geometry in the same submitted frame

#### Scenario: Animated occluder leaves camera view
- **WHEN** a moving mannequin leaves the view while remaining between a key light and a visible receiver
- **THEN** its current shadow remains without replacing its silhouette by its collision capsule

#### Scenario: Character pose freezes
- **WHEN** the caller suspends animation or repeats a pose through recovery
- **THEN** visible geometry and shadow remain at that pose without resetting or anticipating resumed motion
