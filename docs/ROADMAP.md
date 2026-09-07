# Runtime and Authoring Roadmap

## Purpose and planning status

Prepare this game's runtime and standalone editor for the first-person
narrative horror experience defined in [VISION.md](VISION.md) and
[GAMEPLAY.md](GAMEPLAY.md). Complete the supported technical capabilities and
the authoring workflow before developing the story. Technical readiness means
the author can build, inspect, play, save/resume, and package a small scene
using the supported lighting, audio, characters, interactions, and events
without adding runtime code for that scene.

This is a sequence of work, not a delivery-date commitment. P01 is implemented
and archived, providing interior authoring and saved-file playtesting. Its automated checks and M1
Windows desktop acceptance passed, as recorded in its
[validation record](../openspec/changes/archive/2026-09-06-add-interior-level-authoring/validation.md).
P02 and P03 are implemented and archived: generated interactive doors and
selected apartment assets share the final v6 level format. Their validation
records retain the successful combined build, 281 debug tests and seven Vulkan
smoke tests, agent desktop observations, and user-confirmed manual acceptance
on 2026-09-06. Main specs were synchronized in order P03 then P02, with each
change archived before the next sync. P04 now implements version-7 audio
authoring, spatial playback, Russian captions, and a separate telephone fixture.
P04 was accepted by the user and archived on 2026-09-07. Its
[validation record](../openspec/changes/archive/2026-09-07-add-spatial-audio-and-captions/validation.md)
retains 322 passing debug tests, eight passing Vulkan smoke tests, qualitative
user acceptance and the unavailable quantitative latency/drift measurements.
The remaining eight linked changes
capture proposals only; their designs, delta specs, and tasks must be developed
and reviewed before implementation. Structural validation alone does not
establish implementation readiness. Those proposal-only feature changes do
not qualify for the documentation-only `skip_specs` exemption.

The remaining technical changes use neutral acceptance scenes. A test document,
movable household object, character route, or cancellable sound sequence does
not establish a plot. Names, dialogue, relationships, errands, episode order,
danger, and endings are deferred to story development after technical readiness.
P08 retains earlier escape/help ideas as draft material for that later stage.
Existing archived scene names and the P04 telephone fixture remain historical
acceptance evidence, without making their story mandatory.

Use representative geometry, materials, voices, and animated assets to expose
technical constraints. This roadmap prepares this game's bounded capabilities;
it does not promise support for every future mechanic or schedule all final art,
voice recording, writing, or historical research. A later story requirement
outside the supported profile needs its own concrete scope review.

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

P01 replaced the mandatory terrain/spawn and packaged-file replacement
constraints with optional terrain, named starts, surface placement, and
explicit saved-file launch. P02/P03 added the selected material/asset profile
and moving doors; P04 added audio and captions. The current format is v7.
The two-light/single-switch limit, character animation, additional interactions,
events, and save-game/session support belong to the remaining technical work.
Update affected main requirements through each change's delta specs against
the then-current implementation. Preserve useful ownership and validation
guarantees without retaining obsolete prototype limits.

## Technical milestones and acceptance

M1 (apartment and stairs) and the P02/P03/P04 acceptance records above remain
completed history. T1-T6 replace the unimplemented story-led M2-M7 milestones.

| Milestone | Changes needed | Observable acceptance |
| --- | --- | --- |
| T1: Interior lighting | P10 | Author several local lights/switches and ambient values in a furnished control interior. Check wall/door light blocking, supported shadows, readable darkness, editor/runtime agreement, and measured frame times. |
| T2: Animated characters | P07 | Import a representative character, preview the supported clips, and walk an authored route. Player/door obstruction, accepted motion, footsteps, and supported character shadows agree. |
| T3: Object interactions | P06 | Read a test document, carry/place a supported object, and operate a household prop. Repeated actions, blocked placement, input transitions, and editor undo/redo preserve coherent object state. |
| T4: Events and sequences | P05 | Enter a region to change a light, start a captioned sound, and request a character action. Re-entry does not replay a completed event; a competing test condition cancels pending actions. Pause/minimize and different frame batches preserve the defined result. |
| T5: Session and checkpoint recovery | P09, P12 session work | Start a new test session, pause/resume, change settings, exit, and continue from a safe checkpoint. Player, object, actor, light, door, and event state agree; failed saves retain the last usable checkpoint. |
| T6: Authoring and packaging readiness | P11, P12 final acceptance; T1-T5 | Author a second neutral scene using existing capabilities, launch a prepared setup, diagnose and repair a broken link, then package and run it outside the source tree without the editor. Exercise menus, settings, muted captions, restart, and checkpoint resume. |

