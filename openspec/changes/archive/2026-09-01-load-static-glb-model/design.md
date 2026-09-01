## Context

The renderer currently converts `PrototypeLevel` boxes and terrain directly into one `PositionColorVertex` vector, and `GraphicsPipeline` owns both Vulkan pipeline state and the single host-visible vertex buffer. One immutable four-layer texture array and one immutable lighting descriptor serve the one opaque draw. Runtime resource resolution knows only the scene shaders and four fixed textures, while physics independently derives Jolt heightfield and box collision from the same level description.

This change introduces the first file-authored geometry source. It must cross runtime resource resolution, world placement, renderer startup, GPU ownership, and static physics without turning the prototype level into a scene graph or introducing a general asset pipeline. See `proposal.md` for motivation and the capability deltas for the behavioral contract.

## Goals / Non-Goals

**Goals:**

- Isolate GLB parsing and profile validation from Vulkan resource creation.
- Preserve the world module's backend- and filesystem-independent data boundary.
- Establish explicit reusable ownership for two immutable opaque vertex buffers driven by one Vulkan pipeline.
- Make every startup failure name the concrete model path and failed parse, validation, conversion, or Vulkan operation.
- Keep the first imported asset deterministic, small, visible from the prototype route, and covered by automated and validation-assisted tests.

**Non-Goals:**

- Do not add model identifiers, a scene hierarchy, an asset registry, asynchronous loading, or runtime model mutation.
- Do not add per-object model matrices to the 128-byte camera-plus-spot-light push constant contract.
- Do not introduce indexed GPU drawing, device-local geometry staging, batching, instancing, or mesh optimization before measurement warrants them.
- Do not interpret glTF materials or images, generate missing normals or UVs, or make collision from render triangles.

## Decisions

### Use a pinned renderer-private cgltf dependency

Add `cgltf` at a fixed Git commit through the existing CMake `FetchContent` pattern and expose its include directory only to `near_laugh_render`. Compile its implementation in exactly one renderer-private translation unit. The loader API returns engine-owned CPU data and does not expose `cgltf` declarations or allocation rules.

`cgltf` fits this milestone because it is a small dependency-free glTF 2.0 parser and leaves file/profile policy explicit in engine code. Assimp was rejected because multi-format import and its larger dependency surface conflict with the one-format FPS scope. TinyGLTF and fastgltf were considered, but their bundled or transitive parsing machinery is unnecessary for one synchronous controlled asset. A hand-written GLB/glTF parser was rejected because buffer/accessor validation is subtle and not game-specific value.

### Keep asset location and world placement separate

`RuntimeResources` gains the normalized absolute `models/prototype_chair.glb` path and continues to verify every required file before subsystem construction. CMake copies the `resources/models` directory beside the executable with shaders and textures.

`PrototypeLevel` gains exactly one concrete static-prop description containing translation, yaw, positive uniform scale, the obstacle surface role, and box-proxy center/half extents. It contains no path, asset id, parser type, or Vulkan/Jolt value. `Engine` composes the one model path with the one placement when constructing the renderer, while physics consumes only the placement and proxy.

This avoids both filesystem data in the world module and a one-entry asset registry. The concrete one-to-one composition is sufficient for the current FPS prototype.

### Parse, validate, and flatten the accepted GLB during startup

A renderer-private loader reads the resolved GLB, uses `cgltf` parsing, buffer loading, and structural validation, then applies the narrower engine profile from `static-model-loading`. External buffer URIs and required extensions are rejected before conversion. Parser data is held by a local RAII owner and freed on every exit path.

The loader walks the sole triangle primitive in index or vertex order and emits the existing opaque vertex representation. It combines the glTF root-node transform with the level-authored translation, yaw, and uniform scale, transforms normals with the inverse transpose of the combined linear transform, renormalizes them, preserves `TEXCOORD_0`, writes opaque white tint, and selects the obstacle texture layer. Singular transforms, non-finite results, accessor mismatches, range errors, empty output, and 32-bit draw-count overflow fail startup.

