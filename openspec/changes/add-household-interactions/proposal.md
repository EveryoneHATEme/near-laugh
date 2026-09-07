## Why

A switch and moving doors do not provide document reading, object carrying,
placement, or other supported household actions. Prepare these interactions
and their authoring workflow before deciding the game's errands or story.

## What Changes

- Implement a bounded set of concrete actions: read a document, pick up/carry
  and place a supported household object, and operate a household prop such as
  the radio. Use neutral content to choose and validate the supported profile.
- Own explicit object location/use state with clear success/refusal feedback.
  Repeated actions cannot duplicate an item or lose it on failed placement.
  Keep rendering, interaction targeting, and any associated collision coherent.
- Extend the existing nearest-target interaction rules to these objects,
  retaining reach, obstruction, input-edge, and no-fallthrough guarantees.
- Present readable document text and interaction feedback using the existing
  text subsystem. Define reading input ownership, suspension behavior, and
  return to exploration without a held press becoming a world action.
- Make accepted object state changes available through their concrete owners.
  P05 later uses them as event conditions/actions; this change needs no story
  facts, quest links, relationship model, or help outcome.
- Author object identities, initial placement/state, readable content, and
  supported action/placement references with editor selection, validation,
  and undo/redo. Source definitions remain unchanged during play.
- Version required content additions explicitly. Detailed hand animation,
  general physics pickup, an inventory/equipment framework, and story-specific
  errands, telephone contacts, or rescue rules are outside scope.

## Capabilities

### New Capabilities

- `household-interactions`: Supported household actions, readable objects,
  carrying/placement, and coherent run-local object state.

### Modified Capabilities

- `level-persistence`: Persist household object definitions and references.
- `level-object-placement`: Author/select the supported household objects.
- `level-editor`: Edit readable content and diagnose invalid action/placement links.
- `authored-interaction`: Include supported household objects in deterministic
  nearest-target arbitration without acting through a refused target.
- `game-text-presentation`: Present readable documents and action feedback
  alongside the existing caption capability.
- `physics-simulation`: Keep collision for supported movable/removed objects
  consistent with their accepted run-local presence and placement.
- `player-input`: Define exploration versus document-reading action ownership
  and safe input transitions.
- `runtime-composition`: Own item/action state and coordinate its changes
  and presentation without modifying the authored level.
- `vulkan-renderer`: Present supported object presence/placement changes and
  readable-content requests coherently with the running state.

## Impact

Affects concrete gameplay actions, world validation, document reading, object
visibility/placement and collision, audio feedback, and editor properties.
Use selected P02 assets or representative temporary objects. Document the
supported actions, placement rules, and authoring contract.

## Dependencies and Boundaries

P06; requires
[P04](../archive/2026-09-07-add-spatial-audio-and-captions/proposal.md) and
[P10](../add-interior-lighting/proposal.md), including their asset, door, and
interaction prerequisites. Rebase targeting on P10's multiple switches. The
selected work order puts this after P07, but actor integration is not required
for T3's object tests. P05 later connects object state to events, and P09 adds
checkpoint reconstruction. No plot, delivery errand, or accepted help decision
is needed to complete this capability.

## Acceptance Criteria

- Read a neutral document, carry/place a supported object, and operate a test
  prop with visible/audible feedback. Repeat actions and vary their supported
  order without duplicating or losing the object.
- Refuse an invalid/blocked placement without changing the last valid item
  state. Successful placement updates presentation, targeting, and collision
  together without changing the authored level.
- Reading text suppresses conflicting world interaction; returning to play
  cannot reuse a held press. Occluded/out-of-range actions remain unavailable.
- Save/reopen definitions and undo/redo preserve identities and links. Fresh
  scene entry restores initial object state; presentation recovery preserves
  the current state. Run state/input/reference/collision tests, affected Vulkan
  smoke, and a manual neutral interaction scene.
