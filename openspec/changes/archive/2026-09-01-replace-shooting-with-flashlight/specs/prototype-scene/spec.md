## ADDED Requirements

### Requirement: Built-in inert plate geometry
The immutable prototype-level description SHALL include exactly three visually separated inert textured plates. Each plate's authored placement and dimensions SHALL derive matching opaque world-space geometry and static physics collision, each plate SHALL select the fixed shooting-target surface texture, and the plates SHALL require no gameplay target description, mutable health, model, general material, collision, or level asset beyond the existing executable-relative shader and fixed texture resources.

#### Scenario: Target plates are constructed
- **WHEN** the built-in prototype level is created
- **THEN** it contains three distinct inert plate solids assigned to the shooting-target surface texture without target gameplay descriptions

#### Scenario: Plate geometry reaches rendering and physics
- **WHEN** renderer and physics initialization consume the same prototype level
- **THEN** each plate has textured visible geometry and collidable geometry with matching placement and dimensions

#### Scenario: Prototype plates are packaged
- **WHEN** the executable runs from its copied executable-relative resource layout
- **THEN** all plate geometry and its fixed surface texture remain available without an additional target, model, material, or level asset

## REMOVED Requirements

### Requirement: Built-in shooting target geometry
**Reason**: The plates remain in the scene, but they no longer have gameplay target descriptions or form an active shooting range.

**Migration**: Use the new built-in inert plate geometry requirement for their visible, textured, and collidable scene role.
