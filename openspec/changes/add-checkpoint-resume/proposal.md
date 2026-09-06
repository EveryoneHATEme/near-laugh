## Why

Testing and playing the evening requires returning to a meaningful state
without replaying every errand. The existing level codec stores authored
definitions and cannot restore decisions, moved items, or a locked room.

## What Changes

- Add a separate, versioned save-game representation of concrete progression:
  level/checkpoint identity, an authored safe player placement, relevant story
  facts, completed events, door/light state, and household-item state.
- Restore defined checkpoint boundaries rather than arbitrary runtime memory.
  Reconstruct active ambience and eligible future events while preventing
  replay of completed one-shots or resurrection of cancelled danger sequences.
- Provide an explicit checkpoint resume/restart entry point sufficient for
  playtesting before P12 supplies the full game-session menu.
- Preserve the previous usable save when writing fails. Diagnose damaged,
  unsupported, or content-incompatible saves without entering a partially
  restored game or altering authored level files.
- Define reset of input latches, simulation/story timing, physics placement,
  transient text, and audio playback so restoration behaves like a coherent
  scene entry. Keep user saves outside packaged source assets.
- Give checkpoints stable identities and validate their safe placement and
  referenced story state. Choose exact checkpoint locations, retention, and
  compatibility policy during design using the first playable branch.
- P07 and P08 must extend reconstruction for their actor/outcome state when
  introduced. No generic object serializer, arbitrary mid-animation save,
  cloud storage, or multi-profile framework.

## Capabilities

### New Capabilities

- `checkpoint-resume`: Durable checkpoint state, safe reconstruction,
  compatibility diagnostics, and preservation of the last usable save.

### Modified Capabilities

- `runtime-composition`: Coordinate checkpoint entry/reconstruction across
  player, physics, narrative, world presentation, and audio owners.
- `player-controller`: Restore a validated checkpoint pose/stance and reset
  transient motion/input so resumed play begins coherently.

## Impact

Adds concrete save data and filesystem operations distinct from level
persistence. Affects story checkpoints, runtime reset/entry, player/physics
placement, door/item/light state, and audio/text restoration. Document save
location, supported compatibility, and the initial resume command/workflow.

## Dependencies and Boundaries

P09; requires [P06](../add-household-interactions/proposal.md).
P05 supplies progression identities and P01/P03 supply entry/door definitions.
No current level codec requirement changes merely because a separate save
file exists. Revisit prerequisite capabilities during detailed planning if
checkpoint marker authoring needs an additional requirement.

## Acceptance Criteria

- Save after an errand and early-help decision, exit the process, and resume.
  Item location, recognition, help facts, door/light state, and eligible events
  agree with the checkpoint's documented state.
- Resume before the telephone or danger slice without replaying previously
  completed cues or scheduling an incompatible invitation.
- Changed ordering of authored records does not reassign saved identities.
  Unknown references or incompatible content fail with a useful explanation.
- Simulate malformed/truncated saves and failed replacement writes; retain the
  previous usable save and leave authored files unchanged.
- Test reconstruction deterministically, then restart and resume the actual
  playable branch, including input held across a resume transition.
