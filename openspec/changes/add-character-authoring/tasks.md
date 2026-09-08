## 1. Character document commands

- [ ] 1.1 Confirm P07a/P07b acceptance and rebase on their resulting main specs; verify editor work reuses the delivered v9 schema, sampler and selected resource preflight.
- [ ] 1.2 Add actor/mark/route selection and concrete properties with preserved unknown references; verify each record lists/selects once and finite invalid edits remain repairable with correct dirty state.
- [ ] 1.3 Implement add/duplicate/delete, independent actor-start-mark cloning and combined capacity checks; verify one-step undo/redo, cleared duplicate route/source links and no partial mutation on capacity failure.
- [ ] 1.4 Implement rename/link/order edits including incoming audio-source references; verify all consumers update atomically, deletion leaves broken links and undo restores references/selection/saved revision.

## 2. Placement and inspection

- [ ] 2.1 Add catalog visual/proxy bounds, mark/facing handles and ordered route overlays; verify consistent nearest picking, missing endpoint handling and list repair access without invented coordinates.
- [ ] 2.2 Extend surface placement to actor initial marks and scene marks with visible shared consumers; verify upper-floor feet placement, unsuitable nearer faces, UI capture/cancel and one-gesture history.
- [ ] 2.3 Add explicit silent clip snapshot playback, pause/restart/seek and clip transition controls; verify identical sampler results, unchanged authored data and no event/audio side effects.
- [ ] 2.4 Add labeled schematic route playback and current segment/mark/final-action inspection; verify ordered motion/facing and unresolved-link refusal while retaining the editor's no-physics boundary.
- [ ] 2.5 Invalidate snapshots on edits/history/replacement/selection/minimize/Play and arbitrate against audio audition; verify neither path silently restarts or leaves stale pending commands.

## 3. Preview resources and Play

- [ ] 3.1 Integrate pose-only changing updates and coherent transactional character scene replacement; verify playback does not rebuild static resources and failed replacement preserves a labeled usable preview.
- [ ] 3.2 Verify selected character/audio preflight within dirty Save and Play/Cancel, saved-file freshness and Unicode native launch; add behavioral process cases showing zero child on each failed/canceled path and one child on success.
- [ ] 3.3 Exercise preview pause, resize, format recovery, minimize/edit replacement and shutdown in editor Vulkan smoke; verify state/lifetime behavior and validation after final GPU destruction.
- [ ] 3.4 Drive real ImGui controls and shortcuts through the UI test path; verify focus/capture, field commits, selection changes and undo/redo do not accidentally move the camera or mutate preview-only values.

## 4. Authoring acceptance and handoff

- [ ] 4.1 Build and retain a second neutral scene using only editor operations; verify actor/marks/routes/audio/lights/door can be authored, saved, reopened and launched without code or JSON edits.
- [ ] 4.2 Inspect that saved scene's real obstruction/release, facing/action sound/captions and matching actor shadows; record the authoring/Play sequence and observed preview limits in validation.md.
- [ ] 4.3 Follow docs/DEVELOPMENT.md: configure/build game and editor, run affected deterministic/UI/process tests and Vulkan smoke, and validate any changed shaders; record commands/results and unavailable checks.
- [ ] 4.4 Update implemented editor/asset workflow documentation, review git diff and run strict OpenSpec validation; verify the docs distinguish schematic inspection from physical Play.
- [ ] 4.5 Record the T2/P07 acceptance decision and prepare the handoff for the subsequent archive workflow; verify ROADMAP/dependent readiness refers to evidence from all three stages before P06/P05 starts.