Each milestone proves an actual capability without requiring final dialogue,
relationships, or outcomes. T6 closes the technical stage. Story development
then selects scenes and mechanics within the supported profile and revisits
P08's draft before planning or implementing its content.

## Proposal index and direct prerequisites

Prerequisites below are planning/implementation dependencies, not native
OpenSpec cross-change scheduling. Resolve them before implementing the
dependent change and rebase its artifacts on the resulting main specs. Later
integration checks have explicit owners below; they do not block acceptance
of an earlier capability in its supported scope.

| ID | Proposal | Direct prerequisites |
| --- | --- | --- |
| P01 | [Interior level authoring](../openspec/changes/archive/2026-09-06-add-interior-level-authoring/proposal.md) | None |
| P02 | [Authored scene assets](../openspec/changes/archive/2026-09-06-add-authored-scene-assets/proposal.md) | P01 |
| P03 | [Interactive doors](../openspec/changes/archive/2026-09-06-add-interactive-doors/proposal.md) | P01 |
| P04 | [Spatial audio and captions](../openspec/changes/archive/2026-09-07-add-spatial-audio-and-captions/proposal.md) | P03 |
| P05 | [Event state and sequences](../openspec/changes/add-narrative-state-and-sequences/proposal.md) | P06, P07 |
| P06 | [Household object interactions](../openspec/changes/add-household-interactions/proposal.md) | P04, P10 |
| P07 | [Scripted character and animation support](../openspec/changes/add-scripted-characters/proposal.md) | P04, P10 |
| P08 | [Deferred escape/help story draft](../openspec/changes/add-escape-and-help-outcomes/proposal.md) | T6 accepted, then story scope reviewed; P12 supplies the technical prerequisites |
| P09 | [Checkpoint resume](../openspec/changes/add-checkpoint-resume/proposal.md) | P05 |
| P10 | [Interior lighting](../openspec/changes/add-interior-lighting/proposal.md) | P02, P03 |
| P11 | [Story playtest tools](../openspec/changes/add-story-playtest-tools/proposal.md) | P09 |
| P12 | [Game session and packaging](../openspec/changes/add-game-session-and-packaging/proposal.md) | P09 to start session work; P11 also required for final packaging/workflow acceptance |

P01, P03, P02, P04 are already archived. The selected remaining order is:

```text
P10 --> P07 --> P06 --> P05 --> P09
                               |
                               v
                         P12 (session)
                               |
                               v
                              P11
                               |
                               v
                     P12 (packaging, T6)
                               |
                               v
                     Story development / P08
```

This is the chosen work order, not a claim that every adjacent pair is a hard
dependency. P12 remains one change: its session portion supports T5, and it is
accepted/archived only after P11 and T6 packaging checks. P11 uses P09's explicit
resume/setup entry and does not depend on P12's menus. Rebase P12's remaining
artifacts after P11 is integrated; do not create a circular dependency.

P10 is the next implementation candidate; develop its design, delta specs, and
tasks first. P10 validates static geometry and moving doors. P07 owns adding
and checking animated-character occlusion in that lighting profile. P05 owns
event-driven light, object, door, audio, and actor integration. P09 restores
all of those already implemented states. P12 integrates their common session
pause and settings behavior. No prerequisite plot is needed for these checks.

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

### State, timing, and recovery

