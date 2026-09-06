## Context

See proposal.md for the content need. The current renderer privately loads one
GLB into a world-space triangle stream and assigns the obstacle layer from a
fixed three-texture array. Physics receives one separately authored box.
Editor document edits replace scene resources transactionally. These ownership
boundaries remain useful; the singleton content and appearance assumptions do
not.

This design was grounded in read-only inspection of the user-supplied
`house_interior_pack` on 2026-09-06. The selected source GLBs all report Blender
glTF exporter 4.2.70, one scene, one mesh-bearing root, one triangle primitive,
float positions/normals/UV0, unsigned 16-bit indices, no skins/animation or
extensions, and embedded 128-by-128 base-color and roughness PNGs:

| Source under house_interior_pack | Vertices / indices | Material evidence |
| --- | --- | --- |
| props/chair.glb | 330 / 540 | OPAQUE, metallic factor 0 |
| props/table.glb | 248 / 372 | OPAQUE, metallic factor 0 |
| props/phone.glb | 164 / 276 | BLEND, alpha used by the cord ribbon |
| props/radio.glb | 361 / 648 | OPAQUE, albedo contains alpha ignored by that mode |

All four select nearest magnification and nearest sampling of the nearest mip
(`9728` / `9984`). Phone UV sampling finds alpha below 0.5 on its last twenty
triangles, which form the cord ribbon. Its alpha is not just unused atlas
padding. Converting the whole phone to opaque would expose that ribbon.
`geometry/floorWood.glb` and `geometry/wallWallpaper.glb` each contain usable
128-by-128 base-color maps; their source plane geometry is not needed.
The inspected fridge has a hierarchy, emission, and BLEND; it is intentionally
outside this first selected subset. This is not evidence for a general glTF
scene loader.

P03 is being planned in parallel. Its chosen integration baseline is level
version 5 with required `doors`, retaining the singleton prop. P02 writes
version 6 after P03. Shared replacement delta blocks must include P03's full
contracts, and must be checked again against the final P03 before apply.

## Goals / Non-Goals

**Goals:** A small repeatable route from these source assets to packaged game
assets; repeated static placements and editable proxies; scene-selected
materials; faithful base colors, pixel sampling and phone cutout; strict
migration and transactional editor recovery.

**Non-Goals:** Arbitrary pack discovery, authoring catalog entries in the
editor, runtime asset reload, nested scene editing, animated or movable model
placements, PBR/roughness/emission shading, blended transparency, texture paint,
per-face material editing, or replacing P03's generated door presentation.
The phone and radio are static visual props here; their interaction and sound
belong to later changes.

## Decisions

### 1. Stage a selected, controlled game profile

Keep `house_interior_pack` untouched. During implementation, prepare only chair,
table, phone, and radio under packaged model paths, retaining their mesh/UVs and
base-color pixels. Remove unused exporter metadata, roughness resources and
unsupported material inputs deliberately in this offline step. Set the phone
derivative to `MASK` with cutoff 0.5; preserve alpha and nearest sampling.
Other selected models remain `OPAQUE`, whose alpha does not create holes.
Record source paths, source hashes, conversion steps, and provenance alongside
the selected packaged assets. The build copies selected game resources, never
the raw pack. Conversion is a reproducible content preparation operation, not
another runtime importer mode.

The runtime profile remains one default scene, one root, one mesh and one
triangle primitive per selected GLB, embedded geometry and at most one embedded
PNG base-color image. It retains finite accessor/index/transform validation and
rejects unsupported geometry, required extensions, outside file references,
textures or unsupported shading inputs rather than silently ignoring them.
Prepared materials explicitly set metallic factor to 0, use roughness factor 1
with no roughness texture, and have zero emission with no emission, normal or
occlusion textures. Double-sided rendering is outside this selected profile.
An absent material is the explicit constant-white case; the material-free
legacy chair instead uses its catalog override. These are controlled diffuse
game derivatives, not support for general glTF PBR defaults.
Absent base-color texture uses white; base-color factor defaults to white and
is validated in [0,1]. OPAQUE and MASK are the only alpha modes; `alphaCutoff`
is finite in [0,1]. Texture coordinate set is UV0; no texture transform or
vertex-color interpretation. The accepted sampler choices are repeat wrapping
with nearest/nearest-mip or linear/trilinear filtering. An omitted sampler or
omitted filters select repeat/linear/trilinear in this game profile; explicit
unsupported combinations are rejected. Legacy prototype textures keep their
linear behavior.

