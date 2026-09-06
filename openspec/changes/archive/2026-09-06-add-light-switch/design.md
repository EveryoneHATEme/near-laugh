## Context

See `proposal.md` for motivation and scope. The runtime currently loads a validated version-2 `PrototypeLevel`, builds immutable scene meshes and a static Jolt world, then coordinates one player and flashlight. `LightingResources` uploads two point lights and ambient once. `FrameRequest` supplies the camera and optional spotlight; their Vulkan push constant already occupies the guaranteed 128 bytes, with two unused scalar lanes in its final vector.

The editor has a flat object set, concrete variant-based property commands, analytic picking, and 128-entry history. It deliberately does not link gameplay runtime or physics. Those boundaries remain useful for this change.

## Goals / Non-Goals

**Goals:**

- Keep authored configuration independent from changing light state, with a clear owner for each.
- Make target eligibility and input edges deterministic and independently testable.
- Reuse immutable rendering resources for state changes and preserve the current Vulkan device baseline.
- Integrate the singleton switch into existing authoring and persistence behavior.

**Non-Goals:**

- General interaction interfaces, persistent object IDs, multiple switches, light registries, or event dispatch frameworks.
- Mutation of authored intensity to represent an off light, mutable scene meshes during play, or editor-owned game simulation.
- A general file migration framework or compatibility with removed version-1 shooter data.

## Decisions

### Store an optional singleton in version 3 and preserve version-2 loading

Add a concrete switch value containing `position` (the plate centre), `yaw_degrees`, `point_light_index` (0 or 1), and `initially_on`. Version 3 requires a `light_switch` field containing that object or null. The remaining document profile is unchanged. Neither runtime nor file format needs a switch ID because there is at most one; light slots already have a stable order and cannot be removed or reordered in the editor.

The private codec accepts the exact former version-2 shape, validates its fields, and normalizes it into the current in-memory document with no switch. It accepts the version-3 shape separately and rejects unknown fields for each version, missing required fields, malformed switch objects, version 1, and unknown versions. Saves always emit canonical version 3. Opening version 2 neither rewrites its source nor marks the normalized document dirty; the UI identifies that the next explicit save writes version 3. This small compatibility branch preserves existing authored work without a migration framework. Reusing version 2 for the new shape was rejected because older readers reject the added field.

Switch validation checks finite position/yaw and transformed bounds, an integer light index in range, an actual boolean initial state, and horizontal placement within the terrain footprint. Fixed plate dimensions avoid arbitrary interaction-size authoring. Validation does not require a wall attachment, prove accessibility, or reject occlusion: authors can temporarily place a switch behind geometry, and runtime obstruction checks still prevent activation. The editor can render finite cross-object-invalid values while saving is gated, as with other objects.

### Generate one static, non-blocking switch plate

Generate a small pale plate with a contrasting fixed rocker using the existing obstacle texture and opaque vertex tints. Use a fixed 0.18 by 0.26 metre face and approximately 0.04 metre depth; one shared world definition supplies its yawed picking bounds and placement convention to runtime targeting, editor picking, and geometry generation. A small amount of local geometry code is preferable to an asset catalog or new material path. Its shape and transform remain immutable during play; the controlled light supplies state feedback.

The plate is a non-blocking interactive decoration, not a new structural solid or physics body. Static-world collision remains unchanged. Geometry is appended to the existing generated-world mesh at startup and in editor rebuilds, including terrain-only rebuilds. The imported chair retains its separate mesh and proxy.

The packaged switch sits just in front of the spawn-facing surface of the existing central obstacle, near standing eye height, facing the approach. It controls light slot 0 and starts on. Tune the exact placement during manual validation so the plate is distinguishable, reachable, and not embedded in the obstacle. No additional external asset is needed.

### Own the run-local state in one concrete gameplay object

Add a small runtime-owned light-switch controller beside the flashlight. It borrows the immutable optional switch definition, owns the current on/off bit and interaction input latch, and produces the two enabled values for the environment lights. Both lights default on; an existing switch applies its initial value only to its linked slot. Toggling never writes `LevelDocument`, changes a light's positive authored intensity, or alters physics.

The runtime is the sole owner of this state. Renderer recreation, skipped frames, and minimization preserve it; a newly started application initializes it from the authored definition. There is no save-game state in this milestone.

### Evaluate a bounded view ray and static obstruction

On an eligible new E press, intersect the switch's yawed box with the normalized forward ray from the same interpolated player eye pose used for that iteration's camera. Accept a nearest non-negative surface distance of at most 2 metres; reject a ray starting inside the plate, a miss, an absent switch, or a target beyond reach.

For a candidate hit, ask `PhysicsWorld` whether existing static collision blocks the eye-to-hit segment. Keep Jolt queries and filters private and return only a project-owned result. Include terrain, solids, and the transformed chair proxy, using their current collision representations; exclude the player. This reuses authoritative blockers instead of duplicating terrain collision logic in gameplay or linking the editor into the runtime. A blocker at the target distance also prevents interaction; document a small numerical endpoint tolerance and verify a plate placed just outside a wall remains usable. Handle rays originating inside blocking collision as blocked.

