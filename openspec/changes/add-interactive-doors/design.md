## Context

See [proposal.md](proposal.md) for the playable purpose and acceptance scope.
The current version-4 document owns one chair and an optional light switch.
`LightSwitchController` owns both the E release latch and switch policy.
`Engine::tick` advances the player in 1/60-second steps, interpolates the eye,
and evaluates interaction once per event batch. `PhysicsWorld` owns static
boxes/terrain and one Jolt virtual character; its visibility filter explicitly
excludes moving bodies. The renderer draws immutable world-space geometry,
and its existing camera/spotlight push block already occupies 128 bytes.

P03 integrates first, producing version 5. The separately planned P02 follows
with version 6 and composes its asset deltas over P03. Neither generated door
geometry nor door collision requires a model import or asset registry.

## Goals / Non-Goals

**Goals:** keep one authoritative accepted leaf pose; make obstructed motion
safe and repeatable; reuse input edges and targeting for the two actual
callers; retain editor repairability, immutable authored data, and explicit
GPU/body lifetime. Keep shared P02 integration small and reviewable.

**Non-Goals:** physical swinging from impulses, player pushing/carrying,
automatic opening direction chosen from the player's side, arbitrary hinges,
hand animation, permanent door art, inventory keys, lock picking, saving an
in-progress game, audio/text delivery, or a generic interaction framework.

## Decisions

### 1. Concrete version-5 definitions

Add a `doors` array to the exact version-4 shape and set `version` to 5.
Keep all other version-4 fields unchanged. Each door has these exact JSON
fields; all are required, and additional fields are rejected:

| Field | Meaning and validation |
| --- | --- |
| `id` | Unique among doors; `[a-z][a-z0-9-]{0,63}`. Entry IDs are a separate existing namespace. |
| `hinge_position` | Existing `{x,y,z}` world-position shape; bottom hinge anchor, finite derived bounds. |
| `closed_yaw_degrees` | Finite yaw using the existing right-handed world Y rotation convention. |
| `width` | Leaf X size, 0.4 through 2.5 metres. |
| `height` | Leaf Y size, 1.0 through 3.5 metres. |
| `thickness` | Leaf Z size, 0.04 through 0.30 metres. |
| `open_angle_degrees` | Signed angle relative to closed yaw; magnitude 15 through 170 degrees. |
| `speed_degrees_per_second` | Magnitude 15 through 180; opening and closing share it. |
| `lock_side` | `none`, `positive-z`, or `negative-z` in the closed local frame. |
| `initially_open` | Boolean selecting closed or authored open endpoint. |
| `initially_locked` | Boolean; true requires closed initial state and a lock side. |

The 32-door bound and dimension/motion limits suit the apartment and bound
geometry and sweep work. They are this controlled authoring profile, not an
attempt to represent every possible door. Defaults for editor creation are
0.9 by 2.0 by 0.06 metres, +90-degree opening, 90 degrees/second, positive-Z
lock side, initially closed and unlocked. Dimensions/angles can be changed
within the profile without changing the runtime architecture.

In local coordinates the slab spans X=[0,width], Y=[0,height],
Z=[-thickness/2,+thickness/2]. Rotate about Y by closed yaw plus current
opening angle, then translate by the hinge. Use the same scalar world helpers
for corners, center, leaf bounds, editor preview, and targeting. The generated
knob/bolt detail stays within the leaf's X/Y footprint and is non-colliding.
Doors are not structural solids and cannot support authored entries.

Shared validation checks all initial leaves against structure, optional
terrain triangles, the chair proxy, other initial leaves, and every standing
entry. Touching within documented numerical tolerance is not penetration;
the packaged example and surface-placement default leave 2 cm floor clearance
and a small clear frame gap. A clear initial pose with an obstructed later
swing is valid: runtime obstruction policy handles it. Use finite bounded
geometry checks in world/editor without importing Jolt or render assets.

This keeps the door record separate from P02's static placement record.
Reusing a generic transform/component/asset object would add indirection
without serving the two different ownership lifetimes.

