# Runtime and Authoring Roadmap

## Purpose and planning status

Prepare this game's runtime and standalone editor for the first-person
narrative horror experience defined in [VISION.md](VISION.md) and
[GAMEPLAY.md](GAMEPLAY.md). The readiness test is an authored, playable story
with meaningful alternatives, checkpoint recovery, and a repeatable
content-production workflow.

This is a sequence of work, not a delivery-date commitment. P01 is implemented
and archived, providing interior authoring and saved-file playtesting. Its automated checks and M1
Windows desktop acceptance passed, as recorded in its
[validation record](../openspec/changes/archive/2026-09-06-add-interior-level-authoring/validation.md). The
other eleven linked changes currently capture proposals only. Their designs,
delta specs, and tasks must be developed and reviewed before implementation.
No change is implementation-ready merely because its proposal exists or
passes structural validation. The eleven proposal-only feature changes still
lack delta specs for strict OpenSpec validation. This is pending planning
work; they do not qualify for the documentation-only `skip_specs` exemption.

The named characters, locations, and episode routes below are reference
content for capability acceptance. Final names, dialogue, episode order,
duration, errands, and ending details remain content decisions. Use
representative temporary geometry, voices, and character assets to test
behavior before producing final content. This roadmap does not schedule all
final art, voice recording, writing, or historical research.

## Baseline before P01

The roadmap began with Vulkan presentation, a grounded Jolt player,
static collision, versioned level documents, a standalone editor with object
editing and undo/redo, heightfield sculpting, local lighting, a flashlight, and
one usable light switch and 19 main specifications.

Constraints captured at that baseline:

- Level version 3 requires one 97-by-97 heightfield, at most 240 axis-aligned
  solids, exactly two point lights, one packaged chair, and at most one switch.
- Spawn validation requires terrain support; direct editor placement targets
  terrain. Neither workflow serves upper-floor authoring adequately.
- The static GLB profile accepts one primitive and ignores file materials.
  Runtime meshes are flattened into immutable world-space geometry.
- Audio, game text, moving doors, narrative progression, animated characters,
  and save-game persistence are absent. Saving a level is not saving a game.
- Playtesting an authored level required replacing the executable's
  packaged prototype level and restarting.

P01 replaces the mandatory terrain/spawn and packaged-file replacement
constraints with optional terrain, named starts, surface placement, and
explicit saved-file launch. The remaining constraints belong to later changes.
These prototype constraints do not constrain the story. Update
the affected main requirements through the corresponding change's delta specs
during later planning. Preserve useful ownership and validation guarantees.

## Milestones and playable acceptance

| Milestone | Changes needed | Observable acceptance |
| --- | --- | --- |
| M1: Apartment and stairs | P01 | Author and launch a blockout with Lena's room, corridor, kitchen, rear stairs, and a lower landing. Traverse the route and start on either floor without terrain tricks. |
| M2: Telephone behind the door | P03, P04, P05 after M1 | Close and lock the room door. Hear the telephone conversation end, then hear the visitor's contradictory invitation. Door state affects sound; captions preserve the clue with audio muted. Temporary off-screen sources are sufficient. |
| M3: Errands and consequences | P06, P09 after M2 | Deliver/read a letter, perform representative household actions, and request help early. Later reactions reflect those actions. Resume a checkpoint with consistent progression and object state. |
| M4: People in the apartment | P02, P07 | The neighbor admits the visitor, he walks past the room and later appears at the authored confrontation position. Visible motion, footsteps, dialogue, doors, and checkpoint restoration agree. |
| M5: Complete temporary-content story | P08; P11 available before assembling the full sequence | Play the ordinary escape and an early-help route through an appropriate epilogue. Alternate exploration order and repeated actions do not trap progression. |
| M6: Atmosphere with representative final assets | P10, integrated with P02/P04/P07 | Assess a furnished control apartment for readable darkness, expected light blocking, character silhouettes, important sound cues, and frame-time stability. |
| M7: Content-production readiness | P12 and all earlier milestone acceptance | Author another episode of this game with existing tools, diagnose a broken link, package it, and complete it using menus, settings, and checkpoint resume from a working directory outside the source tree. |

Asset and lighting investigation can start after M1; their final visual
acceptance uses the integrated scene. P11 follows the first meaningful
checkpoint and must be available before M5 content assembly. Neither advanced
lighting nor final character art blocks the M2 behavior test.

## Proposal index and direct prerequisites

Prerequisites below are planning/implementation dependencies, not native
OpenSpec cross-change scheduling. Resolve them before implementing the
dependent change and rebase its artifacts on the resulting main specs.

| ID | Proposal | Direct prerequisites |
| --- | --- | --- |
| P01 | [Interior level authoring](../openspec/changes/archive/2026-09-06-add-interior-level-authoring/proposal.md) | None |
| P02 | [Authored scene assets](../openspec/changes/add-authored-scene-assets/proposal.md) | P01 |
| P03 | [Interactive doors](../openspec/changes/add-interactive-doors/proposal.md) | P01 |
| P04 | [Spatial audio and captions](../openspec/changes/add-spatial-audio-and-captions/proposal.md) | P03 |
| P05 | [Narrative state and sequences](../openspec/changes/add-narrative-state-and-sequences/proposal.md) | P04 |
| P06 | [Household interactions](../openspec/changes/add-household-interactions/proposal.md) | P05 |
| P07 | [Scripted characters](../openspec/changes/add-scripted-characters/proposal.md) | P02, P09 |
| P08 | [Escape and help outcomes](../openspec/changes/add-escape-and-help-outcomes/proposal.md) | P07, P11 |
| P09 | [Checkpoint resume](../openspec/changes/add-checkpoint-resume/proposal.md) | P06 |
| P10 | [Interior lighting](../openspec/changes/add-interior-lighting/proposal.md) | P02, P05 |
| P11 | [Story playtest tools](../openspec/changes/add-story-playtest-tools/proposal.md) | P09 |
| P12 | [Game session and packaging](../openspec/changes/add-game-session-and-packaging/proposal.md) | P08, P10 |