Target-box math can be a small pure helper in the world module shared by its two actual consumers, runtime and editor. Do not promote it into a query service, scene registry, or general picking framework. Physics tests verify the actual visibility query against known geometry, including the proxy approximation.

### Consume interaction once per sampled event batch

Add the physical E key and a separate player `interact` action. Do not repurpose either existing mouse action. Sample its latch once per processed input batch; observe releases even while interaction is inactive. A held press cannot retrigger, become active when the player later looks at the switch, or survive cursor release/recapture as a pending action. Require an observed release before arming at startup.

In a renderable iteration, sample input/capture transitions, advance the existing fixed simulation steps, derive the displayed view, and consume that iteration's eligible press once. Targeting uses the resulting view. It runs even if there were zero simulation steps, and never once per catch-up step. Minimized, cursor-released, capture-transition, and closing batches cannot activate the switch and consume any press they observe without retaining it. A renderer skip/recovery does not roll back or replay an interaction already accepted by the runtime.

### Supply source-independent enable values without changing light resources

Extend the backend-neutral frame data with two point-light enabled values, defaulting to true. They identify the existing light slots, not a switch. The renderer multiplies each point light's evaluated contribution by 0 or 1; ambient and the independently supplied spotlight are unaffected. Keep the immutable authored lighting uniform buffer and descriptors.

Explicitly repack the renderer-private push constant's final vector as `(spot outer cosine, spot enabled, point 0 enabled, point 1 enabled)`. Retain the camera plus three spotlight vectors before it, keeping the total at 128 bytes. Use a distinct renderer-private packed layout rather than hiding unrelated state inside the public `SpotLightFrame` value; the standalone spotlight contract and its zeroed disabled representation remain intact. Update both shader declarations and their packaged SPIR-V together.

This fits the existing bounded data without new buffers, descriptors, descriptor updates, or GPU waits on each toggle. Per-frame uniform buffers were considered but add lifetime and synchronization work that two booleans do not require. Layout tests should verify the host/shader ABI and correct packing of both lights with the flashlight on and off, while behavioral tests protect lighting independence.

### Extend the existing editor commands and preview

Represent the optional switch with one reserved transient editor ID and a concrete variant value. Allow adding it only when absent, removing it when selected, and restoring it through undo/redo; duplication remains unavailable. Ensure the reserved ID cannot collide with existing fixed objects or allocated solid IDs.

Expose position, yaw, light selection labelled Point light 1/2 (serialized slots 0/1), and Initially on. New switches start near the spawn at standing interaction height, linked to light 0 and on. Numeric properties provide exact wall placement; terrain placement preserves the switch's previous height above its terrain anchor, as for lights. No wall-snapping tool is introduced.

List and viewport picking share selection and use the same plate bounds as runtime targeting. Every add/remove/property/placement command shares the existing history, validation, dirty state, and transactional preview rules. Preview derives the two light enables from the authored initial state, not a runtime controller; changing the linked slot restores the old slot to enabled. A null or structurally unusable switch is omitted from preview without invalid indexing. Terrain-only rebuilds must retain its geometry and authored light preview state. The editor does not offer E-to-interact or play mode.

## Risks / Trade-offs

- **[The small plate is difficult to find or aim at]** -> Use contrasting static geometry, place the example on the approach route, document E, and manually verify targeting with the flashlight both on and off. A HUD is a separate decision.
- **[Collision proxies differ from visible prop silhouettes]** -> Obstruction intentionally follows the existing chair box proxy. Tests and documentation state this approximation; do not import model triangles into physics.
- **[Wall contact or ray endpoints cause false obstruction]** -> Put the plate visibly in front of its support, share target dimensions, and cover endpoint, inside-blocker, occluded, and just-outside-wall cases.
- **[Changes accidentally alter flashlight packing or light state after recovery]** -> Keep public spotlight values separate from packed shader data and exercise every light-enable combination plus resize/minimize/recovery in smoke tests.
- **[Old executables cannot reopen a newly saved level]** -> Retain version-2 read support, identify the version-3 save behavior in the editor and documentation, and never rewrite a file merely on open.

## Migration Plan

1. Add the bounded version-3 document and version-2 normalization path, shared validation, and codec tests.
2. Add interaction input, the static visibility query, and runtime state with deterministic behavior tests.
3. Integrate generated geometry and per-frame light enables into game and editor rendering; update packaged shaders together.
4. Integrate editor commands/preview and author the packaged example as version 3.
5. Update affected documentation, build the game/editor/test targets, run deterministic and Vulkan smoke checks, and manually verify the authored interaction and editor round trip.

Rollback requires matching code and packaged shaders/level from before the change. Existing version-2 sources remain usable; any separately saved version-3 authored work must be retained for the newer build rather than silently downgraded.
