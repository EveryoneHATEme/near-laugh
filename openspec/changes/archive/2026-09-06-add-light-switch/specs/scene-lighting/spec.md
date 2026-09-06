## MODIFIED Requirements

### Requirement: Immutable prototype environment light
The built-in prototype level SHALL define exactly two immutable world-space point lights and one non-negative readable ambient contribution fixed at 0.12. Each point light SHALL have a finite position, a finite non-negative RGB color, a positive finite intensity, and a positive finite influence radius. The ambient contribution SHALL not exceed 0.20. Invalid authored lighting SHALL fail level validation before renderer startup, and the renderer SHALL retain the same valid authored environment-light description for every scene frame. Each point light SHALL additionally have independent per-frame enabled state, defaulting to enabled; disabling it SHALL suppress its contribution without changing the authored description.

#### Scenario: Prototype lighting initializes
- **WHEN** the immutable prototype level is constructed
- **THEN** it exposes the packaged level's two valid authored point lights and 0.12 ambient contribution, and initializes effective point-light enable state from the optional authored switch

#### Scenario: Authored lighting is invalid
- **WHEN** a point light has a non-finite value, negative color component, non-positive intensity, or non-positive influence radius, or ambient is non-finite, negative, or greater than 0.20
- **THEN** prototype-level validation rejects the environment-light description before renderer startup

#### Scenario: Camera orientation changes
- **WHEN** the player looks or moves while the level remains unchanged
- **THEN** both lights remain fixed in world space rather than following the camera

### Requirement: Bounded local diffuse scene shading
The opaque prototype scene SHALL combine its readable ambient contribution with diffuse illumination from each enabled authored point light and at most one optional dynamic spot light. A disabled point light SHALL contribute zero illumination. Every enabled local-light contribution SHALL depend on surface orientation and SHALL fall smoothly to zero at its finite influence boundary; the spot light SHALL additionally fall smoothly to zero across its configured cone transition. Accumulated lighting SHALL remain bounded, surfaces outside every active local-light influence SHALL receive the readable ambient contribution, and both generated and imported geometry SHALL preserve the same textured, depth-tested opaque shading path.

#### Scenario: Surface is near an authored light
- **WHEN** a generated-world or imported-prop fragment lies inside an enabled point light's influence radius and faces toward that light
- **THEN** its displayed illumination includes a distance-attenuated contribution using that light's authored color and intensity

#### Scenario: Surface is inside the active spot light
- **WHEN** a generated-world or imported-prop fragment lies inside the enabled spot light's range and cone and faces toward that light
- **THEN** its displayed illumination includes distance- and cone-attenuated contributions using the supplied spot-light color and intensity

#### Scenario: Surface faces away from an authored light
- **WHEN** a fragment lies inside a point or spot light's bounded influence but its outward normal faces away from that light
- **THEN** that light contributes no diffuse illumination to the fragment

#### Scenario: Surface is outside every local light
- **WHEN** a fragment lies outside every enabled authored point-light radius and outside the active spot-light range or cone
- **THEN** it receives the 0.12 readable ambient contribution

#### Scenario: Dynamic spot light is disabled
- **WHEN** a frame contains no enabled dynamic spot light
- **THEN** the generated world and imported prop retain each enabled authored point-light contribution and readable ambient lighting without a spot contribution

#### Scenario: Prototype atmosphere is inspected
- **WHEN** the built-in scene is viewed from its initial player route
- **THEN** terrain, static structures, and the imported prop remain visibly readable between the two authored point lights

#### Scenario: A point light is disabled
- **WHEN** a fragment would otherwise receive a contribution from a point light whose per-frame state is disabled
- **THEN** that light contributes zero while ambient, the other enabled point light, and an active spotlight continue to be evaluated normally

### Requirement: Packaged lighting shaders
The lit prototype scene SHALL remain executable from the existing explicit runtime resource root and SHALL require only the packaged level, scene shaders, fixed prototype surface textures, and required static GLB. It SHALL NOT require a general material or a separate external lighting data file beyond the packaged level.

#### Scenario: Executable-relative resources are complete
- **WHEN** the launcher supplies a valid executable-relative resource root containing the packaged level, lit scene shaders, fixed surface textures, and static GLB
- **THEN** renderer startup can construct the lit textured scene pipeline and both opaque mesh draws without consulting the process working directory or additional graphics assets
