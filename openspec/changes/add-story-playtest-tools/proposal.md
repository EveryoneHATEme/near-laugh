## Why

The author needs to reproduce a known scene state and understand why an event
did or did not run before assembling a story. Prepared launches and actionable
diagnostics complete the technical iteration workflow built on event state
and checkpoint reconstruction.

## What Changes

- Add named, validated playtest setups that select a saved level, entry or
  checkpoint, and coherent prepared object, actor, light, and event state.
  Launch a separate game process through the existing editor playtest workflow.
- Make setup selection explicit and isolate test progression/saves from the
  player's normal continuation. Reject incomplete or contradictory setups
  with useful reference/state diagnostics.
- Expose active scene state/facts and a bounded event trace with reasons for
  condition rejection, execution, interruption, and cancellation. Connect
  reported identities to the corresponding authored records.
- Add repeatable restart of the selected scene and author-facing validation
  of event, object, actor, light, cue, and checkpoint links using real runtime
  policies.
- Preserve dirty-document decisions and undo/redo for editable setup data.
  Editor-only diagnostics must not require a generic scripting UI or move the
  running simulation into the editor.
- Keep trace/setup facilities scoped to authoring and development. No remote
  debugger, live runtime editing, arbitrary state console, or plugin interface.
  P01 already supplies basic selected-level launching.

## Capabilities

### New Capabilities

- `story-playtesting`: Prepared scene launches, coherent test-state
  validation, isolated progress, and actionable progression inspection.

### Modified Capabilities

- `level-editor`: Select/manage test setups, launch the separate runtime,
  and present actionable state/reference diagnostics.
- `runtime-composition`: Accept explicit development setup configuration and
  expose bounded diagnostics while retaining simulation/lifetime ownership.

## Impact

Affects editor launch UI, development runtime configuration, event
diagnostics, setup data validation, and checkpoint/test-save selection. Setup
data is developer-owned; it need not become an arbitrary runtime level field.
Document one repeatable author-edit-save-start-inspect workflow.

## Dependencies and Boundaries

P11; requires [P09](../add-checkpoint-resume/proposal.md), including already
implemented object, actor, lighting, and event state. P05 supplies baseline
event diagnostics and P01 supplies safe launch. Use P09's explicit entry and
save isolation; P12's menus are not a prerequisite. P12 final packaging/workflow
acceptance requires this change. T6 exercises neutral scenes before story
development, and later story-specific states extend setups only when needed.

## Acceptance Criteria

- Start a neutral interaction/event scene directly from a named setup and
  reproduce it after restart without manually editing source JSON or repeating
  earlier interactions. The prepared actor, object, light, and event states agree.
- Inspect a missing test cue and see the condition that cancelled it; inspect
  another unsatisfied event and see its actual unmet condition.
- A broken object/actor/light/cue/checkpoint reference points to the authorable
  record. A contradictory setup cannot launch a partially initialized scene.
- Canceling a dirty-document launch leaves both document and running session
  unaffected. Test runs cannot replace the ordinary player's continuation.
- Test setup/diagnostic behavior and manually exercise the editor-to-game
  iteration loop, including a deliberate link error and its correction.