### 2. State and controls are explicit

A concrete runtime door owner retains current angle, last requested direction,
moving/stopped status, locked bit, and bounded feedback state per definition.
It starts from authored endpoints. Durable IDs are used in concrete results;
small array indices are only runtime lookup conveniences. There is no entity
registry, callback registration, or serialized runtime-state machinery.

These are proposed interaction defaults for review, not claims about final
game controls:

| Input | Concrete behavior |
| --- | --- |
| E | Open from closed, close from open; while moving or stopped mid-swing, reverse the last requested direction. Locked opening is refused. |
| R | Toggle lock only while fully closed and stopped, from the authored bolt side. |
| Right mouse | Knock once on the selected door without changing movement or lock. |
| Left mouse | Existing flashlight/cursor recapture behavior. |

An E reversal changes intent without snapping geometry. At an endpoint, clamp
the angle exactly and stop. Endpoint behavior takes precedence: E always
requests the opposite endpoint, including retrying an opening blocked before
its first increment. Only a pose strictly between endpoints reverses the last
direction. An obstructed request stops until a new E edge; clearing the blocker
does not retry. Lock-side classification uses
the eye's signed local Z relative to the closed leaf plane; a point within
1 mm of that plane is not a valid lock-side position. R on an open, moving,
wrong-side, or un-lockable door gives refusal and changes nothing.

Results are a small value type containing door ID and concrete result kind.
Current feedback keeps the affected side with its run-local state, rather than
carrying an unused world position in each result. Request accepted, reached open,
reached closed, obstructed, locked, unlocked, refused, and knocked are enough.
They are consumed immediately by the feedback owner in the current update;
multiple fixed steps drain their results individually. No history or deferred
queue is required. P04/P05 can later consume the same concrete results and
authoritative state when their actual callers exist.

For this milestone, a generated bolt changes visible position between locked
and unlocked, a refused handle gives a short contrasting handle movement,
and a knock gives a visibly different pulse on a plate on the struck side.
Use a fixed 0.3-second feedback window advanced with simulation, with the
latest accepted result replacing an older transient on that door. Geometry
feedback does not move the leaf or collision. This temporary feedback allows
muted/manual verification before P04, without adding a HUD, text renderer, or
player hands. It is not a final art requirement.

### 3. Shared target arbitration and release latches

Move the sampled release/press policy out of the switch-only owner into the
concrete interaction coordinator. Keep light enable state in its existing
gameplay owner. Add the physical R key and project-owned lock action; existing
secondary-action input supplies knock. Each of the three actions has its own
initially disarmed release latch. Sample/consume every batch, including waited
and inactive batches. If several new edges coincide, consume all and choose
R, then E, then right mouse. Do not fall back when that action is unsupported
or refused. The flashlight remains independent.

For the displayed eye ray, calculate switch-plate and accepted door-leaf
intersection distances in world-owned geometry helpers, reject inside origins
and distances beyond 2 metres, and find the absolute nearest distance. Select
the lowest stable key among candidates within 0.1 mm of that minimum: doors
ordered by durable ID, followed by the switch. Do not use an epsilon ordering
comparator, because it is not transitive. A locked/ineligible nearest object
does not permit selecting through it.

Replace the public static-only visibility operation with a bounded world
segment query that includes doors and accepts at most the selected door ID
as its self-surface exception. Resolve that ID to the physics-owned body
internally. Check origin-inside conditions before excluding that body's front
surface. Keep all other bodies at or before the hit as blockers, and retain
the existing 0.1 mm endpoint tolerance and no-player-self-hit policy. Tests
must prove both a door targeting itself and that door blocking a switch.

### 4. Accepted motion is checked over the whole arc

Keep Jolt ownership within physics. Allocate one zero-velocity kinematic box
per leaf with zero convex radius to match the complete rectangular slab and
track body cleanup alongside existing static cleanup. Gameplay asks physics
for a clear angular advance; physics returns the accepted angle and whether
an obstruction stopped it. Gameplay owns direction and lock policy.

