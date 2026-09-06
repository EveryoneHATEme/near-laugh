## Why

The opening errands teach the apartment and establish relationships that can
matter during escape. The player also needs concrete ways to call or attract
help; a switch and moving doors do not cover those actions.

## What Changes

- Implement bounded household interactions for the reference apartment:
  reading/delivering the letter, acquiring/transferring bread and the saucepan,
  using the textbook, and the needed dinner/kettle/radio state changes.
- Represent the meaningful item locations and progression facts explicitly.
  Support carrying/placing a specific story object with clear feedback; do not
  introduce an equipment or general inventory framework.
- Support the authored telephone contact attempts and calling for help through
  a window or to a neighbor. Use context-sensitive actions, not an unnecessary
  telephone-dialing simulator or an abstract trust-choice menu.
- Present readable document text and interaction feedback through existing
  text support. Define text-reading input ownership and return to exploration.
- Connect accepted actions to narrative consequences. A delivered letter can
  establish recognition; successful early help changes later reactions rather
  than being ignored to preserve the default sequence.
- Author object identities, initial placement/state, readable content, and
  concrete narrative links with editor selection, validation, and undo/redo.
  Source definitions remain unchanged by pickup, reading, or delivery.
- Version required content additions explicitly. Detailed hand animation,
  general physics pickup, and the complete escape encounter are outside scope.

## Capabilities

### New Capabilities

- `household-interactions`: Concrete errands, story-item state, readable
  objects, telephone contacts, and requests for help.

### Modified Capabilities

- `level-persistence`: Persist household object definitions and references.
- `level-object-placement`: Author/select the supported household objects.
- `level-editor`: Edit readable content and diagnose invalid interaction links.
- `player-input`: Define exploration versus document-reading action ownership
  and safe input transitions.
- `runtime-composition`: Own item/action state and coordinate its consequences
  and presentation without modifying the authored level.
- `vulkan-renderer`: Present supported object presence/placement changes and
  readable-content requests coherently with the running state.

## Impact

Affects concrete gameplay actions, world validation, text-reading interaction,
object visibility/placement, audio feedback, and editor properties. P02 assets
may replace temporary household geometry when available. Document only the
supported interactions and their authoring contract.

## Dependencies and Boundaries

P06; requires [P05](../add-narrative-state-and-sequences/proposal.md).
Reuse its progression, targeting, audio, and text prerequisites. P08 composes
the final rescue/escape outcomes; this change exercises action consequences in
a small authored branch. No final ending condition is decided here.

## Acceptance Criteria

- Read and deliver the letter; the recipient's later reaction reflects
  delivery. Repeating the action cannot duplicate the item or its consequence.
- Complete representative food/utensil actions in a different supported order
  without losing a required item or creating an impossible state.
- An accepted early call or request produces a meaningful authored response
  and prevents a contradictory pending danger cue.
- Reading text suppresses conflicting world interaction; returning to play
  cannot reuse a held press. Occluded/out-of-range actions remain unavailable.
- Save/reopen definitions and undo/redo preserve identities and links. Run
  state/input/reference tests and a manual errands/early-help playthrough.
