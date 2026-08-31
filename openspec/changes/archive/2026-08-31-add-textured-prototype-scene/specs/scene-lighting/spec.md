## MODIFIED Requirements

### Requirement: Directional diffuse scene shading
The opaque prototype scene SHALL form each surface's base color from its sampled fixed surface texture modulated by its authored tint, apply the existing highlight-or-dim presentation to that base color, and modulate the presented color using its world-space normal, the directional light, and the ambient contribution. Surfaces oriented toward the directional light SHALL receive more directional illumination than otherwise identical surfaces oriented away from it, while the ambient contribution SHALL keep valid unlit-facing surfaces visible.

#### Scenario: Differently oriented faces are visible together
- **WHEN** two opaque faces with the same sampled texture and tint but different normals are rendered under the prototype environment light
- **THEN** their displayed brightness differs according to orientation and neither valid textured face becomes unintentionally invisible

#### Scenario: A lit frame is submitted
- **WHEN** renderer initialization succeeds and the runtime supplies a renderable camera frame
- **THEN** the built-in textured scene is presented with directional and ambient shading through the existing depth-tested opaque scene draw

### Requirement: Packaged lighting shaders
The lit prototype scene SHALL remain executable from the existing explicit runtime resource root and SHALL require only the packaged scene shaders and fixed prototype surface textures. It SHALL NOT require a general material, model, scene, or external lighting data file.

#### Scenario: Executable-relative resources are complete
- **WHEN** the launcher supplies a valid executable-relative resource root containing the packaged lit scene shaders and fixed surface textures
- **THEN** renderer startup can construct the lit textured scene pipeline without consulting the process working directory or additional graphics assets
