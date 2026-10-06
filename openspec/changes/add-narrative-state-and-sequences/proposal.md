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
- Provide a small reusable editor-authored vocabulary, not a filename-selected
  demonstration. Use explicit desired-state commands and instance-checked
  cancellation; observe player box/document actions without automating the
  player's hands or introducing object transport.

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
- `authored-interaction`: Expose accepted player interactions once, with target
  identity and actual outcome, for concrete scene consumers.
- `interactive-doors`: Add authored endpoint/lock requests while preserving
  collision, refusal and stopped-on-obstruction behavior.
- `light-switch`: Share explicit light enables with ordinary switch actions.
- `household-interactions`: Share explicit radio state and observe accepted
  box/document actions without stealing player input ownership.
- `scripted-characters`: Observe and cancel only the route instance started
  by the requesting sequence.
- `spatial-audio`: Reserve suitable sequence sources and observe/cancel only
  the cue instance started by the requesting sequence.

## Impact

Affects gameplay state ownership, loop timing, input gating, level validation,
and editor authoring. Keep a deterministic progression core testable without
a window, GPU, or audio device. Update gameplay and architecture documentation
with the supported state/event model and timing rules.

## Dependencies and Boundaries

P05; requires [P06](../archive/2026-09-26-add-household-interactions/proposal.md) and
[P07 through P07c](../archive/2026-09-09-add-character-authoring/proposal.md), including P07a's
animation and P07b's route actions, and their P04 audio/text
and P10 lighting prerequisites. Rebase on their resulting capabilities and
add modified-capability entries where event integration changes requirements.
This change owns event-driven integration of the earlier technical systems.
T4 uses neutral state transitions; actual story development follows T6.
P09 adds durable save files, P11 adds author-facing inspection/prepared scene
launch, and P12 uses the suspension policy for menus. Basic event diagnostics
and a minimal development pause control belong here, so testing does not wait
for P12. These later changes are not prerequisites for P05 acceptance.

## Acceptance Criteria

- The neutral sequence reaches its defined state/action and cue order during play
  and repeated region entry. Equivalent ordered observations and active-time
  samples produce identical dispatch and cancellation regardless of presentation;
  varied fixed-step batches additionally exercise intermediate region crossings
  and the existing capped-physics/uncapped-audio timing policies.
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
- Author a second variation through the editor, save/reopen and ordinary Play
  without runtime changes; retain semantic UI evidence separately from visual
  and listening acceptance.
