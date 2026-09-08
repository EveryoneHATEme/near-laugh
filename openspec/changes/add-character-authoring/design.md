## Context

[P07c's proposal](proposal.md) closes the split P07 workflow after P07b's v9
definitions, runtime route behavior and initial editor compatibility. Existing
EditorDocument owns one selection, durable referenced IDs and 128-entry history.
Picking/overlays, safe invalid documents, transactional GPU replacement and
saved-file child-process Play already exist.

The editor currently does not link physics or construct the game runtime.
P07b's capsule/door acceptance belongs to the saved game run. P07a supplies the
shared pure animation sampler and animated rendering resources.

## Goals / Non-Goals

**Goals:** author the supported character scene with existing document concepts,
inspect clips/route/facing, repair links, then validate the actual saved scene
in the ordinary game.

**Non-Goals:** game simulation inside the editor, live game synchronization,
animation graph/timeline authoring, new asset imports, skeletal editing,
retargeting, arbitrary scripted actions or a second level-format change.

## Decisions

### 1. Extend the existing flat commands and history

Add actor, mark and route selection kinds to the existing supported object set.
Use transient selection handles only in editor memory; all serialized links use
durable strings. Property drafts commit through existing commands and group
a rename plus its incoming link rewrites into one history entry.

Use separate actor/mark/route panels within existing Objects/Properties.
Catalog controls expose the supported model and final clip; references show
durable labels and preserve invalid strings visibly. Actor start placement is
its initial mark. Show all consumers when a mark is shared, making a mark move's
effect inspectable before committing it. Do not maintain a duplicate actor
transform that can disagree with that mark.

Adding an actor can atomically add its initial mark. Duplicating an actor clones
the mark to an independent new identity, preserves model/speed and clears
initial_route/footstep_source/interaction_source. This avoids implicit route
ownership errors and audio sharing. Check both actor and mark capacities before
the command. The new placement can be safely invalid due to overlap until
moved; it remains repairable. Route duplication retains owner and ordered
outgoing links. Deletion retains incoming broken links; no cascading remove.

Extend source renames to include incoming actor links as well as existing
audio users. Preserve all link updates and saved revision identity through
undo/redo. Do not invent an entity registry or general reference-management
framework for these bounded collections.

### 2. Surface placement and overlays reuse existing geometry tools

Pick actors by conservative catalog visual bounds, independent of their capsule,
and marks by visible handles. Route line/mark picking resolves to the same
single selection as the list; list access remains authoritative for invalid
records that have no safe spatial representation. Show facing arrows,
ordered route links, missing endpoints and distinct visual/proxy bounds through
the existing clipped overlay path.

Placing an actor edits its initial mark at the exact upward structural or
terrain hit. Reuse nearest-surface rejection, UI capture, Escape and one-gesture
history behavior. Shared-mark consumers update together and are identified
in Properties. Mark points do not become collision or player support. Keep
top-floor support and blocked/headroom errors under shared v9 validation.

### 3. Silent snapshot inspection; physical acceptance uses Play

The editor application owns one transient preview session: either a selected
clip or one selected route. Capture validated selected definitions/resources
and a document generation. Reuse P07a sampler/blends and actor render handles;
other instances display their authored initial idle pose. Expose clip selection,
play/pause/resume/restart/stop and time seek. Clip seek has no events or audio.

Route inspection traverses its ordered mark positions and yaws schematically,
using authored speed and supported standing-turn/walk/interact conventions.
It can linearly visualize a stair segment's endpoint elevations because it is
a route diagram in motion, not a grounded collision prediction. Identify this
mode in the panel as excluding collision, door operation and sound. Show its
current segment/mark and final action, and use the existing Play action for
actual blocking/audio checks. Do not link PhysicsWorld or Engine just to preview.

Any edit, history action, document object selection change, New/Open/Close, minimize or Play
stops and drops the snapshot. Fresh Start requires valid current references.
Changing the preview clip is a playback command, not a document selection change.
Pause retains preview time; only presentation recovery without invalidation
continues the current preview. Starting audio audition stops character preview,
and vice versa. This uses existing P04 audition for explicit sound inspection
without introducing two simultaneous editor coordinators.

Snapshot preview was chosen over an embedded game because it preserves the
existing editor ownership boundary and keeps this change independently
reviewable. The product wording must expose its practical limits clearly.

### 4. Coherent preview resources and saved-file launch

Use the existing document generation and transactional scene replacement for
selected character assets/materials/initial palettes together with static
geometry/lighting. Pose-only playback writes frame-slot changing data and does
not rebuild static scene resources. Retain the complete old set on failed
asset decoding/allocation and label it stale. Safely invalid references
remain visible via list/markers; no fallback model is silently saved.

P07b already extends selected character/audio preflight. Verify that a pending
preview stops before Play and that all fields survive saved-file semantic
comparison. The launch path remains sibling near_laugh plus literal absolute
level/entry arguments. Dirty work requires the existing Save and Play/Cancel
decision; do not add an alternative editor-only scene handoff. Device absence
does not block Play, but missing selected content does.

### 5. Close T2 with a second authored scene

Build a second small neutral interior using editor actions: floor/walls, safe
player entry, a door, local shadowed lights, one actor, marks and route, selected
short sound/caption links and an initial route. Save As to an independent path,
reopen, inspect clips, then run the saved scene from outside the repo with a
Unicode path. Demonstrate a blocked/released route, facing/action sound and
animated shadows. No JSON patch or new runtime scene code may define it.

Use behavioral command tests for references/compound capacity/history/dirty
state, real ImGui tests for editing and preview controls/capture, process tests
for failed/successful fresh preflight, and editor Vulkan smoke for live poses,
edit/undo/replace/minimize/recovery/allocation failure and shutdown. Record the
manual authoring/play sequence and its limitations in validation.md. P07b
already owns full runtime motion/timing acceptance; repeat that measurement
only for a new change or an observed regression.

## Risks / Trade-offs

- Schematic routes can appear physically traversable → label their scope and
  require saved-game obstruction evidence for T2.
- Shared marks can affect several consumers → show incoming uses and make
  changes atomic/undoable; clone initial marks when duplicating actors.
- Safe invalid data may lack preview geometry → retain list selection and
  explicit diagnostics instead of inventing coordinates or discarding data.
- Preview state can outlive a document → check generation/selection and
  invalidate at every documented transition before consuming another frame.

## Migration Plan

Reuse P07b's canonical v9 shape and legacy readers unchanged. No new conversion
is needed; test character-bearing documents through all existing workflows.
Update DEVELOPMENT with actual controls and scope and ARCHITECTURE/RENDERING
with implemented ownership only after validation. On acceptance, sync/archive
P07c and update ROADMAP to mark the whole P07 chain/T2 accepted with its records.
Later P06/P05 work consumes the completed chain. Rolling back these editor
controls leaves P07b runtime and v9 content valid but removes authoring readiness.
