## ADDED Requirements

### Requirement: Built-in shooting target geometry
The immutable prototype-level description SHALL include exactly three visually separated target plates with stable target descriptions that identify distinct solids. Each plate's authored placement and dimensions SHALL derive matching opaque world-space geometry and static physics collision, and the plates SHALL require no model, texture, material, collision, or level asset beyond the existing executable-relative shader resources.

#### Scenario: Target plates are constructed
- **WHEN** the built-in prototype level is created
- **THEN** it contains three valid target descriptions that identify three distinct plate solids

#### Scenario: Target geometry reaches rendering and physics
- **WHEN** renderer and physics initialization consume the same prototype level
- **THEN** each target plate has visible and collidable geometry with matching placement and dimensions

#### Scenario: Shooting range is packaged
- **WHEN** the executable runs from its copied executable-relative resource layout
- **THEN** all target geometry remains available without an additional target or level asset

