## Context

See [proposal.md](proposal.md) for motivation and the P01 scope in
[the roadmap](../../../../docs/ROADMAP.md). This design targets its M1 apartment
and stairs blockout, before doors, imported scene assets, or narrative state.

The current code has useful boundaries to preserve:

- `src/core/world/level_document.hpp`, `level_codec.cpp`, and
  `prototype_level.cpp` own the bounded document, private JSON codec, shared
  validation, and immutable runtime handoff. Version 3 reads the exact version-2
  shape and writes through a sibling temporary file.
- `src/core/physics/physics_world.cpp` consumes the immutable level, creates a
  mandatory Jolt heightfield, adds the axis-aligned solids and yawed chair
  proxy, and creates one virtual character from the single spawn. Its body
  capacity is 256; keeping the 240-solid limit avoids capacity changes.
- `src/core/render/prototype_scene.cpp` generates solids, terrain, and the
  optional switch. Game and editor use that generator. The editor already
  previews gameplay-invalid object edits independently of runtime validation.
- `src/editor/editor_document.*`, `editor_commands.cpp`, and
  `editor_picking.cpp` provide concrete object commands, transient selection
  IDs, 128-entry history, and terrain-only direct placement. The UI and editor
  do not link game runtime or physics.
- `src/main.cpp` takes no arguments. `runtime_resources.cpp` requires the
  packaged prototype along with the shaders, textures, and chair;
  `Engine` initializes the player from that level's single spawn.

Terrain's role as spawn support and as a horizontal bound for the prop and
switch must be separated from structural interior geometry. Solids already
have independent finite coordinates. Prototype fixture assertions in tests
must remain fixture checks rather than become general level constraints.

## Goals / Non-Goals

**Goals:**

- Keep validation in the world module, including entry support, so editor
  diagnostics and runtime admission agree without constructing editor physics.
- Make terrain absence explicit throughout document, preview, collision, and
  tests, and keep the selected entry independent from authored default state.
- Extend the existing concrete document/command workflow to create interiors,
  author entries, target structural faces, and launch a saved file once.
- Preserve the existing runtime resource and library ownership boundaries.

**Non-Goals:**

- No general scene objects, room graph, stair generator, CSG, transform
  hierarchy, persisted structural IDs, world-boundary object, or workplane
  framework. Axis-aligned boxes and gaps serve this blockout.
- No change to walking, step height, crouch, jump, flashlight, or switch input
  policy. No runtime level editing or save-game state.
- No expansion of the chair/light profile, material import, audio, moving
  doors, rendering techniques, or general process-management service.
- No terrain creation/removal tool within an existing document. New Interior
  starts without terrain; existing terrain documents retain their brushes.

## Decisions

### 1. Version 4 replaces the single spawn with bounded named entries

Use this canonical top-level field order:

| Field | Version-4 value |
| --- | --- |
| `version` | `4` |
| `terrain` | `null` or the existing origin, spacing, and 9,409 height samples |
| `solids` | Existing solid objects in authored order, one through 240 when valid |
| `entries` | One through sixteen objects in authored order |
| `default_entry` | Identifier of one entry |
| `environment_light` | Existing two point lights and ambient data |
| `static_prop` | Existing single chair placement and box proxy |
| `light_switch` | Existing nullable switch definition |

Each entry contains `id`, `foot_position`, and `yaw_degrees` in that order.
Identifiers match `[a-z][a-z0-9-]{0,63}`. Sixteen entries leave room for actual
episode starts in one compact level without requiring a registry; this limit
does not preallocate slots for future narrative objects. The identifier also
serves as the author-facing name; there is no separate display-name field.

Use an optional terrain value and a small vector of project-owned entry
records in world data. Retain the existing immutable level owner; avoid a
repository-wide rename of `Prototype*` types in this change. The old spawn
pose type can serve as the pose within an entry without retaining two authored
sources of truth. Filesystem paths and source-version metadata stay in codec
results/editor state, outside immutable gameplay definitions.

Keep strict version-specific field checks. Version 2 still has no switch;
version 3 still requires its nullable switch. Normalize either single spawn
to `{id: "default", foot_position: ..., yaw_degrees: ...}` and set
`default_entry` to `default`. Preserve all other values and report the source
version separately so the editor can explain the pending migration while an
unchanged opened file remains clean. Version 1 and future versions fail
clearly. No older profile is allowed to accept newer fields accidentally.

