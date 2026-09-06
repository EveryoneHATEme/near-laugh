## Why

The reference encounter needs a visitor who first appears ordinary and later
occupies a threatening position in a familiar corridor. Audio-only cues can
validate the first slice, but visible acting requires supported character
assets, motion, and explicit coordination with the scene.

## What Changes

- Add the controlled animated-character export profile required by
  representative cast assets: skeleton/skin data and a bounded set of clips
  for standing, walking, turning, and the actual door/telephone scene actions.
  Determine required clip transitions and interaction alignment during design.
- Author character identities, placements, routes, scene marks, and initial
  roles/states. Use concrete scripted behavior and bounded reactions for this
  apartment's small cast.
- Keep authored route movement, visible pose, collision, footsteps, dialogue,
  and interaction moments consistent. Define what happens when Lena or a door
  blocks a route; actors must not slide through blocking geometry to meet a cue.
- Integrate the neighbor opening the shared entrance, the visitor passing
  Lena's room, and his later confrontation/telephone positions.
- Add actor/route selection, properties, route/clip preview, link validation,
  and undo/redo. Extend checkpoint reconstruction for the supported actor state.
- Evolve serialized character data explicitly. Preserve resource ownership
  during repeated scene entry, animation, and presentation recovery.
- No combat states, health, generic behavior trees, crowd navigation,
  full-body player embodiment, or required facial/lip-sync framework.
  Add detailed acting only where the chosen scene/asset profile requires it.

## Capabilities

### New Capabilities

- `scripted-characters`: Authored cast, routes, scene actions, obstruction
  responses, and recoverable concrete actor state.
- `character-animation`: Controlled animated asset profile and coherent
  visible motion/clip playback for the required scenes.

### Modified Capabilities

- `level-persistence`: Persist character definitions, routes, and clip links.
- `level-object-placement`: Place/select actors and their scene marks/routes.
- `level-editor`: Preview supported animation and diagnose invalid actor data.
- `physics-simulation`: Support the required character blocking and movement
  beyond the one local player while preserving physics ownership.
- `runtime-composition`: Own actor state and coordinate its simulation,
  presentation, audio, and checkpoint reconstruction.
- `vulkan-renderer`: Present the supported animated character geometry and
  changing poses with explicit GPU lifetime.

## Impact

Affects private asset loading, animated rendering, actor gameplay, collision,
sound-source updates, save reconstruction, and editor preview. Keep the static
asset profile separate where useful; P02's existing static props do not need
animation machinery. Document the character export and scene-blocking workflow.

## Dependencies and Boundaries

P07; requires [P02](../archive/2026-09-06-add-authored-scene-assets/proposal.md) and
[P09](../add-checkpoint-resume/proposal.md), including their story/audio/door
prerequisites. P08 owns the escape encounter; this change supplies its concrete
actors. Confirm visibility/clip requirements before choosing animation details.

## Acceptance Criteria

- The neighbor admits the visitor and he passes the room at a plausible scale,
  using visible movement aligned with footsteps and the greeting.
- Blocking the corridor or closing a route door produces a defined pause or
  authored response without teleportation through visible blocking geometry.
- A telephone action and associated voice originate at the correct scene mark.
  Invalid route/clip/actor references identify the affected record.
- Resume a checkpoint with the correct cast/phase and no duplicate actor,
  greeting, or incompatible pending action.
- Run route/state/import/reconstruction tests and animated Vulkan smoke;
  manually inspect the integrated visitor sequence and repeated recovery.
