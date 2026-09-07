## Context

See [proposal.md](proposal.md) for motivation and scope. At the planning
baseline, P02/P03/P04 were archived and the level codec wrote v7.
`level_document.hpp` stored two
point lights and one optional switch whose link is an array index and whose
initial flag owns that light's startup value. `FrameRequest` has two enables;
`LightingResources` uploads an immutable 80-byte uniform. The scene shader
uses a 128-byte camera/spotlight/enable push constant and unoccluded Lambert
lighting. Runtime and editor already share material/geometry preparation,
fenced changing door buffers and transactional scene replacement.

The baseline furnished apartment has 41 structural solids, six selected model
placements and one moving door. Wood/wallpaper, opaque furniture and the MASK
phone cord supply representative surfaces without new art. The current
renderer uses FIFO presentation, so a wall-clock frame interval alone cannot
separate lighting cost from presentation waits. Implementation measurements
and the resulting local refinements are recorded below and in `validation.md`.

## Goals / Non-Goals

**Goals:** Keep authored lighting identities separate from runtime values,
make multiple-switch behavior unambiguous, and render bounded shadows through
the existing explicit Vulkan path. Editor preview and game presentation must
use the same geometry, material coverage and light initialization rules.

**Non-Goals:** Authored spot/directional/area lights, flashlight shadows,
per-room ambient volumes, indirect lighting, exposure/HDR/PBR, transparency,
volumetrics, animation, events, checkpoints and session menus. Global ambient
and explicitly unshadowed lights are artistic illumination, not a simulation
of transmission. No dependency on audio rooms, physics raycasts for lighting,
render graph, light registry, ECS, bindless descriptors or new library.

## Decisions

### 1. A small authored point-light profile

Use 0-8 point lights and 0-16 switches. T1 needs six sources: four shadowed key
lights for two furnished rooms, corridor and stair/landing region, and two
short-range unshadowed fills. Eight provides space to exercise additions and
duplication in this same scene; sixteen supports two approaches per light.
These are explicit content bounds, not measured capacity claims or permanent
limits for every later scene. The upper-bound variant enables all eight lights
and all four shadowed sources together.

Each light stores `id`, `position`, `color`, `intensity`, `radius`,
`initially_on`, and `casts_shadows`. IDs use the existing 1-64 character
lowercase ASCII identifier grammar and are unique within the light collection.
Switches use the same grammar in their own collection and store only `id`,
`position`, `yaw_degrees`, and `light_id`. Retain fixed plate bounds and no
collision body. Ambient remains one finite level-wide scalar in [0, 0.20];
0.12 is a starter default, not a fixed gameplay floor. Zero lights/zero ambient
is valid; navigational readability is an acceptance-scene responsibility.

At most four definitions can have `casts_shadows`, counting disabled lights.
This makes every valid switch combination renderable without dynamic shadow
allocation or eviction. Unshadowed lights retain positive finite radii;
shadowed radii are limited to [0.25, 20] metres for the local interior profile.
Source color stays finite/non-negative and intensity finite/positive, with
derived upload/projection validation before GPU use. Runtime movement of
authored lights is outside P10.

Alternatives: keep indexed links (fragile under deletion); store initial state
on switches (conflicts when two switches share a light); unlimited lights
(no demonstrated scene need); room-tag light filtering (cannot represent the
shadow cast by a partially open door).

### 2. One state per light, concrete switch references

Extend the existing concrete lighting controller to own one enable value per
loaded light, initialized from the light definition. Resolve switch IDs/links
against that immutable collection. Each eligible E edge toggles exactly the
selected switch's target value; two switches controlling the same light read
and modify the same value. No switch-owned Boolean and no multi-light groups.
Do not add event or checkpoint entry points until P05/P09 have actual callers.

Keep nearest-ray targeting, obstruction, inside-origin rejection, 2 m reach,
release latches and R/E/knock priority. Compute the true nearest distance first,
then select within its 0.1 mm tie interval by type (door before switch), then
lexicographic durable ID. Do not use a pairwise approximate comparator whose
result depends on iteration order. Switches do not become physical blockers;
the nearest switch still consumes an unsupported action.