Explicit saves emit only v4 using the current deterministic serialization and
temporary-file replacement behavior. Keep stable authored array order rather
than sorting by IDs. Numeric syntax remains locale-independent, with one
trailing newline. Codec decoding may produce a structurally safe but
gameplay-invalid candidate; save and immutable handoff always run validation.

**Alternatives considered:** Retaining `player_spawn` alongside optional
entries would create competing defaults. Removing v2 support would discard
compatibility the current runtime already provides without helping interiors.
An unbounded entity registry or opaque generated UUIDs would add concepts
without improving these explicit launch references.

### 2. Entry height selects support; validation does not move entries

Validate every entry, independently of launch selection. After finite-value
and identifier checks, find support at its authored X/Z and Y:

1. If terrain exists and covers that X/Z, its exact existing triangle-based
   height is a support candidate.
2. Every structurally valid solid's upward top face covering that X/Z is a
   support candidate, regardless of the solid's visual role.
3. Require a candidate height matching the authored foot Y within the existing
   0.1 mm support tolerance. Do not cast from the sky, choose the highest
   floor, or rewrite the foot Y. Reject unsupported positions and positions
   beneath the terrain surface at their X/Z.
4. Check standing clearance against all structural solids, the declared yawed
   prop proxy, and intersecting terrain where present. Touching the supporting
   top face is permitted; penetration beyond numerical tolerance is not.

Keep the world module's conservative upright clearance envelope with the
existing 0.35 m radius and 1.80 m standing height for structural admission.
Account for the chair proxy's actual authored yaw when checking that volume;
do not treat its unrotated half extents as world-axis extents. Terrain clearance
uses its local triangle surface and the standing capsule envelope, permitting
ordinary contact on supported slopes rather than rejecting the whole terrain
bounding box. Use bounded CPU geometry helpers over project-owned scalar
types; do not link Jolt into the world or editor modules. Verify admitted poses
against the real Jolt character in deterministic tests.

There is no new authored support reference: changing or removing a floor
revalidates affected entry geometry naturally. The chair and switch remain
blocker/decorative objects and are not selectable entry-support surfaces.
Remove terrain-footprint admission checks for the chair and switch, and allow
solid-supported entries outside any present terrain footprint. All authored
values and transformed bounds still have to remain finite. Present terrain
retains its sample layout and supported-slope validation everywhere.

Diagnostics carry an entry array path and a readable ID when one is usable.
An invalid unused entry still blocks save and play so a saved level has one
unambiguous validity contract. Selection of a different entry cannot hide an
invalid pose.

**Alternatives considered:** Snapping to terrain or the highest surface loses
the author's chosen floor. Persisting a support-solid ID would introduce
structural identity and broken-link repair for a relationship determinable
from geometry. Editor-owned Jolt validation would break a useful dependency
boundary and duplicate physics lifetime. Replacing all clearance checks with
a new general collision-query framework is unnecessary.

### 3. Keep level choice and entry resolution in runtime composition

Extend the public standard-library-only runtime configuration with an optional
level path and entry identifier. The launcher parses `--level <path>` and
`--entry <id>` before constructing the application, resolves relative paths
once against the caller's working directory, and passes an absolute level
path. Preserve literal native path characters, including Windows Unicode;
obtain/decode native arguments in launcher code rather than relying on a lossy
ANSI conversion. Report usage errors for unknown, repeated, empty, or missing
option values. No positional level alias is added to the game launcher.

Resource resolution always checks the executable-relative shader, texture,
and chair assets. It selects either the explicit file or
`resources/levels/prototype.level.json` and requires only that selected level.
An absent unselected prototype must not break an external-level launch.

`Engine` owns the validated immutable level and resolves a borrowed entry pose
before creating `PhysicsWorld` or `PlayerController`. Pass the pose explicitly
to character construction and its yaw to the initial player view. Initialize
both presentation snapshots from that start to avoid an initial frame from
the default pose. Neither entry lookup nor the selected launch pose changes
the level's default field. Platform/window startup and existing reverse
destruction remain under the current owners.

