## Why

An animated model alone cannot walk an authored scene coherently. P07 needs
bounded route actions whose accepted position agrees with collision, visible
motion, shadows and localized sound before later events can drive characters.

## What Changes

- Add zero to four named character placements, durable scene marks and ordered
  routes using the P07a mannequin/clip catalog.
- Implement explicit idle, turn, walk, blocked, interaction, completed and
  canceled run-local results. Initial routes run once; later concrete requests
  can start or cancel a route without a scripting system.
- Advance only accepted route movement: the player, static geometry, other
  actors and accepted door leaves block actors; actors also block the player,
  door sweeps and interaction visibility. Actors wait and retry automatically;
  obstructed doors retain P03's explicit reactivation policy.
- Synchronize walking phase/footsteps with accepted travel and fire one
  localized cue at the supported interaction marker. Define cancellation,
  cue contention, suspension and fresh-run behavior.
- **BREAKING**: write level v9 with required character collections; read exact
  v2-v8 shapes without rewriting their source and normalize them to no actors.
- Package a neutral route fixture and capacity/obstruction checks. Preserve
  actor records through editor Open/Save/Play and show initial presentation;
  dedicated actor authoring and interactive preview are P07c.

## Capabilities

### New Capabilities

- `scripted-characters`: Bounded authored actor/mark/route data, concrete
  actions, accepted movement, sounds and explicit run-local state.

### Modified Capabilities

- `level-persistence`: Character format, compatibility, validation and round trips.
- `physics-simulation`: Bounded actor collision and coherent actor/player/door steps.
- `interactive-doors`: Character obstruction preserves no-crushing door behavior.
- `authored-interaction`: Actor proxies block door/switch visibility.
- `runtime-composition`: Own and coordinate character state, audio and suspension.
- `level-editor`: Preserve, initially display and preflight character-bearing files.

## Impact

Affects world codec/validation, concrete gameplay controllers, Jolt integration,
runtime composition, existing cue coordination, selected resource preparation
and minimal editor compatibility. Reuses P07a sampling and character shadows;
does not change the static prop profile or add physics-derived render meshes.

## Dependencies and Boundaries

P07b retains the original `add-scripted-characters` change ID.
[P07a](../archive/2026-09-08-add-character-animation/proposal.md) is implemented
and archived; recheck its resulting main specs and handoff before applying these artifacts.
[P07c](../add-character-authoring/proposal.md) completes the editor workflow and
T2. P05 depends on the full P07 chain; P09 later reconstructs safe checkpoint
states. Neither is required to run this neutral fixture.

Routes support ordinary grounded walking on authored traversable surfaces.
No navmesh, obstacle avoidance, autonomous door operation, combat, generic AI,
general animation graph, root motion, prop manipulation, dialogue sequencing,
save-game serialization or final cast/story is introduced.

## Acceptance Criteria

- The actor turns, walks between marks and performs Interact at the final
  mark, with feet at the supported surface and sound at the visible action.
- Player, thin wall, prop, stair, closed/closing/reversed door and second actor
  cases produce deterministic accepted motion without tunneling or crushing.
- Waiting produces no walking-in-place footsteps or early arrival/action;
  clearing the route permits retry without teleporting.
- Cancellation, restart, mute, no audio device, explicit suspension and
  minimize/restore preserve the specified event identities and timing.
- Runtime/initial editor views agree; exact legacy loads, canonical v9 saves,
  malformed records, partial construction and repeated recovery are checked.
- Retain full-route visual/listening observations, animated Vulkan validation
  and one/four-actor release measurements. This stage alone does not close T2.
