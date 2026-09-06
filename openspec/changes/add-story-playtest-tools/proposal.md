## Why

An hour-long story cannot be iterated efficiently by replaying the opening
after every event edit. Once progression and checkpoints exist, the author
needs to start a known episode and understand why a cue did or did not run.

## What Changes

- Add named, validated playtest setups that select a saved level, entry or
  checkpoint, and a coherent prepared story state. Launch a separate game
  process through the existing editor playtest workflow.
- Make setup selection explicit and isolate test progression/saves from the
  player's normal continuation. Reject incomplete or contradictory setups
  with useful reference/state diagnostics.
- Expose the active phase/facts and a bounded event trace with reasons for
  condition rejection, execution, interruption, and cancellation. Connect
  reported identities to the corresponding authored records.
- Add repeatable restart of the selected episode and author-facing validation
  of story, object, cue, and checkpoint links using the real runtime policies.
- Preserve dirty-document decisions and undo/redo for editable setup data.
  Editor-only diagnostics must not require a generic scripting UI or move the
  running simulation into the editor.
- Keep trace/setup facilities scoped to authoring and development. No remote
  debugger, live runtime editing, arbitrary state console, or plugin interface.
  P01 already supplies basic selected-level launching.

## Capabilities

### New Capabilities

- `story-playtesting`: Prepared episode launches, coherent test-state
  validation, isolated progress, and actionable progression inspection.

### Modified Capabilities

- `level-editor`: Select/manage episode setups, launch the separate runtime,
  and present actionable story-reference diagnostics.
- `runtime-composition`: Accept explicit development setup configuration and
  expose bounded diagnostics while retaining simulation/lifetime ownership.

## Impact

Affects editor launch UI, development runtime configuration, narrative
diagnostics, setup data validation, and checkpoint/test-save selection. Setup
data is developer-owned; it need not become an arbitrary runtime level field.
Document one repeatable author-edit-save-start-inspect workflow.

## Dependencies and Boundaries

P11; requires [P09](../add-checkpoint-resume/proposal.md).
P05 supplies baseline event diagnostics and P01 supplies safe launch.
Complete this change before assembling P08's full story; later actors and
outcomes provide their concrete setup/reconstruction data through their owners.

## Acceptance Criteria

- Start the telephone scene directly from a named setup and reproduce it after
  restart without manually editing source JSON or playing preceding errands.
- Inspect a missing invitation and see that early help cancelled it; inspect
  another unsatisfied event and see the actual unmet condition.
- A broken object/cue/checkpoint reference points to the authorable record.
  A contradictory setup cannot launch a partially initialized episode.
- Canceling a dirty-document launch leaves both document and running session
  unaffected. Test runs cannot replace the ordinary player's continuation.
- Test setup/diagnostic behavior and manually exercise the editor-to-game
  iteration loop, including a deliberate link error and its correction.