A valid implementation order is P01, P03, P04, P05, P06, P09, P11, P02,
P07, P08, P10, P12. P02 can move earlier after P01; P10 can proceed after
P02/P05. Proposal numbering identifies scope, not a forced serial order.

## Shared decisions

### Authored definitions and running state

Keep level definitions immutable during play. Runtime-owned state represents
door motion, item locations, narrative facts, actor state, and other actual
changes. Rendering, collision, and audio consume coherent presentations of
that state. A moving door does not require rewriting its source level or
reuploading the whole static world.

Introduce durable identifiers when a real record needs references or recovery:
entry points, doors, sound cues, story markers, items, and actors. Transient
editor selection handles, array positions, and native resource handles are
not save identities. Do not create a general entity registry for this purpose.

P01 owns the initial interior-format transition. Each later change owns any
additional format evolution it needs and must state compatibility with the
then-current format. Opening never silently rewrites authored work. Do not
preassign every future format version or require indefinite support for all
prototype formats.

### Agency, timing, and recovery

The delivery of the letter may affect recognition; it must not become a
mandatory key to every successful ending. An accepted early request for help
must alter or cancel incompatible danger sequences. The story must not reject
help solely to force the ordinary pursuit scene.

Use explicit game-specific progression and sequence logic with bounded author
parameters. Define what happens when the player leaves, repeats an action,
opens a door during dialogue, or triggers competing conditions. Pausing and
minimizing must not silently advance danger or separate captions from speech.
Hardware audio completion and rendering frequency must not decide outcomes.

P09 restores safe, authored checkpoint boundaries. Later actor and outcome
changes extend reconstruction for their actual state and add resume checks.
Free saving during arbitrary animation or dialogue is not assumed.

### Authoring and technical scope

Each feature proposal includes its own editor fields, references, validation,
undo/redo where editable, and representative runtime exercise. P11 adds story
diagnostics and repeatable episode setup; it does not postpone basic authoring
until the end. Playtesting uses a separate game process and an explicitly
selected saved level; unsaved edits require an explicit save decision.

Favor authored routes and concrete behaviors for the small cast. No combat,
general scripting language, behavior-tree framework, general inventory, ECS,
render graph, or streaming architecture is required by this plan. Existing
jump, sprint, crouch, and flashlight behavior remains a prototype choice;
revise it only when traversal and presentation tests establish a game need.

Choose the supported model/material/animation profile using representative
exports before committing to an importer design. Choose the audio dependency
and light-blocking/shadow method during their designs. Measure the furnished
scene before adding substantial performance machinery.

### OpenSpec coordination

Proposal capability lists describe new ownership and changes to main specs
that exist at this planning baseline. A dependency's new capability is reused,
not declared again as new by its consumers. When a dependency is implemented,
review its new requirements and add any necessary modified-capability entries
to the dependent proposal before writing delta specs.

Several proposals intentionally touch level persistence, rendering, runtime
composition, and the editor. Their deltas must be authored against the latest
main specs in dependency order, not independently applied against version 3.
Keep this index and the proposal links usable when changes are archived.

## Decisions to resolve during detailed planning

| Decision | Needed before | Starting assumption |
| --- | --- | --- |
| Playable footprint of the bread errand and outside help | Expanding M1 beyond the apartment/stairs blockout | One compact authored location; a shop or street can remain a scene transition/content choice. No world streaming requirement. |
| Degree of visible acting and required clips | P07 design | The reference encounter uses a visible visitor; temporary models validate blocking. Detailed facial animation and full first-person hand animations need separate scene evidence. |
| Contact/failure behavior during the escape | P08 design | Short authored danger sequence. Add a failure/retry condition only if the encounter needs it; no health/damage model. |
| What determines Anna Petrovna's outcome | P08 design | Meaningful actions and explicit story milestones. Do not assume an invisible global countdown or punish ordinary exploration. |
| Exact checkpoint locations and retention | P09 design | Safe authored boundaries sufficient to repeat dangerous or important episodes without replaying the whole evening. |
| Visual target and performance budget | P02/P10 design and M6 evaluation | A representative furnished interior, specified target hardware, resolution, and measured frame times. Choose rendering techniques and visual fidelity from demonstrated scene requirements. |

## Validation and readiness evidence

Follow [DEVELOPMENT.md](DEVELOPMENT.md) for implementation checks. In addition
to affected builds and tests, retain small, playable acceptance scenes for the
milestones above. Validate event order, cancellation, idempotent interactions,
checkpoint reconstruction, invalid references, and failed loads through
observable behavior. Run Vulkan smoke validation for relevant rendering,
resource, window, and lifetime changes; listen to audio and manually assess
its captions and spatial cues. Muted playthroughs must retain essential clues.

Final readiness requires the actual packaged routes and authoring workflow,
including a pause/minimize/resume exercise and both help outcomes. Passing unit
tests alone does not establish that pacing, acting, atmosphere, or production
iteration meets the game's needs. Record unavailable validation explicitly.