Set explicit defensive import bounds before allocating: GLB at most 16 MiB,
one image at most 2048 by 2048, and at most 300,000 expanded vertices per asset.
The selected files are below 51 KiB and 648 indices, so these bounds allow
ordinary edits without broad asset infrastructure. Validate byte ranges and
overflow before decoding, and check aggregate repeated-placement vertex bytes,
image bytes and draw ranges before allocation/upload. The legacy chair retains
its current shape through a
catalog material override selecting the legacy obstacle material; the override
is explicit game catalog content, not a fallback for unsupported assets.

Alternative considered: support all source metadata or force every raw export
directly through runtime. That would introduce hierarchy, blending and PBR
without improving the selected scene. Stripping phone alpha was rejected on
the measured UV evidence. A new geometric cord is unnecessary for this scope.

### 2. Use a small explicit catalog and stable document identities

Maintain a project-owned finite model/material catalog, using stable logical
strings in JSON and typed identities internally. Initial model IDs are
`prototype-chair`, `apartment-chair`, `apartment-table`, `apartment-phone`, and
`apartment-radio`. Initial structural material IDs are `prototype-floor`,
`prototype-boundary`, `prototype-obstacle`, `wood-floor`, and `wallpaper`.
Model materials belong to their packaged model definition; placements do not
offer arbitrary material overrides. Logical names are not file paths, and
paths are resolved only by the resource/composition side under the explicit
package root. No manifest discovery, directory scan, plugin, or global service
is needed. Adding another real asset is an explicit catalog/content edit.

Version 6 replaces `static_prop` with ordered `props`, each containing `id`,
`model`, `translation`, `yaw_degrees`, `uniform_scale`, and ordered
`collision_boxes`. Each box contains model-local `center` and `half_extent`.
There are 0 through 128 placements and 0 through 8 boxes per placement. A prop
ID follows the existing entry lexical form and is unique in the prop namespace;
it is independent of entry and door IDs and of vector/selection positions.
There are no authored references to prop IDs yet, so deletion removes only
that placement. Model and material catalog deletion is not an editor operation;
unknown references remain visible as diagnostics, never remapped to a default.

Each solid replaces `surface` with `material`, independent of its existing
collision kind. One material covers all of a solid's faces, retaining its tint
and world-scaled UVs. Present terrain adds one material ID, defaulted only when
migrating older shapes; its whole surface uses that material. The terrain brush
does not edit it. Structural choices are OPAQUE tileable materials only, so a
visible floor or wall cannot acquire cutout holes while remaining collidable.
Extract wood-floor and wallpaper base colors for these materials and retain
their source nearest sampling. P03 doors and the switch retain their generated
appearance through the explicit `prototype-obstacle` material alias.

Alternative considered: put paths, arbitrary material definitions or a catalog
editor in each level. Stable references to the small packaged set meet current
authoring needs with less validation and ownership machinery.

### 3. Keep collision authored and static

The level owns collision boxes independently of GLB geometry. For each box,
apply positive placement scale and yaw to its local center, and scale its half
extents; physics creates a static oriented box. Default boxes are concrete
catalog authoring defaults, copied into a new placement and then independently
editable. Chair/table use useful body/leg proxies; small tabletop phone/radio
may intentionally have no collision. No box is an entry-support surface.

All boxes participate in player collision, entry standing-clearance checks,
door obstruction checks and interaction visibility queries. Empty proxy lists
mean decorative geometry is non-blocking, including for targeting. A proxy's
coverage is deliberate simplified gameplay collision, not alpha/triangle
collision; the editor shows its bounds for inspection. Moving a placement
updates all its proxies coherently. Changing its model preserves the placement
ID, transform and existing proxies; an explicit reset-to-model-defaults action
can replace proxies in one undoable edit.

P03 retains its independent generated door, collision body and run-local state.
An imported model's transforms or GLB child names never define a door. This
avoids coupling the two parallel work streams.

### 4. Extend immutable static material rendering, retain moving doors

Decode each referenced static model once per scene load and share its immutable
local geometry/material description while building repeated world placements.
Continue baking static transforms into world-space draw vertices; repeated
placement vertex duplication is acceptable at the documented bound. Batch
static world/prop ranges by material where simple. Do not add dynamic static-
prop transforms or GPU instancing merely because several chairs share a model.

Use explicit immutable per-material sampled-image descriptors and a small
renderer-private material uniform for base-color factor, alpha mode and cutoff.
Use white texture fallback for constant-color assets. Keep the existing
authored-light descriptor and current frame lighting/camera push-constant
layout. The current push constant is already 128 bytes: material data must not
increase the device baseline or consume P03's pose representation. P03's
per-frame door stream continues to bind its fixed opaque material and follows
its own frame-slot synchronization.