**Alternatives considered:** Encoding a resource path in the level violates
the current authored-data boundary. Copying every playtest over the packaged
prototype destroys the author-to-play workflow. Mutating a loaded default to
select an entry obscures authored state. A resource manager or runtime level
switching service is not needed for one startup selection.

### 4. Optional terrain extends existing geometry and physics paths

Conditionally create the Jolt heightfield and generated terrain triangles.
Always consume the same immutable solids and chair proxy/placement. Make
partial-construction cleanup and static interaction queries work when the
first installed body is a solid instead of terrain. Keep current body
capacity, main-thread stepping, and private Jolt dependency.

The shared scene generator accepts absent terrain. No shader, descriptor,
lighting-layout, or new draw-category change is needed. Keep the chair stream
separate and immutable. Editor preview uses the same generator with editable
fields. A transient invalid interior can have zero solids and no switch:
represent an absent generated mesh at its renderer owner and omit its draw
instead of creating a zero-byte buffer. The chair, entry markers, diagnostics,
and UI can still render.

Editor replacement between terrain-bearing and terrain-free documents retains
the current build-then-install resource transaction. If a replacement fails,
preserve previous usable GPU resources and present the error. Clear obsolete
terrain overlays and marker data on successful replacement. Terrain strokes
remain coalesced and available only when the document has terrain.

**Alternatives considered:** An invisible fallback terrain would still block
lower floors and hide data errors. A separate interior renderer or collision
world would duplicate the existing box geometry path. A zero-sized immutable
buffer is not a useful representation of absent geometry.

### 5. Extend concrete editor commands and surface hits

File > New Interior creates a small valid starting document: a floor slab
with its top at Y=0, one `default` entry on it, two existing-profile lights,
ambient 0.12, the fixed chair away from entry clearance, and no terrain or
switch. It starts dirty without a path. New uses the same pending dirty
replacement decision as Open. It does not become an automatic conversion
action on an already open terrain file.

Allocate transient editor IDs for entries separately from their durable
strings. Reuse the concrete object-value variant and history. Add/duplicate
entries below the limit, generating the first unused `entry-N` ID; duplication
copies the pose and never changes the default. Entry renames and any default
reference update are one command. Making an entry default is one command.
Refuse deletion of the default/only entry with instructions to choose another
default first. This avoids silently changing where a level starts.

Keep the existing 128-entry history and saved revision model. Undo/redo
restores entry identity, default, pose, and object selection. Launch selection
is editor-only state: initialize it from the default on replacement, follow
the same transient entry on rename, and reset it to the current default if
that entry is removed. None of those launch-selection changes marks the
document dirty. An invalid loaded default remains a diagnostic until the
author selects a valid default; do not silently repair the authored field.

Keep ordinary object picking nearest-first. Add a concrete surface-hit result
for placement: position, outward normal, distance, and transient structural
object/face or terrain identity. Offer two explicit modes:

- **Scene surfaces:** test structural boxes and present terrain, excluding the
  moving object. Preview the nearest surface's identity, height, and normal.
  An unsuitable nearer face blocks placement; no hidden search through it.
- **Terrain only:** preserve the existing explicit terrain workflow and
  triangle intersection, disabled when terrain is absent.

Use the same current hit for preview and the committing click. Ordinary
entry placement requires an upward face; move the camera to see a lower floor
under an upper slab. Numeric position entry remains available for exact
authoring. No selection masks, floor registry, or hidden construction planes
are introduced.

| Target | Solid | Entry | Chair | Point light | Switch |
| --- | --- | --- | --- | --- | --- |
| Upward face / terrain | Bottom at hit | Feet at hit | Translation at hit | Hit plus height offset | Hit plus height offset, preserve yaw |
| Vertical solid face | Contact face at hit, retain axes | Unavailable | Unavailable | Hit plus outward offset | Plate back just outside wall, front along outward normal |
| Solid underside | Unavailable | Unavailable | Unavailable | Unavailable | Unavailable |

