## Why

The runtime has static props but no supported animated-character workflow.
Prepare character import, clip playback, authored movement, and scene
coordination on a neutral test character before choosing the cast or story.

## What Changes

- Add a controlled animated-character export profile based on a representative
  test asset: skeleton/skin data and a bounded set of standing, walking,
  turning, and interaction clips. Determine supported transitions and scene-mark
  alignment during design without requiring a particular story action.
- Author character identities, placements, routes, scene marks, and initial
  states. Use concrete route/clip controls and bounded reactions appropriate
  to this game's small cast.
- Keep authored route movement, visible pose, collision, footsteps, dialogue,
  and interaction moments consistent. Define what happens when the player or a
  door blocks a route; actors must not slide through geometry to meet a cue.
- Exercise a test character walking between scene marks, turning, and playing
  an interaction clip with a localized sound. Integrate animated-character
  occlusion with P10's supported lighting/shadow profile.
- Add actor/route selection, properties, route/clip preview, link validation,
  and undo/redo. Own explicit run-local actor state and define restart and
  suspension behavior. P09 later reconstructs safe checkpoint actor states.
- Evolve serialized character data explicitly. Preserve resource ownership
  during repeated scene entry, animation, and presentation recovery.
- No combat states, health, generic behavior trees, crowd navigation,
  full-body player embodiment, or required facial/lip-sync framework.
  Add detailed acting only where the chosen scene/asset profile requires it.

## Capabilities

### New Capabilities

- `scripted-characters`: Authored character definitions, routes, supported
  scene actions, obstruction responses, and explicit run-local actor state.
- `character-animation`: Controlled animated asset profile and coherent
  visible motion/clip playback for the required scenes.

### Modified Capabilities

- `level-persistence`: Persist character definitions, routes, and clip links.
- `level-object-placement`: Place/select actors and their scene marks/routes.
- `level-editor`: Preview supported animation and diagnose invalid actor data.
- `physics-simulation`: Support the required character blocking and movement
  beyond the one local player while preserving physics ownership.
- `runtime-composition`: Own actor state and coordinate its simulation,
  presentation, audio, suspension, and fresh scene entry.
- `vulkan-renderer`: Present the supported animated character geometry and
  changing poses with explicit GPU lifetime.

## Impact

Affects private asset loading, animated rendering, actor gameplay, collision,
sound-source updates, lighting integration, and editor preview. Keep the static
asset profile separate where useful; P02's existing static props do not need
animation machinery. Document the character export and scene-blocking workflow.

## Dependencies and Boundaries

P07; requires
[P04](../archive/2026-09-07-add-spatial-audio-and-captions/proposal.md) and
[P10](../archive/2026-09-07-add-interior-lighting/proposal.md), including P02's static asset and
P03's door prerequisites. Rebase on P10's resulting main specs and add its
lighting capability to modified capabilities where actor occlusion changes
requirements. T2 is a neutral character/route test, independent of P05 and P09.
P05 later drives actor actions from events; P09 restores actor state. Character
identity, motivation, dialogue, and escape behavior remain later content work.

## Acceptance Criteria

- Import and preview the supported clips, then move a test character between
  authored marks at the intended scale with aligned footsteps and transitions.
- Blocking the corridor or closing a route door produces a defined pause or
  authored response without teleportation through visible blocking geometry.
- An interaction clip and associated sound originate at the correct scene mark.
  Invalid route/clip/actor references identify the affected record.
- In P10's supported cases, the animated character casts the expected shadow
  consistently with its visible pose. Static props and door lighting still work.
- Suspension freezes route/clip/audio coordination; resume does not duplicate
  an action. Fresh scene entry resets actor state and presentation recovery
  preserves it without duplicate actors or sounds.
- Run route/state/import tests and animated Vulkan smoke; manually inspect the
  neutral route, clip/lighting integration, and repeated recovery. Record the
  supported asset profile, limits, and measured frame times.
