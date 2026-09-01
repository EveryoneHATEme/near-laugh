## MODIFIED Requirements

### Requirement: Static prototype collision
The physics world SHALL create static collision for the finite terrain surface and every solid floor where present, boundary, obstacle, movement-test step, and low-clearance structure declared by the immutable prototype-level description. Corresponding visible and collision terrain surfaces SHALL share the same height samples, placement, and dimensions; corresponding solid structures SHALL share the same structural dimensions.

#### Scenario: Static collision world starts
- **WHEN** the prototype level is installed into a newly initialized physics world
- **THEN** its declared terrain and solid structures are available for character collision before the first simulation step

#### Scenario: Visible structure is tested for collision
- **WHEN** the player reaches visible terrain, a floor, boundary, obstacle, step, or low-clearance structure
- **THEN** collision is evaluated against a surface with matching placement and dimensions