The fixed-step order is explicit:

```text
input samples -> fixed-step batch
                  |
                  v
 player step against installed leaves (including stance clearance)
                  |
                  v
 retain previous/current player capsule presentation envelope
                  |
                  v
 doors in durable-ID order: sweep -> accept clear angle -> install body
                  |
                  v
 consume motion results and advance bounded feedback
                  |
                  v
 interpolated player view + current accepted leaves
                  |
                  v
 one action dispatch -> geometric frame request
```

The installed leaf remains stationary during the character step. Applying a
verified kinematic transform after that step uses zero linear/angular body
velocity, so Jolt does not push the player or independently integrate another
door pose on the next physics update. Requests sampled after simulation start
moving only in a later fixed step. An action in a zero-step batch is consumed
once without inventing a partial physics update.

Endpoint overlap tests alone are insufficient for rotating thin geometry.
Use conservative angular coverage with subintervals no larger than 1 degree:

1. Express the two endpoint sets of slab corners in the subinterval's midpoint
   orientation and form their enclosing box.
2. Expand its horizontal X/Z bounds by the maximum hinge radius times
   `1 - cos(abs(deltaAngle)/2)` plus the small horizontal numeric margin.
   The sagitta bound contains every corner arc and therefore the entire leaf
   throughout the subinterval. Y does not rotate: retain exact vertical
   extents instead of inflating them into the supporting floor.
3. Use a zero-convex-radius query box so rounded corners cannot remove any of
   the proven envelope. Query it against terrain, static boxes/proxy, the other
   currently installed leaves, and the player presentation envelope. Exclude
   only the moving leaf's own body. Other doors use already accepted poses
   earlier in ID order and installed poses for the remainder.
4. If clear, accept the subinterval. If blocked, bisect to recover a clear
   prefix until the remaining tip travel is at most 1 mm; stop at the last
   verified angle. If no progress is verified, retain the previous pose.
   A bound of twelve subdivisions per candidate interval prevents unbounded
   refinement; reaching it stops safely rather than accepting unchecked work.

The player envelope conservatively contains both previous and current
standing/crouched capsule extents and the translation between them, using the
larger stance height. Include the explicitly configured 2 cm character contact
padding: horizontal radius grows by that padding and the height by twice the
padding because Jolt also raises the capsule. Initial door-to-entry validation
uses the same bounds. A simple swept AABB is sufficient initially. This
protects the interpolated eye as well as the current physics capsule. Door
angles are not interpolated: the frame's leaf, targeting leaf, and installed
collision pose are identical. Player interpolation remains unchanged.

This method trades occasional early stops near corners or space the player
has just vacated for a small, provable no-penetration path. There is no automatic
retry of those stops. Test ordinary traversal and refine the conservative
envelope only if it prevents the authored exercise, preserving the safety
guarantee. A free rigid-body hinge would require force/crushing policy and
would not by itself satisfy this requirement; separate render interpolation
would introduce an additional moving blocker pose and visual safety problem.

### 5. Small changing opaque geometry beside immutable streams

Add a bounded backend-neutral opaque-box presentation description (center,
positive half extents, yaw, tint, and existing surface selection). Runtime
resolves accepted door/feedback state into those values before calling the
renderer; frame data carries no door IDs, locks, actions, or Jolt handles.
At most six boxes per door bound the current leaf/handle/bolt/feedback stream
to 192 boxes. Reserve backing storage once, and keep it alive through the
synchronous frame call. The editor uses the same geometric presentation from
authored initial state without constructing gameplay or physics.

Generate world-space vertices/normals for this small stream, using the current
opaque obstacle texture and separate tints. Add one reusable host-visible
vertex-buffer owner per existing frame slot, sized for the bounded stream;
use coherent memory or explicitly flush mapped ranges. Wait for that slot's
fence before writing or replacing it. A zero-door scene needs no allocation
or draw. Keep failed partial allocation and teardown paths explicit.

