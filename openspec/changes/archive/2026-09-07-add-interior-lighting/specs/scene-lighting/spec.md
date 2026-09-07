## MODIFIED Requirements

### Requirement: Immutable prototype environment light
The level SHALL define zero through eight immutable authored point lights and a finite ambient contribution from 0 through 0.20 inclusive, as defined by interior-lighting. Each light SHALL have a unique durable ID, finite position, non-negative finite RGB color, positive finite intensity and radius, and boolean initial enable and shadow flags. Invalid authored lighting SHALL fail shared validation before renderer startup. The renderer SHALL retain the valid authored description for the scene lifetime and apply separately supplied per-light enabled state without modifying that description. Ambient SHALL be authored independently; 0.12 SHALL remain a starter default rather than a forced floor.

#### Scenario: Prototype lighting initializes
- **WHEN** the immutable prototype level is constructed
- **THEN** it exposes the selected level's valid authored light set and ambient value, and initializes each effective enable from that light's initial state

#### Scenario: Authored lighting is invalid
- **WHEN** a point light has a non-finite value, negative color component, non-positive intensity, or non-positive influence radius, or ambient is non-finite, negative, or greater than 0.20
- **THEN** prototype-level validation rejects the environment-light description before renderer startup

#### Scenario: Camera orientation changes
- **WHEN** the player looks or moves while the level remains unchanged
- **THEN** all authored lights remain fixed in world space rather than following the camera

#### Scenario: No local lights are authored
- **WHEN** a valid level contains an empty light set and ambient zero
- **THEN** environment illumination is zero without an invented light or ambient floor

### Requirement: Bounded local diffuse scene shading
The opaque prototype scene SHALL combine its authored ambient contribution with diffuse illumination from each enabled authored point light and at most one optional dynamic spot light. A disabled point light SHALL contribute zero illumination. A shadow-casting point light SHALL additionally obey the supported occlusion behavior in interior-lighting; an unshadowed point light, ambient and the optional dynamic spotlight SHALL remain unoccluded. Every enabled local-light contribution SHALL depend on surface orientation and SHALL fall smoothly to zero at its finite influence boundary; the spot light SHALL additionally fall smoothly to zero across its configured cone transition. Accumulated lighting SHALL remain bounded, surfaces outside every active local-light influence SHALL receive the authored ambient contribution, and both generated and imported geometry SHALL preserve the same textured, depth-tested opaque shading path.

#### Scenario: Surface is near an authored light
- **WHEN** a generated-world or imported-prop fragment lies inside an enabled point light's influence radius and faces toward that light without supported occlusion
- **THEN** its displayed illumination includes a distance-attenuated contribution using that light's authored color and intensity

#### Scenario: Surface is inside the active spot light
- **WHEN** a generated-world or imported-prop fragment lies inside the enabled spot light's range and cone and faces toward that light without supported occlusion
- **THEN** its displayed illumination includes distance- and cone-attenuated contributions using the supplied spot-light color and intensity

#### Scenario: Surface faces away from an authored light
- **WHEN** a fragment lies inside a point or spot light's bounded influence but its outward normal faces away from that light
- **THEN** that light contributes no diffuse illumination to the fragment

#### Scenario: Surface is outside every local light
- **WHEN** a fragment lies outside every enabled authored point-light radius and outside the active spot-light range or cone
- **THEN** it receives the selected level's authored ambient contribution

#### Scenario: Dynamic spot light is disabled
- **WHEN** a frame contains no enabled dynamic spot light
- **THEN** the generated world and imported prop retain each enabled authored point-light contribution and readable ambient lighting without a spot contribution

#### Scenario: Prototype atmosphere is inspected
- **WHEN** the built-in scene is viewed from its initial player route
- **THEN** terrain, static structures, and the imported prop remain visibly readable between the two authored point lights

#### Scenario: A point light is disabled
- **WHEN** a fragment would otherwise receive a contribution from a point light whose per-frame state is disabled
- **THEN** that light contributes zero while ambient, other enabled point lights, and an active spotlight continue to be evaluated normally

### Requirement: Packaged lighting shaders
The lit scene SHALL execute from the explicit runtime resource root using the selected level, scene shaders and referenced model/material resources. It SHALL retain the bounded authored point-light set, its independent per-light enabled state and supported shadows, ambient and optional spotlight behavior for all generated geometry, doors and surviving OPAQUE/MASK prop fragments. It SHALL NOT require a general material framework, a separate lighting data file, or the raw source pack. Base-color materials SHALL NOT implicitly add emission, roughness/specular lighting or extra lights.

#### Scenario: Executable-relative resources are complete
- **WHEN** the launcher supplies a valid executable-relative resource root containing the selected level, lit scene shaders, and all referenced model/material resources
- **THEN** renderer startup can construct the lit textured scene pipeline and its selected generated, prop and door draws without consulting the process working directory or unrelated graphics assets