Base-color RGB is sampled as sRGB and converted for existing diffuse lighting;
alpha remains linear. For MASK, evaluate sampled alpha times base-color-factor
alpha against cutoff before color/depth writes. Surviving fragments remain
opaque. Generate full color/alpha mip chains; nearest source assets choose the
nearest texel in the nearest mip, while legacy tiled surfaces retain filtered
sampling. The same alpha comparison is used at every mip and no alpha-to-
coverage, coverage-rescaling or blend pass is added. Visual acceptance includes
cord appearance at ordinary inspection distances and while receding; expected
distant cutout thinning is a known limit of this bounded profile.

Each referenced model/material/image is resolved from the selected scene only.
No per-frame parsing, filesystem access, mutable descriptor updates or resource
rebuilds follow light changes. Partial startup and replacement failure release
only newly constructed owners; static resources survive swapchain recovery.
The editor follows its existing fence-before-replacement transaction, sharing
the runtime importer and material behavior. Terrain rebuilds retain unrelated
props, materials and P03 door preview. Empty world/prop streams produce no
empty allocations or draws.

Alternative considered: extending the fixed texture array. Different texture
sizes, samplers and MASK parameters already occur in the selected content;
ordinary per-material bindings are clearer than normalizing every resource to
one array or introducing bindless indexing. This decision does not require a
render graph or new rendering API boundary.

### 5. Integrate editor operations without asset authoring machinery

The flat Objects set gains independent prop entries with stable IDs. Add Prop
chooses a catalog model and copies its default proxies. Duplicate uses a new
`prop-N` identifier and deterministic offset, retaining the shared model ID;
remove affects only the selected placement. Properties expose ID, catalog
model, transform, box list and explicit reset of proxies. Solids expose the
structural material choice separately from kind; terrain's single material is
a property separate from the height brush. Doors keep all P03 operations.

Viewport picking uses transformed visible model bounds rather than requiring a
collision proxy, so decorative objects remain selectable. Selection bounds and
optional proxy overlays are clearly distinguishable. Direct placement still
uses the nearest structural or terrain surface; props place their translation
anchor and do not become placement surfaces. Telephone/radio table placement
can use explicit numeric translation and the table's known height in this
milestone, without adding render-mesh surface queries.

Unknown logical references are safe editable validation failures: preserve the
authored ID, show a selectable marker/proxy bounds, and block saving/play until
corrected. Missing/corrupt packaged data or failed GPU replacement retains the
last usable preview with an explicit stale-preview diagnostic. A newly opened
safe document may remain editable even if its assets cannot render; the old
preview must never be presented as matching it. Successful save requires valid
level/reference data; saved-file Play additionally preflights required assets
before process creation. Other model files and the raw pack are irrelevant.

## Risks / Trade-offs

- Cutout mips can thin the phone cord at distance -> inspect staged images and
  camera distances in game/editor; keep the documented nearest/mip behavior and
  avoid claiming full blended-source equivalence.
- A simple box can block visible gaps -> use a few explicit boxes, show them in
  the editor and exercise chair/table passage and door swing by walking.
- Parallel P03/P02 plans touch the same requirement blocks -> integrate P03
  first and recompose all overlapping deltas/code against its final result;
  separate checkouts alone do not resolve semantic conflicts.
- Extra bindings and baked repeated vertices increase memory/draw work -> use
  bounded placements and simple shared decode/material resources; measure the
  representative scene before considering more rendering architecture.
- Source-pack provenance is not established by its folder name -> carry the
  supplied source's provenance with selected derivatives during staging and
  do not package or redistribute the entire raw source pack.

## Migration Plan

1. Re-read final P03 contracts and confirm its version-5 baseline, door fields,
   initial-state validation, editor history and dynamic render lifetime.
2. Add strict version-6 decoding/writing and exact version-2/3/4/5 readers.
   Normalize the legacy chair to `props[0]` with ID `prototype-chair`, the
   legacy model and unchanged transform/one box; map old surface roles to their
   explicit legacy material IDs. Older terrain becomes `prototype-floor`.
   Preserve v5 doors; v2/3/4 normalize to no doors. Preserve existing spawn/
   entry/switch migrations, including v2 no switch and v2/3 `default` entry.
3. Explicit saves alone write version 6 and retain deterministic list ordering,
   numeric format and newline behavior. Version 6 rejects old singleton/surface
   keys; each older version rejects later fields. Unsupported v1 stays rejected.
4. Stage selected assets and migrate packaged acceptance files while preserving
   P03's Lena door and doorway exercise. Furnish the existing route without
   blocking its walking path or entries. New Interior starts with no props and
   no doors, a floor, default entry and the two existing lights.
5. Run migration, import, proxy, editor, combined door/material and Vulkan
   acceptance checks; update affected development/architecture/rendering docs
   and stale main-spec Purpose text when syncing the change.

Older binaries cannot read version 6. Rollback uses the prior binary/resource
bundle and retained original v2-5 level files or version-control copies; never
attempt lossy automatic conversion of multiple props back into one chair.
