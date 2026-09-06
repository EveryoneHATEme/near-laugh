## MODIFIED Requirements

### Requirement: Static prototype collision
The physics world SHALL create static collision for terrain only when the selected immutable level declares it, every declared solid floor, boundary, obstacle, step, or low-clearance structure, and the one declared static model prop. It SHALL NOT synthesize terrain or an invisible floor for an interior. Corresponding visible and collision terrain surfaces SHALL share the same height samples, placement, and dimensions; corresponding generated solid structures SHALL share the same structural dimensions; and the imported prop SHALL use its separately authored finite box proxy at the declared model placement rather than deriving collision from loaded render triangles. Character initialization SHALL use the resolved entry pose after level and entry validation. Partial collision construction SHALL remain safe both with and without a terrain body.

#### Scenario: Static collision world starts
- **WHEN** the selected level is installed into a newly initialized physics world
- **THEN** its declared terrain when present, generated solid structures, and static model-prop proxy are available for character collision before the first simulation step

#### Scenario: Visible structure is tested for collision
- **WHEN** the player reaches visible terrain, a floor, boundary, obstacle, step, or low-clearance structure
- **THEN** collision is evaluated against a surface with matching placement and dimensions

#### Scenario: Imported prop is tested for collision
- **WHEN** the player reaches the visible static model prop
- **THEN** collision is evaluated against the prop's declared simple box proxy at the same world placement without loading model geometry into the physics module

#### Scenario: Interior collision starts without terrain
- **WHEN** a validated terrain-free interior starts at its lower-landing or apartment entry
- **THEN** the character begins at the selected authored feet position with structural collision already installed and no terrain body or hidden floor

#### Scenario: Terrain-free collision startup fails
- **WHEN** collision construction fails after some interior solids or the prop proxy have been created
- **THEN** all successfully created bodies and physics owners are released exactly once without relying on a terrain body having existed
