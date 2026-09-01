## MODIFIED Requirements

### Requirement: Bounded local diffuse scene shading
The opaque prototype scene SHALL combine its near-black ambient contribution with diffuse illumination from both authored point lights and at most one optional dynamic spot light. Every local-light contribution SHALL depend on surface orientation and SHALL fall smoothly to zero at its finite influence boundary; the spot light SHALL additionally fall smoothly to zero across its configured cone transition. Accumulated lighting SHALL remain bounded, surfaces outside every active local-light influence SHALL receive only the near-black ambient contribution, and the scene SHALL preserve its existing textured, depth-tested opaque draw.

#### Scenario: Surface is near an authored light
- **WHEN** a fragment lies inside a point light's influence radius and faces toward that light
- **THEN** its displayed illumination includes a distance-attenuated contribution using that light's authored color and intensity

#### Scenario: Surface is inside the active spot light
- **WHEN** a fragment lies inside the enabled spot light's range and cone and faces toward that light
- **THEN** its displayed illumination includes distance- and cone-attenuated contributions using the supplied spot-light color and intensity

#### Scenario: Surface faces away from an authored light
- **WHEN** a fragment lies inside a point or spot light's bounded influence but its outward normal faces away from that light
- **THEN** that light contributes no diffuse illumination to the fragment

#### Scenario: Surface is outside every local light
- **WHEN** a fragment lies outside both authored point-light radii and outside the active spot-light range or cone
- **THEN** it receives only the near-black ambient contribution and remains intentionally dark

#### Scenario: Dynamic spot light is disabled
- **WHEN** a frame contains no enabled dynamic spot light
- **THEN** the scene retains the existing two authored point lights and near-black ambient lighting without a spot contribution

#### Scenario: Prototype atmosphere is inspected
- **WHEN** the built-in scene is viewed from its initial player route
- **THEN** it retains a dim spawn light, a distinct destination light, and an intervening dark region that the optional spot light can illuminate locally
