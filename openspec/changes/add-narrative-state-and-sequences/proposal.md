## Why

The author needs to connect implemented objects, characters, lights, and audio
into predictable events before developing the story. Neutral test sequences
must establish conditions, interruption, and cancellation beyond P04's fixed
audio demonstration.

## What Changes

- Add bounded scene facts/state, named trigger regions, and authored sequence
  parameters for this game's supported actions. Define location, accepted
  interaction/object state, elapsed active time, and prerequisite conditions
  using neutral fixtures; do not encode final story phases or relationships.
- Give events durable identities and defined one-shot/repeat behavior.
  Establish deterministic resolution of competing conditions and cancellation
  of queued actions that no longer fit the current scene state.
- Drive supported door, object, and character actions, sound/caption cues, and
  local light state through their concrete interfaces while preserving
  immutable authored definitions. Respect refused/blocked actions and busy
  foreground audio; define their sequence consequences without forcing success.
- Coordinate event timing with pause, minimization, and input ownership,
  including document reading and the existing player/door/actor/audio owners.
  Outcomes do not depend on frame count, rendering recovery, or audio hardware
  callbacks; leaving a scene has an explicit continuation/interruption policy.
- Add authorable regions, references, initial state, and supported sequence
  parameters to the editor, with validation and undo/redo. New behavior kinds
  remain explicit game code rather than arbitrary user scripts.
- Build a neutral region-triggered light/sound/character sequence and a second
  condition that cancels a pending action. Use repeated entry and alternate
  action order to verify behavior without a telephone plot or rescue branch.
- Version serialized narrative additions explicitly. No generic scripting
  language, node editor, behavior trees, or global event bus.

## Capabilities

### New Capabilities

- `narrative-progression`: Bounded scene state, supported conditions,
  consequence resolution, and deterministic progression for later story use.
- `authored-sequences`: Named triggers, bounded authored event sequences,
  cancellation, interruption, and observable execution history.

### Modified Capabilities

- `level-persistence`: Persist narrative definitions, identities, and references.
- `level-object-placement`: Author trigger regions and scene markers.
- `level-editor`: Edit supported sequence parameters and diagnose broken links.
- `player-input`: Define action ownership and stale-input suppression across
  sequence suspension, reading, and return to exploration.
- `runtime-composition`: Own narrative advancement and coordinate concrete
  world/presentation actions under a defined active-time policy.

## Impact

Affects gameplay state ownership, loop timing, input gating, level validation,
and editor authoring. Keep a deterministic progression core testable without
a window, GPU, or audio device. Update gameplay and architecture documentation
with the supported state/event model and timing rules.

## Dependencies and Boundaries

P05; requires [P06](../add-household-interactions/proposal.md) and
[P07](../add-scripted-characters/proposal.md), including their P04 audio/text
and P10 lighting prerequisites. Rebase on their resulting capabilities and
add modified-capability entries where event integration changes requirements.
This change owns event-driven integration of the earlier technical systems.
T4 uses neutral state transitions; actual story development follows T6.
P09 adds durable save files, P11 adds author-facing inspection/prepared scene
launch, and P12 uses the suspension policy for menus. Basic event diagnostics
and a minimal development pause control belong here, so testing does not wait
for P12. These later changes are not prerequisites for P05 acceptance.

## Acceptance Criteria

- The neutral sequence reaches its defined state/action and cue order during play,
  repeated region entry, and different render/fixed-step batch sizes.
- A completed event cannot replay merely because a region is re-entered or
  rendering recovers. Competing triggers resolve identically from equal state.
- Activating the competing test condition cancels the incompatible pending
  action; no stale sound, actor request, or light change executes afterward.
- A blocked actor/door action or busy foreground cue follows its documented
  continuation/refusal policy without bypassing the underlying owner.
- Leaving during a cue, opening a door, reading a document, pausing, and
  minimizing each have a defined, testable result without hidden elapsed time.
- Broken references and unsupported conditions are rejected with scene/event
  context. Editor save/reopen and undo/redo preserve state definitions and
  links. Run deterministic progression tests and play the integrated neutral
  scene audibly, muted, and without an output device.
