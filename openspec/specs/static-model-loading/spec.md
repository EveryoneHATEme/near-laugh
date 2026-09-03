# static-model-loading Specification

## Purpose

Defines the bounded binary glTF profile and deterministic startup conversion used to load one immutable opaque static prop for the game prototype world.

## Requirements

### Requirement: Bounded static GLB asset profile
The runtime SHALL accept exactly one packaged binary glTF 2.0 model containing one default scene, one mesh-bearing root node with no child nodes, and exactly one mesh with one non-empty triangle-list primitive. Geometry data SHALL be embedded in the GLB binary chunk; the primitive SHALL provide finite `POSITION`, `NORMAL`, and `TEXCOORD_0` accessors and MAY provide unsigned 8-bit, 16-bit, or 32-bit indices. The model SHALL NOT require extensions, sparse accessors, compression, skins, animation, morph targets, cameras, lights, multiple primitives, or external buffers. File-defined material and texture data SHALL NOT affect the prototype prop's appearance.

#### Scenario: Supported model is loaded
- **WHEN** the required packaged GLB satisfies the bounded static profile
- **THEN** startup obtains one non-empty triangle mesh without loading another model, buffer, image, material, animation, or scene file

#### Scenario: Unsupported model feature is present
- **WHEN** the required GLB uses a non-triangle primitive, required extension, sparse or compressed accessor, external buffer, child node, additional mesh or primitive, skin, animation, or morph target
- **THEN** startup rejects the model with an actionable error identifying the unsupported feature and resolved model path

### Requirement: Deterministic world-space mesh conversion
The loader SHALL traverse the accepted primitive in declared index or vertex order, expand it to a non-indexed triangle stream, combine the model node transform with the immutable prototype-level translation, yaw, and positive uniform scale, and produce finite world-space positions, finite unit-length world-space normals, and preserved finite `TEXCOORD_0` values. Normal conversion SHALL remain correct for the accepted node transform, and the output vertex count SHALL be non-zero, divisible by three, and representable by the renderer's 32-bit draw count.

#### Scenario: Indexed primitive is converted
- **WHEN** the accepted primitive supplies valid indices
- **THEN** output vertices follow index order and reference the corresponding position, normal, and texture-coordinate elements

#### Scenario: Placement is applied
- **WHEN** the model has a valid node transform and the prototype level supplies its static prop placement
- **THEN** every output position and normal is transformed once into the world space used by the existing camera and lighting calculations

#### Scenario: Geometry data is invalid
- **WHEN** an accessor is missing, mismatched, out of bounds, non-finite, degenerate for normal transformation, empty, or too large for the renderer draw contract
- **THEN** startup rejects the model before creating a drawable model mesh and reports the resolved model path and validation reason

### Requirement: Synchronous immutable model lifetime
The required model SHALL be resolved and loaded synchronously during startup before the first frame, SHALL remain immutable for the runtime lifetime, and SHALL be released during orderly renderer teardown. The runtime SHALL NOT discover, reload, stream, cache, or mutate model assets while the main loop is running.

#### Scenario: Runtime enters the frame loop
- **WHEN** model parsing, validation, conversion, and renderer initialization all succeed
- **THEN** every frame uses the same immutable loaded model geometry without additional model file access

#### Scenario: Model startup fails
- **WHEN** the model is missing, unreadable, malformed, unsupported, or invalid
- **THEN** startup does not enter the frame loop and releases every already-created runtime and renderer resource exactly once