Expose light/switch offsets in placement controls; keep them editor-only.
Terrain-only mode initializes height from the old terrain anchor when that
anchor exists. Otherwise use a visible starting height of 2 m for lights and
1.4 m for switches, with author adjustment before placement. Wall lights use
a visible initial 0.1 m outward offset. Solid wall anchoring offsets its center
by the half extent along the hit normal. A mounted switch aligns its local
front to the wall normal and leaves its back 1 mm outside the face, preserving
its link and initial state. Placement is one edit; misses, unsuitable hits,
Cancel, UI capture, and camera input produce none.

Retain gameplay-invalid edits for repair. Open admits only a bounded,
structurally safe decoded candidate, then shows shared gameplay diagnostics.
Malformed or unsafe data preserves the active document. Save and play both
require full validation. New/open/close clears history, placement hits,
terrain gestures, and stale entry selection after the existing dirty decision.

**Alternatives considered:** Retaining one special spawn makes upper/lower
starts cumbersome. Treating runtime IDs as selection handles breaks identity
on reorder and reload. Adding a generic command or transform framework would
not simplify these few existing operations. A ray through all floors with an
automatic highest/lowest policy would make stacked-floor placement ambiguous.

### 6. A play request is a saved-document transaction

Keep orchestration in editor core/application code, with the UI presenting
decisions and a small editor-owned native process owner handling execution.
The game target acquires no editor dependency. Prepare this sequence once per
Play activation:

1. Finish a pending property commit or terrain stroke and run full validation.
   Require a valid selected entry and no active child.
2. If dirty, offer **Save and Play** or **Cancel**. There is no implicit discard
   or launch-of-old-saved-state option. Without a path, Save and Play enters
   the existing Save As path dialog. A failed save never launches.
3. After a successful save, or for a clean document, reload the resolved file,
   validate it and the selected entry, and compare normalized semantic content
   with the prepared document. If the disk file changed externally, refuse
   launch and explain that Save or Open is needed. Do not write it implicitly.
4. Send the sibling executable and distinct `--level`, absolute path,
   `--entry`, identifier arguments to the process owner and consume the pending
   action. Errors do not leave a deferred launch armed for another frame.

Locate the game beside the actual editor executable, not through PATH or the
level's directory. Extend only the desktop branches already supported by
native executable discovery. Launch directly through the host process API;
preserve arguments and Unicode without a command shell. The editor-launched
game is a separate visible interactive window; helper consoles are unnecessary.

Own the child handle/PID with RAII, check exit non-blockingly in the editor
loop, and reap completed children. While one child is running, disable Play
and show its level and entry; keep authoring responsive. Report native launch
errors and unsuccessful exit status with the attempted executable/path/entry.
The child retains its existing startup diagnostics; process-created status
must not be described as proof that game initialization succeeded. Detailed
story logs and runtime inspection remain P11 work.

The child owns its runtime after launch. Editing/saving/closing an editor
document has no effect on that run. Editor shutdown releases monitoring
resources without killing or blocking on the game. No IPC, embedded game
loop, readiness protocol, or automatic restart is introduced.

There is an ordinary filesystem race after preflight and before the child
opens the file. The child independently validates what it loads and reports
failure. P01 does not lock external authoring files or promise a snapshot
against concurrent external writers; exercise normal playtests with a single
writer. Preflight comparison prevents knowingly launching stale disk content.

**Alternatives considered:** Running gameplay inside the editor loses process
and dependency isolation. Shell-composed launch text mishandles real file
paths. Writing a temporary unsaved snapshot would skip the explicit saved-file
contract. Discard-and-play adds another document-state policy without being
needed for M1. A general subprocess abstraction has only this one caller.

### 7. Retain small behavioral acceptance fixtures

Add `resources/levels/apartment-stairs.level.json` to the same executable-
relative copy rules as the prototype and use it in tests and both smoke paths.
Build the upper apartment at Y=3 m and lower landing at Y=0, connected by
ordinary box treads with rises of 0.2 m, comfortably below the existing 0.3 m
step limit. Provide adequate standing headroom and at least 1.2 m clear stair
width. Exact room and corridor coordinates may be adjusted during authoring
to make the required route readable; both entries, route, and no-terrain
acceptance remain fixed. Use existing Floor/Boundary roles for slabs/walls
and supported obstacle/step roles for treads. No special collision-only ramps
or movement changes compensate for unwalkable geometry.