Cursor release/minimize retains enables and consumes inactive edges without
delayed activation. There is no autonomous P10 timer to suspend. Presentation
outcomes never reset state; a fresh run reinitializes it. The frame supplies
an exact-length borrowed enable span in immutable light order, consumed
synchronously. A mismatch is a boundary error before submission. It carries
no switch IDs, controller types or Vulkan handles. Dense runtime order is an
upload detail, never a persistent identity.

### 3. Direct raster shadow passes on rendered geometry

For each enabled shadowed point light render six 90-degree depth views into
six layers of one bounded 2D depth array, followed by the ordinary forward
color pass. Allocate six layers per configured shadowed source, with one
cleared dummy layer if there are none. Start with 512x512 texels per face,
near plane 0.01 m, far plane equal to the light radius and Vulkan [0,1]
projection depth. A four-light, 32-bit allocation is 24 MiB per frame slot,
48 MiB for the existing two slots, excluding alignment and other resources.
Check actual allocation sizes and device limits; do not treat that arithmetic
as measured memory consumption.

Use the existing static material batches plus the same accepted changing boxes
used by the color pass. Shadow rendering uses visible geometry, never prop
collision boxes. OPAQUE ignores texture alpha; MASK samples the existing
material alpha/factor/cutoff before writing depth. Decorative objects with no
collision can cast shadows; holes in the phone cord stay holes. Rasterize both
sides for shadow depth so supported thin surfaces remain occluders. Player
collision geometry, editor markers and UI do not cast shadows. P07 later adds
animated render geometry to this concrete input.

Select the face/layer by the dominant light-to-fragment axis and compare the
receiver's depth in that face projection. Use a small fixed manual PCF kernel
with nearest depth samples; reproject taps crossing a face edge into the
neighbor face instead of clamping them into a bright seam. Explicit nearest
sampling avoids assuming linear depth filtering support. Start with a small
constant/slope depth bias with zero bias clamp; tune against wall contact,
slanted surfaces and moving-door captures, not by hiding leaks with ambient.
Skip disabled and out-of-radius contributions before shadow samples.

The supported blocking exercise places emitters at least 5 cm from occluders,
uses ordinary opaque walls/slabs and door leaves at least 5 cm thick, and stays
inside the declared radius. Sub-centimetre contacts and pixel-perfect fine
cutout shadows at long distance are not guaranteed. Record aliasing, contact
offset and face-seam observations; a bright patch through the body of the
control wall/closed leaf is a defect, not an accepted resolution limitation.
Fully blocked key-light regions match the same source disabled away from the
shadow boundary; ambient/other lights are isolated for that comparison.
For identical-view source-on/off readbacks, use fixed interior receiver patches
excluding edges: mean absolute stored RGB error <=2/255 and maximum channel
error <=4/255. Retain patch coordinates and captures with the measurement.

Render the bounded shadowed set afresh for each submitted frame initially.
Reusing immutable geometry is already available; shadow caching, visibility
culling, atlases and scheduling add invalidation complexity before there is
evidence of a bottleneck. If measured T1 cost misses its gate, profile first
and review a concrete optimization or quality change; do not silently reduce
the declared shadow count or disable wall blocking.

Alternatives: baked lightmaps do not cover independently switched lights and
accepted moving doors without another dynamic path; screen-space shadows miss
off-screen blockers; ray tracing changes the device baseline; geometry-shader
layered rendering adds an unnecessary feature dependency. Six ordinary views
fit the existing Vulkan 1.3 dynamic-rendering model.

### 4. Explicit resource and synchronization ownership

Keep immutable light definitions and material resources scene-owned. Each
frame slot owns its mutable light upload, shadow matrices/depth images and
descriptors, alongside its changing-geometry buffer. Wait its fence before
writing uploads or reusing images. Move point enables out of the camera/spot
push constant into the bounded per-frame uniform; keep the existing 128-byte
push-constant ceiling. A single array sampler uses a layer coordinate, avoiding
an array of dynamically indexed sampler descriptors. Dummy resources are
valid even when no light uses shadows.

