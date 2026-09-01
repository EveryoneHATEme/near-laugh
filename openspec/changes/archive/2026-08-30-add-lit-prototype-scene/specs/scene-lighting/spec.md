## Purpose

Defines the small, deterministic environment-lighting model that makes the built-in opaque FPS prototype scene spatially readable without external art assets.

## ADDED Requirements

### Requirement: Lightable prototype geometry
Every rendered face generated from an immutable prototype solid SHALL carry a finite, unit-length world-space normal that points away from the solid, and both triangles forming the same planar face SHALL use the same normal.

#### Scenario: Prototype solids are expanded for rendering
- **WHEN** the built-in axis-aligned solids are converted to opaque triangle vertices
- **THEN** each face's vertices contain the matching outward-facing unit normal

### Requirement: Immutable prototype environment light
The built-in prototype level SHALL define one finite, normalized world-space directional light together with non-negative directional and ambient contributions, and the renderer SHALL use that same environment-light description for every scene frame.

#### Scenario: Prototype lighting initializes
- **WHEN** the immutable prototype level is constructed
- **THEN** it exposes a valid directional-light orientation and bounded directional and ambient contributions without loading an external light or scene file

#### Scenario: Camera orientation changes
- **WHEN** the player looks or moves while the level remains unchanged
- **THEN** the light remains fixed in world space rather than following the camera

### Requirement: Directional diffuse scene shading
The opaque prototype scene SHALL modulate each surface's base color using its world-space normal, the directional light, and the ambient contribution. Surfaces oriented toward the directional light SHALL receive more directional illumination than otherwise identical surfaces oriented away from it, while the ambient contribution SHALL keep valid unlit-facing surfaces visible.

#### Scenario: Differently oriented faces are visible together
- **WHEN** two opaque faces with the same base color but different normals are rendered under the prototype environment light
- **THEN** their displayed brightness differs according to orientation and neither valid face becomes unintentionally invisible

#### Scenario: A lit frame is submitted
- **WHEN** renderer initialization succeeds and the runtime supplies a renderable camera frame
- **THEN** the built-in scene is presented with directional and ambient shading through the existing depth-tested opaque scene draw

### Requirement: Packaged lighting shaders
The lit prototype scene SHALL remain executable from the existing explicit runtime resource root and SHALL NOT require texture, material, model, or lighting data files in addition to the packaged scene shader resources.

#### Scenario: Executable-relative resources are complete
- **WHEN** the launcher supplies a valid executable-relative resource root containing the packaged lit scene shaders
- **THEN** renderer startup can construct the lit scene pipeline without consulting the process working directory or additional graphics assets
