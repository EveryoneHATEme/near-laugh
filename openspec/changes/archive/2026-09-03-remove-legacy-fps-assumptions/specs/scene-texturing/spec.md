## MODIFIED Requirements

### Requirement: Fixed prototype surface texture set
The built-in prototype level SHALL assign every solid exactly one texture role from a fixed set containing floor, boundary, and obstacle surfaces. Every role SHALL resolve to a distinct packaged opaque texture, and the assignment SHALL remain immutable for the run. The fixed set SHALL NOT contain a shooting-target role or require a shooting-target texture asset.

#### Scenario: Prototype level assigns surface roles
- **WHEN** the immutable prototype level is constructed
- **THEN** every floor, boundary, obstacle, and movement-test structure has one valid fixed texture role from the three-role set

#### Scenario: Movement-test geometry is assigned
- **WHEN** the walkable step or low-clearance structure is prepared for rendering
- **THEN** it uses the fixed obstacle surface texture role without requiring another material definition

#### Scenario: Removed surface role is supplied
- **WHEN** level data or a runtime resource request supplies the legacy shooting-target surface role or texture
- **THEN** it is rejected or absent rather than mapped to a compatibility texture layer

## ADDED Requirements

### Requirement: Textured prototype surface appearance
Every prototype surface SHALL combine its sampled texture color with its authored tint, the immutable point-light and ambient environment, and any active dynamic spot-light contribution without target-specific highlight, dimming, or presentation state.

#### Scenario: Prototype surface is rendered
- **WHEN** a textured prototype solid is visible in a renderable frame
- **THEN** it displays its fixed texture modulated by its authored tint and the bounded point-plus-optional-spot lighting

## REMOVED Requirements

### Requirement: Textured spot-lit prototype appearance
**Reason**: The existing requirement includes presentation behavior for three inert target plates that are being removed from the prototype.

**Migration**: Use `Textured prototype surface appearance` for the remaining ordinary surfaces; no target-plate presentation path is retained.
