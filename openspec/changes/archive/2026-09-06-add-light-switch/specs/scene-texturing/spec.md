## MODIFIED Requirements

### Requirement: Textured prototype surface appearance
Every prototype surface SHALL combine its sampled texture color with its authored tint, the authored ambient environment, each currently enabled authored point light, and any active dynamic spot-light contribution without target-specific highlight, dimming, or presentation state.

#### Scenario: Prototype surface is rendered
- **WHEN** a textured prototype solid is visible in a renderable frame
- **THEN** it displays its fixed texture modulated by its authored tint and the bounded point-plus-optional-spot lighting

#### Scenario: Point-light state changes
- **WHEN** one authored point light is disabled by a frame's lighting state
- **THEN** texture sampling, tint, depth visibility, ambient, and other active lights retain their behavior while that point light's contribution is absent
