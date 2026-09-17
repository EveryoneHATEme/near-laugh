## Why

A switch and moving doors do not provide document reading or physical object
handling. P06 prepares a complete editor-to-play workflow for reading a neutral
document, picking up/dropping/throwing an explicitly authored physical box,
and switching a stationary radio, before deciding the game's errands or story.

## What Changes

- Support explicitly authored pickup objects, initially generated boxes with
  gravity, full rotation, contact response and sleeping. Existing static props,
  furniture, doors and characters do not automatically become pickup objects.
- Hold at most one physical box visibly in front of the player without hands;
  release it at its actual physical pose or throw it with a bounded impulse.
  Objects remain subject to collision while held and after release. Placement
  follows physical contact with surfaces; it does not require authored slots.
- Preserve object identity and actual pose through pickup, obstruction, drop,
  throw and repeated actions. Rendering, shadows, targeting and collision must
  agree; refused pickup or obstructed holding must not teleport or duplicate it.
- Extend the existing nearest-target interaction rules to these objects,
  retaining reach, obstruction, input-edge, and no-fallthrough guarantees.
- Present readable Russian document text and interaction/holding feedback using
  the existing text subsystem, alongside captions. Reading suppresses player
  controls while world time continues; explicit suspension remains separate.
  Return to exploration without a held press becoming a world action.
- Operate stationary radios through explicitly owned existing audio sources,
  with visible on/off feedback even when muted or no audio device is available.
- Make accepted object state changes available through their concrete owners.
  P05 later uses them as event conditions/actions; this change needs no story
  facts, quest links, relationship model, or help outcome.
- Author identities, initial box placement, readable content and radio links
  with editor selection, validation and undo/redo. The editor shows initial
  definitions; physical play takes place in the existing separate game process.
  Source definitions remain unchanged during play.
- **BREAKING** for older builds: introduce level format v10 with explicit v9
  read compatibility and retained v2-v8 migrations. Opening never rewrites a
  file; only explicit Save writes v10, which older executables cannot read.

## Capabilities

### New Capabilities

- `household-interactions`: Readable documents, supported physical boxes,
  pickup/holding/drop/throw, stationary radios and coherent run-local state.

### Modified Capabilities

- `level-persistence`: Version-10 household definitions, references, migration
  and immutable runtime handoff.
- `level-object-placement`: Author/select the supported household objects.
- `level-editor`: Edit readable content, diagnose household links and preflight
  the complete saved-file interaction scene.
- `authored-interaction`: Include supported household objects in deterministic
  nearest-target arbitration without acting through a refused target.
- `game-text-presentation`: Present readable documents and action feedback
  alongside the existing caption capability.
- `physics-simulation`: Bounded dynamic boxes, collision-respecting holding,
  throwing, safe player/actor/door integration and resource lifetime.
- `interactive-doors`: Physical boxes obstruct door sweeps without crushing.
- `player-input`: Define exploration versus document-reading action ownership
  and safe input transitions.
- `player-controller`: Box contact without player propulsion or box-supported
  traversal, and reading-aware cursor/input transitions.
- `runtime-composition`: Own item/action state and coordinate its changes
  and presentation without modifying the authored level.
- `vulkan-renderer`: Full-orientation generated box presentation, coherent
  changing shadows, reading/feedback text and frame-resource recovery.

## Impact

Affects world data/codec/validation, Jolt body and player collision ownership,
concrete interaction and input, runtime coordination, changing geometry and text
presentation, audio coordination, editor commands/properties/preflight, tests
and the affected gameplay/architecture/rendering/development documentation.
Reuse current dependencies, a generated box/document representation and the
P02 stationary radio asset. No new asset importer is required.

## Dependencies and Boundaries

P06; requires
[P04](../archive/2026-09-07-add-spatial-audio-and-captions/proposal.md) and
[P10](../archive/2026-09-07-add-interior-lighting/proposal.md), including their asset, door, and
interaction prerequisites. Rebase targeting on P10's multiple switches. The
selected work order puts this after the accepted P07a-P07c chain. Physical boxes
now require regression checks with the existing actors as well as player and
doors; this does not add character prop manipulation. P05 later connects object
state to events, P09 adds checkpoint reconstruction, and P12 supplies session
menus. Those changes must rebase on P06's physical state and suspension contract.

Required scope is the bounded box profile, reading, radio and their complete
authoring workflow. Arbitrary mesh pickup, physical furniture, inventories,
hand animation, manual in-hand rotation, adjustable hold distance, throwing
damage/combat, breakage, impact-sound synthesis, narrative reactions, save-game
serialization and final story content are outside scope. Additional object
shapes and art are follow-up work with their own supported-profile review.

## Acceptance Criteria

- Read a neutral document, pick up/drop/throw physical boxes and operate a radio
  with visible/audible feedback. Repeat actions in different orders without
  duplicate objects or repeated throws from held input.
- Exercise gravity, tumbling, settling, box-to-box contact, a thin wall, a
  stationary player, doors and actors. Held boxes cannot pass through walls;
  release uses the actual pose and blocked holding has a bounded safe outcome.
  Motion and contact remain coherent with targeting, color and shadows.
- Reading text suppresses conflicting world interaction; returning to play
  cannot reuse a held press. Occluded/out-of-range actions remain unavailable.
- Save/reopen definitions and undo/redo preserve identities and links. Author a
  second neutral scene through the UI and launch it through ordinary saved-file
  Play. Fresh scene entry restores initial state; minimize, explicit suspension
  and presentation recovery preserve existing state without delayed actions.
- Run deterministic state/input/reference/physics checks, integrated Debug
  checks, affected Vulkan smoke/readback, manual T3 interaction and authoring
  acceptance, and bounded Release measurements. Keep unavailable checks and
  unresolved visual or physical-behavior acceptance explicit.
