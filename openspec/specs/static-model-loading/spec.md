# static-model-loading Specification

## Purpose

Defines the controlled binary glTF profile and deterministic startup conversion of packaged static models with bounded base-color materials and immutable scene lifetime.

## Requirements

### Requirement: Bounded static GLB asset profile
The runtime SHALL accept the selected packaged binary glTF 2.0 models using one default scene, one mesh-bearing root without children, and one mesh with one non-empty triangle-list primitive per asset. Geometry SHALL be embedded in the GLB binary chunk, with finite POSITION, NORMAL and TEXCOORD_0 accessors and valid optional unsigned 8-bit, 16-bit or 32-bit indices. The accepted root and placement transforms SHALL be finite and nonsingular. The controlled profile SHALL support one base-color OPAQUE/MASK material and at most one embedded PNG base-color image as defined by authored-scene-assets, or a material-free constant-white/explicit legacy catalog assignment. A present material SHALL explicitly use metallic factor 0, roughness factor 1 with no roughness texture, zero emission and no emission/normal/occlusion textures; double-sided materials and other shading inputs SHALL be unsupported. An absent sampler or absent filters SHALL select repeat/linear/trilinear; explicitly supported filter pairs SHALL be nearest/nearest-mip and linear/trilinear with repeat wrapping, and other pairs SHALL be rejected. It SHALL reject required extensions, sparse accessors, compression, skins, animation, morph targets, cameras, lights, multiple primitives, child nodes, external buffers/images, and unsupported material or sampler inputs. GLB size SHALL be at most 16 MiB, an embedded image at most 2048 by 2048, and expanded output at most 300,000 vertices per asset; byte ranges and decoded allocation sizes SHALL be checked before use.

#### Scenario: Supported model is loaded
- **WHEN** the required packaged GLB satisfies the bounded static profile
- **THEN** startup obtains the supported geometry and material with its optional embedded base-color image without following external references

#### Scenario: Unsupported model feature is present
- **WHEN** the required GLB uses a non-triangle primitive, required extension, sparse or compressed accessor, external buffer/image, unsupported material/sampler, child node, additional mesh or primitive, skin, animation, or morph target
- **THEN** startup rejects the model with an actionable error identifying the unsupported feature and resolved model path

#### Scenario: Oversized or malformed texture is supplied
- **WHEN** a required model exceeds the documented bounds or contains an invalid embedded image or out-of-range image buffer view
- **THEN** loading rejects it before unsafe decoding/allocation and identifies the asset and image context

### Requirement: Deterministic world-space mesh conversion
The loader SHALL traverse the accepted primitive in declared index or vertex order, expand it to a non-indexed triangle stream, combine the model node transform with the authored placement translation, yaw, and positive uniform scale, and produce finite world-space positions, finite unit-length world-space normals, and preserved finite `TEXCOORD_0` values. Normal conversion SHALL remain correct for the accepted node transform, and the output vertex count SHALL be non-zero, divisible by three, and representable by the renderer's 32-bit draw count.

#### Scenario: Indexed primitive is converted
- **WHEN** the accepted primitive supplies valid indices
- **THEN** output vertices follow index order and reference the corresponding position, normal, and texture-coordinate elements

#### Scenario: Placement is applied
- **WHEN** the model has a valid node transform and the level supplies any placement of that model
- **THEN** every output position and normal is transformed once into the world space used by the existing camera and lighting calculations

#### Scenario: Geometry data is invalid
- **WHEN** an accessor is missing, mismatched, out of bounds, non-finite, degenerate for normal transformation, empty, or too large for the renderer draw contract
- **THEN** startup rejects the model before creating a drawable model mesh and reports the resolved model path and validation reason

### Requirement: Synchronous immutable model lifetime
Every required static model and its material resources SHALL be resolved and loaded synchronously before the first frame, remain immutable for the run, and be released during orderly renderer teardown. Repeated references SHALL reuse the same decoded model/material resources within a scene load, while each placement retains its own transform. The runtime SHALL NOT discover, hot-reload, stream or mutate model assets during its main loop; an explicit editor scene replacement SHALL use transactional replacement resources.

#### Scenario: Runtime enters the frame loop
- **WHEN** model parsing, validation, conversion, and renderer initialization all succeed
- **THEN** every frame uses the same immutable loaded model geometry without additional model file access

#### Scenario: Model startup fails
- **WHEN** the model is missing, unreadable, malformed, unsupported, or invalid
- **THEN** startup does not enter the frame loop and releases every already-created runtime and renderer resource exactly once
