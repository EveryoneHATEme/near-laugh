## Why

Technical scene capabilities need a usable session flow and a dependable
editor-to-package workflow before story development. A neutral scene must run
with menus, settings, and continuation from a self-contained package.

## What Changes

- Add the concrete game-session flow: new game, continue from a compatible
  checkpoint, pause/resume, return to menu, and orderly exit. A new game or
  menu transition has explicit progress/continuation behavior.
- Provide the essential control, display, audio, and subtitle settings chosen
  for this desktop game, including mouse sensitivity and readable captions.
  Persist preferences independently of checkpoints.
- Define menu/document/gameplay input ownership and cursor transitions.
  Pause and minimization preserve event/audio/animation timing without
  queued world interactions appearing on resume.
- Package the selected game content, shaders, models/materials, character
  clips, sounds, text, and fonts with validated references. Produce useful
  missing/incompatible-content diagnostics before entering a broken session.
- Keep game saves/preferences and development playtest setups separate from
  packaged assets. The game must run independently of editor binaries and
  from a working directory outside the repository.
- Verify the author-edit-save-playtest-package workflow by authoring a second
  neutral scene within the supported profile, then exercising its interactions,
  events, characters, and checkpoint continuation without new runtime code.
- This change adds no new story mechanics, platform targets, storefront
  integration, online services, or general-purpose distribution system.

## Capabilities

### New Capabilities

- `game-session`: Player-facing session flow, pause/input ownership,
  continuation, and persistent essential preferences.
- `game-content-packaging`: Complete selected runnable resources, package
  diagnostics, and repeatable content-to-build validation.

### Modified Capabilities

- `player-input`: Define menu/settings/exploration transitions and suppression
  of held or stale world actions under the selected session controls.
- `player-controller`: Apply session pause/control policy and the chosen
  configurable look settings.
- `runtime-composition`: Own session transitions, preferences, selected content,
  checkpoint entry, and coherent paused/resumed subsystem behavior.
- `level-editor`: Complete the documented author-to-playtest-to-package handoff
  without making the editor a game runtime dependency.
- `vulkan-renderer`: Present the supported game menu/settings requests and
  retain correct presentation/lifetime behavior across session transitions.

## Impact

Affects launcher/runtime configuration, game UI, settings persistence, cursor
policy, package resource validation, CMake resource staging, and development
documentation. Reuse the existing executable-relative resource-root boundary.
Record supported controls, save/preferences locations, and build requirements.

## Dependencies and Boundaries

P12 starts session/menu/settings work after
[P09](../add-checkpoint-resume/proposal.md), whose prerequisites provide all
supported mutable scene systems. This portion supports T5.
Final packaging and authoring-workflow acceptance also require
[P11](../add-story-playtest-tools/proposal.md) and all T1-T5 checks. P11 uses
P09's explicit resume/setup entry and does not depend on P12 menus.

Keep both portions in this change, with their gates recorded in detailed
tasks. Rebase remaining artifacts against P11's integrated specs before final
acceptance; accept/archive P12 only when both portions and T6 pass. P08 and
actual story content are not prerequisites. Story development starts after T6;
representative assets and neutral scenes establish this technical readiness.

## Acceptance Criteria

- Start a new game, pause during an important cue, resume, exit, and continue
  from a checkpoint without changed scene state or duplicate/stale actions.
- Change audio/text/control settings and restart; preferences persist and
  essential clues remain accessible with audio muted.
- Run the packaged game without the editor or source-tree working directory.
  Missing required resources and incompatible continuation are diagnosed.
- Exercise a neutral scene containing supported lights, audio/captions,
  character movement, object interactions, and cancellable events, including a
  process restart/checkpoint resume and minimize/restore exercise.
- Author and package a second supported neutral scene through the documented
  workflow without new runtime code. Use P11 to launch a prepared state and
  diagnose/repair a deliberate broken link. Run affected builds/tests and
  game/editor Vulkan smoke, review diffs, and record manual session, packaged
  scene, and authoring-workflow evidence. No ending or plot is required.
