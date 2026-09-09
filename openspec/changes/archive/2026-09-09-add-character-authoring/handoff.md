# P07c acceptance handoff

## Decision

`add-character-authoring` is accepted on 2026-09-09. All 18 implementation and
acceptance tasks are complete. The independent UI-authored scene was saved,
reopened and launched through ordinary editor Play from outside the repository;
real player/door obstruction and release, moving shadows, final presentation
and the essential interaction caption have retained evidence.

The user confirmed audible output during Play. A separate per-cue listening
matrix and hardware-latency measurement were not collected; screenshots are
not acoustic measurements. The accepted P07b hardware-timing limitation remains
explicit. No implementation or validation blocker remains within this scope.

## Three-stage T2 evidence

- P07a is accepted and archived: [indexed validation](../2026-09-08-add-character-animation/indexed-validation.md),
  366 Debug tests, ten Vulkan checks and all nine Release samples passing the
  unchanged gates; its checklist was complete at 23/23.
- P07b is accepted and archived: [route/collision/audio validation](../2026-09-08-add-scripted-characters/validation.md),
  all nine Release samples and the retained behavioral/visual checks; its
  checklist was complete at 25/25. Its original physical listening and hardware
  latency limitations were explicitly accepted by the user.
- P07c is accepted and archived: [validation and observation limits](validation.md),
  [retained scene/evidence index](evidence/final-authoring/README.md), 471/471
  recorded Debug tests (including 24 real ImGui checks), and 12/12 recorded
  Vulkan smoke tests after final GPU teardown. The Vulkan run preceded the
  subsequent non-rendering empty-save-path fix; the full Debug run includes it.
  No application source changed during the final independent authoring run.

These three stages, not animation alone, establish T2/P07 acceptance. The
[roadmap](../../../../docs/ROADMAP.md) retains the predecessor evidence and the
chosen remaining order P06, then P05. P06 still requires detailed
planning/review against P04/P10/P07. P05 remains dependent on P06 and the whole
P07 chain. This handoff does not begin either dependent implementation.

## Archive boundary

The change was archived on 2026-09-09 after explicit user approval to synchronize
its `level-editor` and `level-object-placement` deltas. Both main specs were
compared with the deltas after merging, and all 28 main specs passed OpenSpec
validation. P07a/P07b main-spec changes were already synchronized. The accepted
scene, screenshots, journals, validation history and `.openspec.yaml` moved
with the change; roadmap/document links now point to the archive. This operation
did not rerun application builds, tests, Vulkan checks or physical listening.
Do not reclassify fixture F5 controls or silent schematic inspection as
ordinary-game evidence.

The editor was left open on the clean saved document with inspection stopped;
its ordinary game child exited with code 0. The final retained scene checksum
and exact process/capture provenance are in the evidence index.
