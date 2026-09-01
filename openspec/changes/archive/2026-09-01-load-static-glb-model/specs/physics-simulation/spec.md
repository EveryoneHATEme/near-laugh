## MODIFIED Requirements

### Requirement: Static prototype collision
The physics world SHALL create static collision for the finite terrain surface; every solid floor where present, boundary, obstacle, movement-test step, and low-clearance structure; and the one static model prop declared by the immutable prototype-level description. Corresponding visible and collision terrain surfaces SHALL share the same height samples, placement, and dimensions; corresponding generated solid structures SHALL share the same structural dimensions; and the imported prop SHALL use its separately authored finite box proxy at the declared model placement rather than deriving collision from loaded render triangles.

#### Scenario: Static collision world starts
- **WHEN** the prototype level is installed into a newly initialized physics world
- **THEN** its declared terrain, generated solid structures, and static model-prop proxy are available for character collision before the first simulation step

#### Scenario: Visible structure is tested for collision
- **WHEN** the player reaches visible terrain, a floor, boundary, obstacle, step, or low-clearance structure
- **THEN** collision is evaluated against a surface with matching placement and dimensions

#### Scenario: Imported prop is tested for collision
- **WHEN** the player reaches the visible static model prop
- **THEN** collision is evaluated against the prop's declared simple box proxy at the same world placement without loading model geometry into the physics module
