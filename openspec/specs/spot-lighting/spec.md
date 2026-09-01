# spot-lighting Specification

## Purpose

Defines a source-independent dynamic spot light and its bounded cone-shaped contribution to the opaque prototype scene.

## Requirements

### Requirement: Source-independent spot-light frame
Each renderable frame SHALL accept at most one optional dynamic spot light described by backend-neutral world-space position, unit direction, non-negative RGB color, positive range and intensity, and valid inner and outer cone limits. The spot-light description SHALL NOT identify or depend on a player, flashlight, weapon, or other producing gameplay object, and the renderer SHALL use the supplied pose rather than deriving a pose from the camera.

#### Scenario: Player flashlight supplies the spot light
- **WHEN** flashlight gameplay supplies a valid enabled spot-light description
- **THEN** the next renderable frame uses that description without exposing flashlight or player state to rendering

#### Scenario: Another source supplies the spot light
- **WHEN** a future runtime object supplies the same valid spot-light description with a different world-space pose
- **THEN** rendering uses the supplied pose without requiring a flashlight-specific renderer path

#### Scenario: No spot light is active
- **WHEN** a frame contains no enabled dynamic spot light
- **THEN** dynamic spot lighting contributes no illumination while the immutable environment lighting remains unchanged

### Requirement: Bounded spot-light diffuse shading
The opaque prototype scene SHALL apply a spot-light contribution only to fragments inside the light's finite range and outer cone whose outward surface normal faces the light. Distance attenuation SHALL reach zero at the configured range, angular attenuation SHALL transition smoothly between the inner and outer cone limits, accumulated lighting SHALL remain bounded, and the contribution SHALL require no visible emitter geometry, volumetric beam, or shadow map.

#### Scenario: Surface is inside the inner cone
- **WHEN** a fragment is inside the spot light's range and inner cone and faces toward the light
- **THEN** it receives the spot light's distance-attenuated diffuse contribution at full angular strength

#### Scenario: Surface is in the cone transition
- **WHEN** a fragment lies between the inner and outer cone limits and otherwise faces the light
- **THEN** its angular contribution falls smoothly toward zero at the outer limit

#### Scenario: Surface is outside the bounded influence
- **WHEN** a fragment is outside the spot light's range, outside its outer cone, or faces away from the light
- **THEN** that spot light contributes no diffuse illumination to the fragment
