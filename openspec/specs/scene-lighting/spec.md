# scene-lighting Specification

## Purpose

Defines the small, deterministic environment-lighting model that makes the built-in opaque FPS prototype scene spatially readable without external art assets.

## Requirements

### Requirement: Lightable prototype geometry
Every rendered face generated from an immutable prototype solid SHALL carry a finite world-space position and a finite, unit-length world-space normal that points away from the solid. Both triangles forming the same planar face SHALL use the same normal, and the opaque lighting stage SHALL receive an interpolated world-space surface position for each covered fragment.

#### Scenario: Prototype solids are expanded for rendering
- **WHEN** the built-in axis-aligned solids are converted to opaque triangle vertices
- **THEN** each face's vertices contain the matching world-space positions and outward-facing unit normal

#### Scenario: A scene fragment is shaded
- **WHEN** an opaque prototype triangle produces a covered fragment
- **THEN** local-light evaluation uses the fragment's interpolated world-space surface position and the face's outward-facing normal

### Requirement: Immutable prototype environment light
The built-in prototype level SHALL define exactly two immutable world-space point lights and one non-negative near-black ambient contribution. Each point light SHALL have a finite position, a finite non-negative RGB color, a positive finite intensity, and a positive finite influence radius. Invalid authored lighting SHALL fail level validation before renderer startup, and the renderer SHALL use the same valid environment-light description for every scene frame.

#### Scenario: Prototype lighting initializes
- **WHEN** the immutable prototype level is constructed
- **THEN** it exposes two valid point lights and a bounded near-black ambient contribution without loading an external light or scene file

#### Scenario: Authored lighting is invalid
- **WHEN** a point light has a non-finite value, negative color component, non-positive intensity, or non-positive influence radius
- **THEN** prototype-level validation rejects the environment-light description before renderer startup

#### Scenario: Camera orientation changes
- **WHEN** the player looks or moves while the level remains unchanged
- **THEN** both lights remain fixed in world space rather than following the camera

### Requirement: Bounded local diffuse scene shading
The opaque prototype scene SHALL combine its near-black ambient contribution with diffuse illumination from both authored point lights. Each point-light contribution SHALL depend on surface orientation and SHALL fall smoothly to zero at its finite influence radius. The accumulated lighting SHALL remain bounded, surfaces outside both light radii SHALL receive only the near-black ambient contribution, and the scene SHALL preserve its existing textured, depth-tested opaque draw.

#### Scenario: Surface is near an authored light
- **WHEN** a fragment lies inside a point light's influence radius and faces toward that light
- **THEN** its displayed illumination includes a distance-attenuated contribution using that light's authored color and intensity

#### Scenario: Surface faces away from an authored light
- **WHEN** a fragment lies inside a point light's influence radius but its outward normal faces away from that light
- **THEN** that light contributes no diffuse illumination to the fragment

#### Scenario: Surface is outside every local light
- **WHEN** a fragment lies at or beyond both authored influence radii
- **THEN** it receives only the near-black ambient contribution and remains intentionally dark

#### Scenario: Prototype atmosphere is inspected
- **WHEN** the built-in scene is viewed from its initial player route
- **THEN** it presents a dim spawn light, a distinct destination light, and an intervening region that falls into intentional darkness

### Requirement: Packaged lighting shaders
The lit prototype scene SHALL remain executable from the existing explicit runtime resource root and SHALL require only the packaged scene shaders and fixed prototype surface textures. It SHALL NOT require a general material, model, scene, or external lighting data file.

#### Scenario: Executable-relative resources are complete
- **WHEN** the launcher supplies a valid executable-relative resource root containing the packaged lit scene shaders and fixed surface textures
- **THEN** renderer startup can construct the lit textured scene pipeline without consulting the process working directory or additional graphics assets