Use explicit game-specific progression and sequence logic with bounded author
parameters. Define what happens when the player leaves, repeats an action,
opens a door during a cue, or triggers competing conditions. Use neutral tests
for cancellation and alternate action order. Pausing and minimizing must not
silently advance sequences or separate captions from speech. Hardware audio
completion and rendering frequency must not decide state transitions.

Before the session UI exists, each changing system defines and tests its
suspension behavior through a minimal development control. P05 coordinates
event time with player, door, actor, audio, and reading/input state; P12 uses
that policy for the player-facing pause/menu flow. Cursor release, document
reading, and an actual session pause must have explicit, distinct policies.

P09 restores safe, authored checkpoint boundaries, including existing actor,
item, event, door, and light state. It reconstructs suitable animation and
ambience at those boundaries without serializing arbitrary runtime internals.
Any later story-specific state extends reconstruction when introduced. Free
saving during arbitrary animation or dialogue is not assumed.

### Authoring and technical scope

Each feature proposal includes its own editor fields, references, validation,
undo/redo where editable, and representative runtime exercise. P11 adds event
diagnostics and repeatable test-scene setup; it does not postpone basic authoring
until the end. Playtesting uses a separate game process and an explicitly
selected saved level; unsaved edits require an explicit save decision.

Favor authored routes and concrete behaviors for the small cast. No combat,
general scripting language, behavior-tree framework, general inventory, ECS,
render graph, or streaming architecture is required by this plan. Existing
jump, sprint, crouch, and flashlight behavior remains a prototype choice;
revise it only when traversal and presentation tests establish a game need.

Choose the supported model/material/animation profile using representative
exports before committing to an importer design. P04 selected pinned miniaudio
for audio. Choose the light-blocking/shadow method during its design. Measure the furnished
scene before adding substantial performance machinery.

### OpenSpec coordination

Existing change IDs and capability paths are retained for continuity. In the
technical stage, names such as narrative progression and story playtesting
refer to supported state/event mechanisms, not an approved plot.

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
| Light/shadow profile and performance budget | P10 design and T1 | A furnished control interior, explicit target hardware/resolution, measured frame times, and documented supported sources/occluders. |
| Animated export profile and clip transitions | P07 design and T2 | A representative test character with standing, walking, turning, and a supported interaction clip; detailed acting needs separate evidence. |
| Supported object actions and placement rules | P06 design and T3 | Read a document, carry/place an object, and operate a household prop using bounded concrete actions. |
| Supported event conditions/actions and timing policy | P05 design and T4 | Neutral tests for region entry, object state, elapsed active time, one-shot execution, interruption, and cancellation. No script language. |
| Checkpoint boundaries, retention, and compatibility | P09 design and T5 | Safe named test-scene boundaries that restore all supported mutable state; actual story checkpoint locations are chosen later. |
| Session controls, preferences, and packaging scope | P12 design and T5/T6 | Essential desktop controls/settings and all selected resources; final acceptance includes P11's authoring workflow. |
| Characters, locations, errands, dialogue, danger, and endings | Story development after T6 | Earlier story examples are draft material. Select and review the actual story before detailing P08; do not derive mechanics solely from those examples. |

## Validation and readiness evidence

Follow [DEVELOPMENT.md](DEVELOPMENT.md) for implementation checks. In addition
to affected builds and tests, retain small, playable acceptance scenes for the
milestones above. Validate event order, cancellation, idempotent interactions,
checkpoint reconstruction, invalid references, and failed loads through
observable behavior. Run Vulkan smoke validation for relevant rendering,
resource, window, and lifetime changes; listen to audio and manually assess
its captions and spatial cues. Muted playthroughs must retain essential clues.

Technical readiness requires T1-T6, including a second authored neutral scene,
the complete editor-to-package workflow, and pause/minimize/resume and process
restart/checkpoint checks. Record manual visual/audio observations, measured
performance, supported limits, and unavailable validation explicitly. Passing
unit tests alone does not establish technical or production-workflow readiness.

Story development follows this gate. Its later acceptance evaluates the chosen
content's progression, alternatives, pacing, acting, and atmosphere. A technical
fixture passing does not establish that the eventual story works.
