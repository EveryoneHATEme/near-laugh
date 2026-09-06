## Why

A playable sequence is not yet a practical game build or a dependable content
workflow. The player needs to start, pause, continue, adjust essential
settings, and complete the story from a package containing all required assets.

## What Changes

- Add the concrete game-session flow: new game, continue from a compatible
  checkpoint, pause/resume, return to menu, and orderly exit. A new game or
  menu transition has explicit progress/continuation behavior.
- Provide the essential control, display, audio, and subtitle settings chosen
  for this desktop game, including mouse sensitivity and readable captions.
  Persist preferences independently of checkpoints.
- Define menu/document/gameplay input ownership and cursor transitions.
  Pause and minimization preserve narrative/audio/animation timing without
  queued world interactions appearing on resume.
- Package the selected game content, shaders, models/materials, character
  clips, sounds, text, and fonts with validated references. Produce useful
  missing/incompatible-content diagnostics before entering a broken session.
- Keep game saves/preferences and development playtest setups separate from
  packaged assets. The game must run independently of editor binaries and
  from a working directory outside the repository.
- Verify the author-edit-save-playtest-package workflow by authoring another
  episode with existing capabilities, then completing both principal help
  routes with continuation and the corresponding epilogues.
- This change adds no new story mechanics, platform targets, storefront
  integration, online services, or general-purpose distribution system.

## Capabilities

### New Capabilities

- `game-session`: Player-facing session flow, pause/input ownership,
  continuation, and persistent essential preferences.
- `game-content-packaging`: Complete runnable game resources, package
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

P12; requires [P08](../add-escape-and-help-outcomes/proposal.md) and
[P10](../add-interior-lighting/proposal.md), transitively including all other
roadmap changes. Final art/voice/content production remains a separate work
stream; representative assets can establish runtime/tool readiness.

## Acceptance Criteria

- Start a new game, pause during an important cue, resume, exit, and continue
  from a checkpoint without changed consequences or duplicate/stale actions.
- Change audio/text/control settings and restart; preferences persist and
  essential clues remain accessible with audio muted.
- Run the packaged game without the editor or source-tree working directory.
  Missing required resources and incompatible continuation are diagnosed.
- Complete ordinary escape and early help through the correct epilogue,
  including a process restart/checkpoint resume and minimize/restore exercise.
- Author and package another supported episode through the documented workflow
  without new engine code. Run affected builds/tests and game/editor Vulkan
  smoke, review diffs, and record manual route and production-workflow evidence.
