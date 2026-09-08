## Context

See [proposal](proposal.md) for the P07 split. The current renderer loads
one-primitive static GLBs privately, flattens placements into world-space
material batches, and uploads accepted door boxes into fenced changing buffers.
P10 renders those batches into both color and six-face point-shadow passes.
The editor shares rendering helpers but does not link game physics/runtime.

[Local inspection](asset-inspection.md) supplies the actual asset constraints:
65 joints, 8,546 indexed vertices, 41,232 indices, two constant materials, three
selected in-place clips and effectively unit scales. No retargeted body has
been demonstrated. P07a needs no level-format migration.

## Goals / Non-Goals

**Goals:** establish a reproducible small animated asset, testable CPU sampling,
one coherent pose per displayed character, and measured integration with the
existing rendering path.

**Non-Goals:** route simulation, actor records, editor authoring, runtime
retargeting, arbitrary character imports, GPU skinning infrastructure,
animation graphs, root motion, extra shading inputs or final character art.

## Decisions

### 1. Prepare the embedded mannequin offline

Add a deterministic game-specific preparation script using the pinned local
UAL1_Standard.glb, producing a committed mannequin GLB and catalog metadata.
Keep idle/walk/interact; strip the other 40 clips, secondary UVs and constant
scale channels. Normalize negligible unit-scale drift within 0.00001; reject
a larger difference rather than flatten meaningful scale animation. Preserve
hierarchy/root orientation and recompute bind data if normalization changes
the rest transforms. Verify evaluated bind geometry within 0.0001 m of source.
Preserve the two base-color factors, explicitly set metallic=0/roughness=1,
retain two-sided opaque presentation and omit unused material properties.

The profile's caps are ceilings for this selected asset, not a promise to
import arbitrary assets of that size. Check sizes/ranges before allocation,
including hierarchy, keys, index ranges and derived outputs. Retain source
and derivative hashes and included notices under resources; normal packaging
copies only prepared resources. Read local sources by an explicit path and
fail if hashes differ. Do not depend on the ignored build directory at runtime.

Catalog identity is `test-mannequin`; clip identities are `idle`, `walk`,
`interact`. Metadata records the neutral feet origin/forward axis, actor
capsule, conservative preview bounds, walk cycle distance, two contact phases
and one interaction phase. Calibrate the three phase values and walk distance
by sampling/visual inspection in this stage and commit them with the asset;
P07b consumes them without guessing timing from clip names. Metadata validation
requires finite positive cycle distance, ordered contact phases in [0,1), and
interaction phase strictly inside (0,1). These measurements are task outputs,
not unresolved choices about the importer or gameplay architecture.

Using the library mannequin avoids introducing an unverified retargeting
algorithm just to test animation. A later Superhero asset task can reuse this
pipeline after proving its deformation/material conversion separately.

### 2. CPU animation with a narrow shared ownership boundary

Add `near_laugh_animation` for immutable prepared animated data, decoding,
local pose sampling/blending and deterministic deformation. Runtime, viewer
and editor are concrete consumers. Its interface uses project scalar/container
types; cgltf and GLM stay private and Vulkan/Jolt are absent. Give the pinned
cgltf implementation one shared compiled owner used by static and animated
loaders; keep their validators and accepted profiles separate.

Sample local TR values then evaluate parent-before-child global transforms.
Use normalized shortest-arc quaternion interpolation; missing channels use
rest values. Pose blending uses a captured current local pose and one target
clip/time, with the target advancing during the fixed 0.15 s blend.
Interrupting captures the displayed blend, avoiding a graph or blend tree.
The caller owns time, seek, looping and one-shot result consumption.

Apply mesh-relative joint globals and inverse binds before the single world
placement transform. With identity mesh transforms and unit joint scale,
weighted rotated normals are renormalized; reject degenerate/non-finite
results. Bind-pose identity, a tiny known two-joint fixture and nonzero
placement/yaw tests catch matrix order, double transforms and normal mistakes.
Do not rely solely on inspecting a complex downloaded model.

### 3. Deform once and use the existing material pipelines