Select a depth format only after checking attachment and sampled-image
features and image limits; prefer D32_SFLOAT with a supported D16_UNORM
alternative. Manual comparisons require no hardware comparison-filter
feature. Fail clearly if the required profile cannot be created; do not
silently substitute unshadowed light. These checks follow the
[Khronos depth guidance](https://docs.vulkan.org/guide/latest/depth.html) and
[format feature reference](https://docs.vulkan.org/refpages/latest/refpages/source/VkFormatFeatureFlagBits.html).

End depth rendering, then transition written layers from depth attachment to
shader-read use before fragment sampling, including both early and late depth
test writes in Synchronization 2 dependencies. Transition sampled layers back
before their next write. Submit shadow/color/caption work on the existing
graphics queue; no parallel queues or queue-family transfers. Use
[Khronos synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html)
as the API reference, and validate the actual resource path in smoke tests.

Shadow extent is independent of the swapchain. Resize/recovery retains valid
scene and frame-slot shadow resources, rebuilding only incompatible pipelines
or swapchain attachments. RAII partial-construction cleanup and destruction
order cover views, images, allocations, descriptors, pipelines and device.
Missing new packaged shadow shaders report their resolved paths.

### 5. Editor definitions are the preview source of truth

Replace reserved singleton selection handles with the existing transient
object-handle mechanism for each light/switch. Add/edit/duplicate/delete and
placement use the shared 128-entry history. Valid light rename updates all
incoming switch references in the same command. Duplicates get fresh IDs,
copy outgoing links and leave incoming links on the original. Deletion never
cascades: broken references stay selectable and block Save/Play until repaired
or undone. A switch can be added to a lightless document with an empty,
diagnosable link. Individual malformed fields are refused; safe broken links
or a fifth requested shadow caster can remain visible for repair.

New lights default enabled/unshadowed; duplicates retain the original shadow
flag, even if this creates repairable budget invalidity. New Interior keeps
two editable enabled/unshadowed starter lights, ambient 0.12 and no switches.
Expose ID, light fields, initial enable, casts-shadows, ambient, switch link,
shadow count/budget and selected light range/link overlays. Shader tuning
parameters remain renderer implementation details, not content fields.

Preview uses each light's initial value and the doors' authored initial poses.
Relinking/deleting a switch cannot change any light's authored initial value.
The editor uses the same shadow implementation without gameplay simulation.
Replacing geometry/light/shadow resources installs a coherent candidate only
after dependent GPU work completes; failure retains the previous preview with
a stale diagnostic. Unsafe shadow inputs cannot be submitted. Terrain-only
edits preserve other resources, but subsequent shadow rendering uses the
updated terrain. Undo, document replacement and presentation recovery must not
combine shadows from one document revision with color geometry from another.

### 6. T1 visual and performance gate

Add `interior-lighting.level.json` as a neutral packaged acceptance scene using
the existing apartment geometry/material catalog, six lights, at least six
switches (two sharing one source), two operable doors and named inspection
starts. Keep earlier acceptance files visually unchanged through migration.
Add a second test setup with eight lights/four shadow casters all enabled.
This setup exercises capacity in the same interior; it does not promise
60 FPS for every document at all unrelated geometry limits.

Record room-to-room wall blocking, a door fully closed/open/player-obstructed,
off-screen blockers, face seams, furniture/MASK coverage and flashlight
independence. Match editor/runtime initial-state camera captures. Walk the
intended route with the flashlight off to judge readable darkness. Test all
lights off, zero ambient, relinking/renaming/deleting and undo, fresh startup,
resize/minimize/recovery, and failed preview replacement.

The planning target is this Windows machine's AMD Radeon(TM) Graphics,
1920x1080 actual framebuffer, 60 FPS. Planning discovery reported Windows
driver 31.0.21921.1000; record actual GPU device/vendor IDs, CPU, RAM, driver,
power mode and presentation settings in implementation measurements. This
local baseline is not a claim about a minimum supported GPU product.

Use a release build with validation disabled for timing; run separate debug
Vulkan validation. After 10 seconds warm-up, collect three 60-second samples
for stationary views and a reproducible route including moving doors, switches
and flashlight, for both six-light and capacity setups. Record p50/p95/p99 for
CPU active-frame work (separate fence/acquire/present waits), GPU shadow work,
GPU whole frame, and end-to-end frame intervals. Capture an unshadowed baseline
on the same geometry/view. Read GPU timestamps only after the owning fence,
respect timestampPeriod and valid bits, and report unavailable measurements.
This is bounded development instrumentation, not a profiler framework.

Gate: p95 CPU active-frame and whole-frame GPU time each <=16.67 ms; end-to-end
median <=16.9 ms, p95 <=20 ms and p99 <=33.4 ms in each measured run on a
60 Hz presentation setup. Record outliers and worst-case door/toggle costs,
not just averages. GPU timestamps or equivalent external GPU measurements
are required to claim the GPU gate passed; FIFO-paced CPU intervals are not
a substitute. Failure triggers investigation and an explicit revised design
if needed. Unavailable visual/timing checks remain recorded as unavailable;
automated tests alone cannot close T1.

### 7. Measurement-driven implementation refinements

Initial fixed-view readbacks exposed self-shadow striping when every PCF tap
used the central receiver depth. Compare each tap against the receiver plane
at its actual nearest texel center. Keep the nine-tap kernel and adjacent-face
reprojection; apply a 2 mm constant plus up to 2 mm slope-dependent world-space
contact bias. D16 readbacks additionally require half a UNORM depth unit of
comparison tolerance for stored-depth rounding; D32 uses zero extra tolerance.
This does not promise pixel-perfect fine contacts at the maximum 20 m radius.

The corrected shader's first full route missed the GPU gate (p95 18.42028 ms),
while shadow rendering itself cost 1.80592 ms at p95. The implementation now
computes receiver-plane slope once per source and uses direct texel offsets
for taps inside the same face; only boundary-crossing taps reproject. The
repeated route measured GPU p95 12.17112 ms with every original gate passing.
This local arithmetic change preserves the profile and comparisons. No cache,
culling system, extra queue or reduction in enabled lights/resolution is added.
The full final repetitions and retained failed runs are in `validation.md`.

Run desktop measurements in the ordinary interactive Windows environment.
Agent-isolated wrapper runs produced long acquisition waits that reproduced
neither direct executable launches nor the same wrapper outside that isolated
environment. Retain these diagnostic failures separately and record the launch
condition; do not relax a percentile gate or infer GPU cost from those waits.

## Risks / Trade-offs

- Twenty-four depth views can be costly on the integrated GPU -> establish
  baseline and per-pass timings before adding culling/caching or changing quality.
- Finite shadow resolution/bias can cause acne, detached contacts or seams ->
  isolate each key source and keep regression captures at doors, corners and
  cube-face boundaries; treat visible control-wall leakage as a failure.
- Ambient, fills and the existing spotlight can illuminate across walls ->
  label the supported profile and isolate sources during acceptance. Flashlight
  shadows require their own explicit scope; do not imply they are implemented.
- v8 changes links and initial-state ownership -> preserve exact legacy
  mappings with fixtures, including initially-off switches and P04 audio.
- Editor allocation failure can leave a stale image -> retain coherent old
  resources and an explicit diagnostic until a replacement succeeds.

## Migration Plan

1. Extend the strict codec to v8: retain `environment_light.point_lights` and
   `ambient_intensity`; add the light fields above and replace `light_switch`
   with required `light_switches`. Reject old fields in v8 and v8 fields in
   older shapes. Empty arrays are explicit; no missing-field defaults.
2. Decode exact v2-v7 through their existing shape checks first. Preserve the
   existing geometry/entry/material/prop/door/audio mappings. Assign old slots
   IDs `point-light-0` and `point-light-1`, both unshadowed and initially on;
   if an old switch exists, create `light-switch-0`, reference the matching
   light ID, and move its initial flag onto that light. No switch becomes an
   empty array. Preserve array order and ambient, including legacy values.
3. Open migrated documents clean with a conversion notice, never writing the
   source. Explicit Save/Save As emits deterministic v8. Older executables
   cannot read v8; retain originals/Save As for rollback. No v8-to-v7 export.
4. Convert packaged levels and their preparation scripts during implementation;
   retain legacy fixtures, add a v7 fixture, and keep historical P01/P04
   content/state behavior. Update current architecture/rendering/gameplay/
   development docs; do not rewrite archived acceptance records.
5. Build, validate shaders/tests/smoke and record T1 visual/performance evidence.
   Update roadmap planning status without claiming later milestones. After
   T1 acceptance, synchronize/archive P10; P07 rebases on its resulting specs.