Flattening indices deliberately trades some memory for the smallest renderer change and exact compatibility with the current `vkCmdDraw` path. The first asset is bounded and loaded once, so an index buffer is not yet justified. File material metadata is ignored after structural parsing because appearance is fixed by the prototype level rather than by a partial material implementation.

### Separate opaque pipeline state from immutable mesh-buffer ownership

Extract the current vertex-buffer allocation, mapping, upload, bind, and destruction behavior from `GraphicsPipeline` into a small renderer-private RAII immutable mesh-buffer owner. `GraphicsPipeline` retains graphics pipeline/layout creation and borrows the existing texture and lighting descriptors; it no longer takes `PrototypeLevel` or owns scene geometry.

Renderer initialization builds generated-world vertices and loaded-prop vertices on the CPU, constructs one mesh-buffer owner for each, and creates the shared pipeline. Frame recording binds the pipeline, immutable descriptors, and scene push constant once, then binds and draws the generated world buffer and imported prop buffer in deterministic order. Both draws use the same camera and lighting values. Mesh buffers survive swapchain recreation and are destroyed before device teardown; pipeline recreation caused by a swapchain format change borrows them without re-uploading them.

Keeping two concrete mesh owners makes the new ownership boundary real without adding generic render items, materials, draw sorting, or a render graph. Host-visible coherent memory remains the geometry upload policy for this bounded startup asset.

### Use a project-owned low-poly chair as the first model

Package one small, project-owned `prototype_chair.glb` containing one scene, root node, mesh, and triangle primitive with positions, normals, UVs, and embedded geometry. Its silhouette must be visibly distinct from the generated boxes, and its placement must be visible along the initial route. The model may contain otherwise valid material metadata, but runtime appearance remains the fixed obstacle texture and white tint.

The runtime asset doubles as the successful loader fixture. Unit tests construct additional minimal GLB byte fixtures in temporary directories for malformed, unsupported, indexed, non-indexed, and transform cases instead of packaging extra runtime models.

### Keep collision as an explicit authored box proxy

Physics creates one additional static Jolt box from the prototype prop's proxy and world placement. It does not link the model parser, inspect loaded vertices, or depend on renderer state. Proxy dimensions are authored conservatively around the chair's blocking volume and validated with the rest of `PrototypeLevel`.

This preserves simple deterministic character collision. Exact mesh collision would increase cooking, lifetime, and geometry-sharing complexity without a demonstrated gameplay need.

## Risks / Trade-offs

- [The narrow profile rejects ordinary exports with multiple primitives or required extensions] -> Keep one controlled project asset, document the accepted profile in diagnostics and development notes, and cover each rejection class with focused tests.
- [Node or coordinate transforms can produce incorrect placement, winding, or lighting] -> Use a known right-handed Y-up asset, assert selected transformed vertices/normals in deterministic tests, retain no-face-culling behavior, and inspect the prop under both point and spot lights.
- [Flattening an indexed model duplicates vertices] -> Bound the first prop and retain the simple draw path; measure geometry memory before proposing indexed GPU buffers.
- [Separating mesh ownership from pipeline state can regress partial-construction cleanup] -> Preserve RAII, add injected mesh-buffer failure stages to the Vulkan smoke test, and verify destruction ordering relative to pipeline and device lifetime.
- [A box proxy does not match chair openings or thin parts exactly] -> Size it as a deliberate blocking volume and verify approach from playable directions; propose compound or mesh collision only if gameplay requires that fidelity.
- [A binary GLB is harder to review in diffs] -> Keep it small and project-owned, validate it through deterministic loader tests, and record its profile and visible purpose in project documentation.

## Migration Plan

1. Add the pinned parser dependency, packaged model path, runtime copying, and CPU loader/profile tests without changing the frame loop.
2. Add the concrete prototype prop placement and static collision proxy with world and headless physics tests.
3. split immutable mesh-buffer ownership from pipeline state, upload both meshes, and record the two opaque draws.
4. Update resource/process tests, lifecycle failure injection, Vulkan smoke coverage, documentation, and visual validation.
5. Roll back by removing the model resource/path, prop description/proxy, second mesh buffer/draw, and parser dependency; no saved data or compatibility migration is involved.