Draw the stream with the existing opaque depth and lighting pipeline after
the immutable world and chair. No model matrix is added to the already full
128-byte push range. No shader, descriptor, material registry, storage buffer,
bindless path, or render graph is required. Swapchain recovery retains these
independent owners and subsequent frames upload the current presentation.
P02 may change material resources while preserving this generated opaque path.

Editor document replacement remains transactional: prepare replacements,
wait for dependent work, install only on success, and preserve the preceding
usable resources on failure. Terrain-only rebuilding must not drop door
presentation. Safely previewable invalid placements remain visible with
diagnostics; unsafe geometry is omitted without losing the editable record.

### 6. Concrete editor workflow and playable content

Extend the existing object variant, transient selection handles, property
transactions, picking, and 128-entry history for the door record. This earns
no scene hierarchy or generic object framework. Allocate IDs as the first
unused `door-N`; duplicates allocate once and restore that ID through undo and
redo, with the same deterministic horizontal offset policy as solids.
Renaming validates door-local uniqueness and preserves selection. There are
no authored references to rewrite in this milestone.

Add Door creates the documented defaults near the selected/default entry;
the author then places its bottom hinge on an upward structural face or
terrain with visible 0.02 m floor clearance. Wall and underside placement are
unavailable; the nearest unsuitable hit blocks placement through it. Exact
numeric hinge/yaw editing handles real doorway fitting. Preserve unrelated
properties. Selection overlays show hinge axis, arc/open endpoint, and bolt
side; runtime only displays the actual generated geometry.

Preview follows `initially_open` and `initially_locked`, without simulating E,
R, or knock in the editor. Initial open/locked contradictions remain repairable
cross-field validation errors rather than silently changing another field.
Finite initial overlap remains visible and invalid, with save/play refused.

Extend `apartment-stairs.level.json` with the room door, unlocked initially,
bolt on the room side, and a reachable switch behind the closed leaf from a
documented observation position. Preserve both entry starts and the ordinary
walking route after opening. Keep the standalone prototype usable with no
doors or a deliberately separate door test fixture. Capture exact manual
controls/observation positions in DEVELOPMENT as implementation documentation.

## Risks / Trade-offs

- Conservative sweep/envelope stops can occur early or just after the player
  vacates space -> exercise both doorway approaches, diagonals, hinge corners,
  crouch transitions, and moving-player cases; retain no-crush behavior.
- Current leaf poses advance at 60 Hz even on a faster display -> assess the
  temporary door visually before introducing interpolation complexity.
- Geometry-only refusal/knock cues may be subtle -> use visibly different
  transient shapes/tints and record a muted manual check; P04 supplies sound
  and text later without changing authoritative leaf state.
- P02 touches codec, validation, editor, renderer, and common main specs ->
  integrate P03 first; explicitly rebase P02's full modified requirements and
  preserve v5 fixtures, doors, visibility blockers, and generated rendering.
- Sweeps and Jolt contact tolerances can disagree near a jamb -> bound and
  document numeric margins, retain clearance in the authored example, and
  test thin blockers and conservative initial overlap at representative scale.

## Migration Plan

1. Read exact v2/v3/v4 with their existing strict keys and normalize to empty
   doors; v2/v3 retain their existing spawn and switch migrations. Record source
   version and preserve clean editor state on opening.
2. Make explicit saves canonical v5, preserving all former fields and stable
   door order. Existing runtime/editor consumers use immutable v5 definitions.
3. Migrate packaged current levels by explicit authored edits during apply and
   retain independent v2/v3/v4 test fixtures. Opening itself never rewrites.
4. Build/test and perform game/editor Vulkan and doorway acceptance before
   archiving P03. P02 subsequently writes v6 and preserves exact v5 reading.

Rollback uses the prior executable with the prior version-controlled level
files. Old builds cannot read v5; there is no lossy downgrade writer. Planning
completion does not constitute implementation or validation evidence.
