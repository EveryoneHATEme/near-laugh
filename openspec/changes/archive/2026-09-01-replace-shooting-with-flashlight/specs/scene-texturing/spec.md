## ADDED Requirements

### Requirement: Textured spot-lit prototype appearance
Every prototype surface SHALL combine its sampled texture color with its authored tint, the immutable point-light and ambient environment, and any active dynamic spot-light contribution. Inert target plates SHALL use the same ordinary textured-lighting path as other prototype solids without per-target highlight or dimming state.

#### Scenario: Prototype surface is rendered
- **WHEN** a textured prototype solid is visible in a renderable frame
- **THEN** it displays its fixed texture modulated by its authored tint and the bounded point-plus-optional-spot lighting

#### Scenario: Inert plate is rendered
- **WHEN** one of the three textured plates is visible
- **THEN** it retains its authored appearance without requiring mutable target presentation masks

## REMOVED Requirements

### Requirement: Textured prototype appearance
**Reason**: The existing requirement includes target highlight/dim behavior and describes the superseded directional-plus-ambient lighting path.

**Migration**: Use the textured spot-lit prototype appearance requirement for ordinary inert surfaces under point, ambient, and optional dynamic spot lighting.