Preserve the packaged prototype as the default terrain/movement/light-switch
fixture. Migrate its file to v4 without changing its content, and retain
explicit v2/v3 compatibility fixtures separately in tests. Do not regenerate
old-format test inputs by changing only the current output's version number.

Validation during implementation follows [DEVELOPMENT.md](../../../../docs/DEVELOPMENT.md):

| Area | Evidence |
| --- | --- |
| Codec and admission | v2/v3 read-only normalization; canonical v4 saves with and without terrain; malformed/unknown fields; duplicate/missing/default IDs; bounds; every entry validated |
| Support and physics | Same X/Z on two floors; unsupported height; wall/ceiling/yawed-proxy overlap; terrain intrusion; upper floor over/outside terrain; actual Jolt settling and bidirectional stairs |
| Editor commands and UI | New/dirty replacement; entry add/rename/default/delete/undo; saved revision; upper-floor and wall hit previews; capture/miss/cancel; terrain-tool transitions; editable invalid loads |
| Launch workflow | Clean Play, Save and Play, Save As, canceled and failed saves, disk mismatch, missing executable, one launch per action, one child at a time, nonzero exit, editor responsiveness |
| Native launch and paths | Real argument-recording child with spaces, non-ASCII and valid shell-significant path characters; alternate CWD and indirect executable invocation; no shell execution |
| Renderer lifetime | Terrain-free/terrain-bearing replacement; empty generated editor stream; current switch state; resize/minimize/recovery and partial-construction cleanup in game/editor Vulkan smoke |
| Manual M1 acceptance | Create, author, save, reopen and play; start both floors; walk room/corridor/kitchen/stairs/landing in both directions; cancel dirty Play; verify default packaged file is unchanged |

Use a small injected launch callable or recording result at the editor
application boundary for deterministic decision tests, plus a real process
probe to check native argument handling. Tests should exercise final document
values, poses, launch records, failure behavior, and lifetime invariants rather
than freeze source names, statement order, or new internal call counts.

## Risks / Trade-offs

- **Conservative entry clearance can reject tight positions** -> retain
  actionable diagnostics and numeric correction; verify representative
  accepted/rejected poses with Jolt and keep generous M1 entry clearances.
- **Stacked slabs can occlude the intended placement surface** -> show the
  nearest face and height, reject unsuitable hits, and keep free-fly/numeric
  authoring available instead of guessing another floor.
- **Native child ownership and argument encoding differ across current hosts**
  -> keep that code editor/launcher-local and test literal arguments and exit
  handling through a real child on each exercised host; report unavailable
  host validation.
- **An external writer can race file launch** -> compare saved semantic state
  at preflight and validate again in the child; no filesystem lock or snapshot
  guarantee is made for concurrent external edits.
- **Two unshadowed lights limit interior atmosphere** -> assess traversal and
  authoring with temporary content and flashlight; P02/P10 own asset and visual
  expansion. Do not silently use M1 imagery as final visual acceptance.
- **Older builds cannot read explicit v4 saves** -> show migration status,
  retain v2/v3 read tests, and use Save As/backups when old builds are needed.

## Migration Plan

1. Implement normalized v4 data and v2/v3 reads with compatibility fixtures,
   then update support/clearance and immutable entry resolution.
2. Update runtime, physics, generated geometry, and editor consumers together
   so no consumer assumes terrain or a singleton spawn remains present.
3. Add interior creation, entry and surface commands, and saved-file playtest
   orchestration; validate each through deterministic behavior.
4. Explicitly migrate the packaged prototype to v4 and add the acceptance
   level and resource copying. Run the existing terrain fixtures alongside M1.
5. Update the relevant current-implementation sections of architecture,
   gameplay, rendering, and development documentation, including launch
   commands, compatibility, and the author-save-launch procedure. Keep the
   roadmap's status and links accurate; later proposals rebase on P01's main
   requirements after archive.
6. Run debug configure/build/tests, both Vulkan smoke paths, and the manual
   M1 exercise. Record missing validation rather than checking its task off.

Rollback means using the previous executable with its previous packaged
resources and an original v2/v3 level copy. This change provides no lossy v4
down-converter: optional terrain and multiple entries cannot be represented
faithfully in the old format. Opening is never a migration write.
