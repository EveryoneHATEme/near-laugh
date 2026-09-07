## Why

Testing and playing an authored scene requires reliable recovery across
process restarts. The level codec stores definitions; checkpoint recovery must
reconstruct the mutable object, actor, event, door, and light state introduced
by the preceding technical changes together.

## What Changes

- Add a separate, versioned save-game representation of concrete progression:
  level/checkpoint identity, an authored safe player placement, relevant scene
  state, completed/cancelled events, door/light state, household-item state,
  and the already implemented character state.
- Restore defined checkpoint boundaries rather than arbitrary runtime memory.
  Reconstruct active ambience and eligible future events while preventing
  replay of completed one-shots or resurrection of cancelled sequences.
  Reconstruct supported actor placement, route/action state, and suitable
  animation at those safe boundaries without copying arbitrary animation frames.
- Provide an explicit checkpoint resume/restart entry point sufficient for
  playtesting before P12 supplies the full game-session menu.
- Preserve the previous usable save when writing fails. Diagnose damaged,
  unsupported, or content-incompatible saves without entering a partially
  restored game or altering authored level files.
- Define reset of input latches, simulation/event timing, physics placement,
  actor presentation, transient text, and audio playback so restoration behaves
  like a coherent scene entry. Keep user saves outside packaged source assets.
- Give checkpoints stable identities and validate their safe placement and
  referenced scene state. Choose retention, compatibility, and safe boundary
  rules during design using neutral test scenes. Actual story checkpoint
  locations are selected during later content work.
- Include P07 actors and P10 lights in initial checkpoint acceptance. Any later
  story-specific state extends reconstruction when introduced. No generic
  object serializer, arbitrary mid-animation save, cloud storage, or
  multi-profile framework.

## Capabilities

### New Capabilities

- `checkpoint-resume`: Durable checkpoint state, safe reconstruction,
  compatibility diagnostics, and preservation of the last usable save.

### Modified Capabilities

- `runtime-composition`: Coordinate checkpoint entry/reconstruction across
  player, physics, event, actor, object, light, presentation, and audio owners.
- `player-controller`: Restore a validated checkpoint pose/stance and reset
  transient motion/input so resumed play begins coherently.

## Impact

Adds concrete save data and filesystem operations distinct from level
persistence. Affects authored checkpoints, runtime reset/entry, player/physics
placement, door/item/light/actor/event state, and audio/text restoration.
Document save location, supported compatibility, and the initial resume workflow.

## Dependencies and Boundaries

P09; requires [P05](../add-narrative-state-and-sequences/proposal.md), including
P06 objects, P07 characters, and P10 lighting. Rebase on those capabilities and
include modified deltas where reconstruction changes their requirements. The
first save model covers all of their supported checkpoint state; actors are
not deferred to another milestone. P12 supplies the later session menu, and
P11 builds on the explicit resume entry without needing that menu.
No current level codec requirement changes merely because a separate save
file exists. Revisit prerequisite capabilities during detailed planning if
checkpoint marker authoring needs an additional requirement. Use neutral scene
boundaries for T5 preparation; no errand, help decision, or ending is required.

## Acceptance Criteria

- In a neutral scene, change an object's placement, operate lights/doors,
  advance a character to a supported checkpoint state, complete one event,
  and cancel another. Save, exit the process, and resume with all owners
  agreeing with the checkpoint's documented state.
- Resume with correct actor presence/placement and eligible route/action state,
  without duplicate actors, replayed completed cues, or cancelled actions
  becoming eligible again.
- Changed ordering of authored records does not reassign saved identities.
  Unknown references or incompatible content fail with a useful explanation.
- Simulate malformed/truncated saves and failed replacement writes; retain the
  previous usable save and leave authored files unchanged.
- Test reconstruction deterministically, then restart and resume the integrated
  neutral scene, including input held across a resume transition. Save/load
  must not move items into impossible collision states or restore unsupported
  player/actor placements.
