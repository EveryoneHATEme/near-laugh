## Why

The visitor's arrival, the silence, and the telephone lie must react to where
Lena is and what she has already done. A fixed cinematic playlist would fail
when the player leaves, repeats an interaction, or requests help earlier.

## What Changes

- Add explicit game-specific story facts and phases, named trigger regions,
  and bounded authored sequence parameters. Support the actual location,
  interaction, elapsed-story-time, and prerequisite conditions of the slice.
- Give events durable identities and defined one-shot/repeat behavior.
  Establish deterministic resolution of competing conditions and cancellation
  of queued actions that no longer fit the current story state.
- Drive door actions, sound/caption cues, and local light state through their
  concrete interfaces while preserving immutable authored definitions.
- Coordinate story timing with pause, minimization, and input ownership.
  Outcomes do not depend on frame count, rendering recovery, or audio hardware
  callbacks; leaving a scene has an explicit continuation/interruption policy.
- Add authorable regions, references, initial facts, and supported sequence
  parameters to the editor, with validation and undo/redo. New behavior kinds
  remain explicit game code rather than arbitrary user scripts.
- Build the door/telephone slice and a small early-help branch that suppresses
  an incompatible later invitation. Final household actions arrive in P06.
- Version serialized narrative additions explicitly. No generic scripting
  language, node editor, behavior trees, or global event bus.

## Capabilities

### New Capabilities

- `narrative-progression`: Concrete story state, conditions, consequence
  resolution, and deterministic progression.
- `authored-sequences`: Named triggers, bounded authored event sequences,
  cancellation, interruption, and observable execution history.

### Modified Capabilities

- `level-persistence`: Persist narrative definitions, identities, and references.
- `level-object-placement`: Author trigger regions and scene markers.
- `level-editor`: Edit supported sequence parameters and diagnose broken links.
- `runtime-composition`: Own narrative advancement and coordinate concrete
  world/presentation actions under a defined story-time policy.

## Impact

Affects gameplay state ownership, loop timing, input gating, level validation,
and editor authoring. Keep a deterministic progression core testable without
a window, GPU, or audio device. Update gameplay and architecture documentation
with the actual phase/fact model and timing rules.

## Dependencies and Boundaries

P05; requires [P04](../add-spatial-audio-and-captions/proposal.md), including its
door prerequisite. Use the audio/text and interaction capabilities it supplies.
P09 adds durable save files; P11 adds developer-facing state inspection and
prepared episode launch. Basic event diagnostics belong here.

## Acceptance Criteria

- The telephone slice reaches the intended cue order during ordinary play,
  repeated region entry, and different render/fixed-step batch sizes.
- A completed event cannot replay merely because a region is re-entered or
  rendering recovers. Competing triggers resolve identically from equal state.
- Accepting early help cancels the conflicting queued scene; no stale voice
  subsequently calls Lena into an already-resolved danger.
- Leaving during dialogue, opening the door, pausing, and minimizing each have
  a defined, testable result without hidden elapsed danger time.
- Broken references and unsupported conditions are rejected with scene/event
  context; run deterministic progression tests and play the integrated slice.