Use CPU skinning of source vertices once per instance, preserving one changing
world-space vertex per source vertex in the existing vertex layout. Store each
distinct selected asset's uint32 indices once in an immutable shared buffer;
instances share its index/material ranges and select their own deformed vertices
with a checked signed vertex offset. Preserve declared triangle and primitive
order. At four instances this is bounded to 40,000 evaluated/uploaded vertices
and 200,000 aggregate drawn indices, counting repeated instances even when their
stored indices are shared. Check index ranges, vertex offsets and byte products
before allocation or draw. Reuse the same indexed ranges for every shadow face
and color draw.

This revision was authorized by the user on 2026-09-08 after the expanded
baseline missed all four-character GPU/frame gates. The retained evidence and
technical review are in [performance-followup.md](performance-followup.md).
The payload reduction is measured separately from any frame-time benefit;
the original failures and acceptance thresholds remain unchanged.

Use separate character buffers per existing frame slot, sized during selected
scene creation, with checked capacity and the same fence/write/flush discipline
as existing changing door buffers. Initialize immutable indices during scene
creation and retain them through recovery, with partial-construction cleanup
and transactional editor replacement covering their buffer and memory too.
Do not enlarge the door box contract or
rebuild static meshes. Reuse current constant-white material textures,
lighting descriptors, push constants and shaders where compatible. The current
pipeline already renders both sides; no new material/pipeline variant is
required by the prepared asset. Preserve both per-primitive colors.

GPU skinning would require additional vertex layouts, pose descriptors and
shader variants in both color and shadow paths. It is deferred until measured
CPU deformation/upload costs justify that complexity. CPU skinning is a
baseline decision, not a performance claim. If the supported scene misses the
budget, retain evidence and revisit the measured bottleneck before acceptance.

### 4. Pose input is presentation, not animation policy

The selected scene prepares immutable character resources once. Frame input
borrows an exact matching instance set with model-relative joint poses and
world transforms, bounded to four. Selection uses render-instance handles,
not actor/route IDs. Validate completeness, uniqueness, palette counts and
finite values before recording; copy into slot-owned GPU storage only after
its fence. Zero selected instances requires no character assets.

The renderer never advances a clip or retains the borrowed span. Caller
state survives Rendered/Skipped/Recovered. Candidate editor replacement owns
a complete compatible set until successful installation; no stale character
palette may be paired with a new model. Destroy pipelines and dependent GPU
work before character buffers/materials, then immutable CPU owners.

### 5. Explicit viewer and evidence before routes

Provide `character_animation_viewer` as a development executable using shared
runtime/platform/render helpers and an explicit mannequin selection. Controls
select clips, pause/resume, restart, seek and choose one/four placements.
No level filename starts this viewer's sequence; ordinary game/editor defaults
remain without selected characters until P07b supplies definitions.

Inspect bind/idle/walk/interact, loop endpoints, mid-blend interruption,
turning placement yaw, off-screen silhouettes, distinct material colors,
normals, flashlight and point shadows. Add automated readbacks at stable
times and independent deformation references, plus startup/allocation failure,
repeated creation, zero/four instances and swapchain/format recovery smoke.

Collect baseline/one/four-character Release runs under the existing T1 machine
and 1920x1080/60 Hz conditions, separating active CPU, deformation/upload,
GPU shadow/whole-frame time and presentation/fence waits. Use 10 s warm-up and
three 60 s samples for each compared setup. Target the existing T1 frame gates
(16.67 ms CPU/GPU p95; 16.9/20/33.4 ms frame p50/p95/p99) and report each result;
preserve any failure instead of silently lowering character or light capacity.
This does not redefine historical T1 acceptance.

## Risks / Trade-offs

- CPU skinning and four shadowed sources can be expensive → retain per-scope
  timings before considering a different deformation implementation.
- A technically valid pose can look wrong → compare sampled source/bind data
  and inspect clips visually; record captures and observed limitations.
- Stand-and-yaw turning can show foot pivoting → P07b records this supported
  provisional turning behavior; no unavailable turn clip is assumed.
- Local sources can disappear on build cleanup → commit prepared derivatives
  and preparation records, and never use cleanup commands on p07-assets.
- Constant materials test a narrower profile than Superhero art → make that
  boundary explicit and avoid claiming body/material compatibility.

## Migration Plan

Add resources and shared helpers without changing v8 documents. Build and
validate the viewer and existing game/editor paths before accepting P07a.
Record evidence in validation.md during implementation, then sync/archive this
change and rebase P07b. Rollback removes this additive viewer/resource path;
existing levels and static importer behavior require no conversion.
