## 1. Baseline and acceptance preparation

- [x] 1.1 Recheck the integrated P06/P07 baseline, current main specs, v10 resources and applicable instructions before implementation; record any drift from design.md and resolve it before dependent edits. Verify the selected change remains strictly valid and retain the initial working-tree status.
- [x] 1.2 Create a compact T4 validation record with the neutral scene, independent authored variation, expected normal/canceled/failed results and separate automated, GPU, semantic, visual/listening and performance evidence slots. Verify every acceptance scenario in the two new capability specs has an assigned check and unavailable checks cannot appear as passes.

## 2. Definitions, validation and level v11

- [x] 2.1 Add the bounded typed facts, regions, triggers, predicates, events and step definitions to the existing world module. Verify all supported kinds, bounds, initial values and invalid per-kind parameters with device-free definition tests; keep backend types and runtime execution state out of authored data.
- [x] 2.2 Add shared typed-reference and ownership validation, including self event references, scene-entry repetition and ineligible/looping sequence sources. Verify contextual failures and safely repairable missing links without constructing physics, audio devices or GPU resources.
- [x] 2.3 Implement exact v11 decoding and canonical serialization with v2-v10 normalization to empty narrative definitions. Verify negative cross-version shapes, every existing migration, clean opening without rewriting, v10 content/order preservation and byte-identical v11 round trips containing every trigger/predicate/step kind.
- [x] 2.4 Update current packaged scenes, current-format generators and save-version notices; provide an idempotent v10-to-v11 conversion preserving existing v11 narrative content. Verify generator determinism, current resource preflight and untouched historical compatibility fixtures; retain originals for older-build use.

## 3. Concrete subsystem commands and observations

- [x] 3.1 Add explicit shared light and radio enable commands without player-input simulation. Verify repeated desired values are no-ops, radio instance/offset preservation, ordinary switch/radio toggles after event changes, and radio commands while reading or carrying.
- [x] 3.2 Add authored door endpoint/lock requests with already-desired checks and concrete refusal results. Verify locked/open/moving/unlockable cases, repeated active targets, sweep safety, stopped obstruction, explicit same-direction retry and no rollback on event cancellation; retain player reach/side/toggle regressions.
- [x] 3.3 Add expected-instance cancellation and completion observation to actor route requests. Verify new-start ownership, busy/already-active non-ownership, blocked cancel, superseded-instance safety and that unrelated character sounds remain intact.
- [x] 3.4 Add expected-instance cue cancellation/observation and sequence-source eligibility at the relevant validation/preflight boundary. Verify actor/radio/autoplay/fixture conflicts, same-source contention, foreground busy, stale cancel, and identical logical completion with mute or absent output.
- [x] 3.5 Publish bounded once-only accepted player interaction outcomes with typed target/result identities at the actual acceptance points. Verify door/switch/radio/document actions, physical pickup/drop/throw acceptance versus queue insertion/refusal, knock without state change, inactive input, and exclusion of safety releases or author-driven state commands.

## 4. Deterministic progression core

- [x] 4.1 Implement run-local facts, region occupancy and trigger latches from supplied observations. Verify spawn-inside behavior, scene entry, initially true conditions, sampled intermediate entries, unsampled thin-region pass-through, initial terminal predicates and every supported condition kind.
- [x] 4.2 Implement start guards, once/rearm lifecycles, run identities and consumed triggers without backlog. Verify cancellation-suppressed starts, active-run triggers, false-after-terminal rearming, repeated region/action occurrences and fresh-process reset.
- [x] 4.3 Implement finite immediate-step advancement and waits using typed command results and injected active time, classifying existing wait outcomes from the boundary snapshot before new starts. Verify delay start/deadline behavior, zero delay, actual cue/route completion, unmet conditions, no backdating, busy retries and all refusal/ownership-loss terminal cases.
- [x] 4.4 Implement cancellation preflight, stable event-ID dispatch and frozen-snapshot semantics. Verify cancellation-versus-next-step ties, simultaneous conflicting writes, reordered storage, next-boundary causal effects, bounded authored cycles, owned-only cleanup and preservation of accepted state/door-target changes. Test completed cue and route reuse within one boundary in both event-ID orders: prior completion succeeds, new ownership remains distinct and stale cancellation cannot stop it.
- [x] 4.5 Add read-only progression snapshots and the bounded transition trace. Verify event/run/step/reason/target context, retained facts, last-256 retention and dropped count, suppression of unchanged-wait spam, and explicit observation-overflow failure without silent action loss.

## 5. Runtime composition and timing integration

- [x] 5.1 After sections 2-4, wire accepted fixed-step region transitions and player outcomes into Engine's observation boundary. Verify zero/one/multiple-step batches, occurrence consumption exactly once and unchanged behavior for empty narrative scenes.
- [x] 5.2 Place cancellation preflight after ordinary accepted interactions but before character audio handoff, then dispatch surviving audio and event continuations before presentation. Verify a cancel coincident with a final interaction marker starts no stale sound, immediate radio/light changes agree with captions/rendered state, and existing player/character/audio regressions pass.
- [x] 5.3 Coordinate narrative active time with the common development suspension/minimize policy while preserving capped physics and uncapped audio. Verify reading, cursor release/owed drop, paused pages, held input, accepted-occurrence retention, discarded queued commands, long stalls and equal decisions for equivalent observation/time streams under varied rendering outcomes.
- [x] 5.4 Establish initialization, fresh-launch, close and teardown ownership. Verify invalid reference/resource startup, partial-construction cleanup, close while busy/suspended, instance-safe cancellation before referenced owners are destroyed, and no writes to loaded level definitions/files.

## 6. Editor commands and region workspace

- [x] 6.1 Add fact/region/event list selection and add/duplicate/remove/property commands to existing document history. Verify unique IDs, fresh duplicate IDs with preserved outgoing links, capacity failure with no partial edit, selection, dirty state and stable redo identity.
- [x] 6.2 Extend typed rename/delete handling across narrative and existing referenced records. Verify all incoming links update atomically on rename, deletion retains broken IDs, no unrelated type is retargeted, and undo/redo restores definitions and references together.
- [x] 6.3 Add region wire previews, list/viewport selection, numeric bounds and surface placement using existing capture/cancel policy. Verify upper-floor placement, retained extents, nearer-wall rejection, invalid finite-bound handling and canceled gesture history with focused geometric/editor tests; visual acceptance is separate.
- [x] 6.4 Add ordered step/condition editing commands and ordinary save/reopen/Play preflight. Verify insert/remove/reorder, kind changes, unknown reference retention, resource/ownership failures, canceled dirty launch and preservation of the running game's earlier saved definitions.

## 7. Editor properties and semantic automation

- [x] 7.1 Expose typed trigger, guard, cancellation, repeat and step property controls plus precise event/step diagnostics. Verify all supported kinds in real-ImGui tests, including invalid draft versus committed value, canceled edits and one undo entry per committed operation; keep preview silent and non-simulating.
- [x] 7.2 Add semantic metadata and independent applied-document inspection for each new standard control and record kind. Verify fresh discovery, selection, draft/applied distinction, validation, step ordering and references through existing automation abstractions without feature-specific mutation tools.
- [x] 7.3 Prepare a reproducible semantic runtime scenario with disposable fixtures, expected draft/applied/saved states and cleanup. Verify it covers creating events, invalid input, cancel, rename/delete repair, reordering, undo/redo and save/reopen; clearly separate unsupported viewport operations and game launch.

## 8. Neutral fixtures and integration checks

- [x] 8.1 Create the ordinary neutral T4 saved scene: region -> light -> captioned one-shot -> delay -> actor route -> completion fact, canceled by turning off an initially-on radio. Verify ordinary selected-file launch uses only authored definitions, and add variations for door waits/refusal, accepted box/document triggers and busy audio without filename-selected progression.
- [x] 8.2 Add an explicit development T4 runner/report using the production progression path, injected checks and shared suspension control. Verify normal completion, cancellation before/during each owned action, repeat rules, input/reading transitions, observation trace and mute/no-device outcomes; report failed steps and exit status truthfully.
- [x] 8.3 Add game/editor GPU smoke coverage for the new definitions, region preview, coherent event presentation, minimize/restore, recovery and owned-action teardown. Verify CPU fixture preflight without a window and register the real GPU checks under the existing Vulkan validation workflow.

## 9. Automated and semantic integrated verification

- [x] 9.1 Incrementally build affected Debug targets, including engine_tests, near_laugh, level_editor and vulkan_smoke; configure only if options/setup changed. Run affected tests during implementation, then ctest --preset debug --output-on-failure and relevant automation/Python suites per DEVELOPMENT.md. Retain commands, real exit codes and diagnostics; fix failures before acceptance.
- [ ] 9.2 Run the prepared narrative semantic scenario via delegated ui_test_runner using the editor-ui-testing skill and MCP or the official SDK after a fresh affected-target build. Verify independent applied state and cleanup; retain A no-window, B SDK-to-real-editor and C runner-visible four-tool evidence separately, with blocked/unknown states and no Windows-input fallback.
- [x] 9.3 Obtain independent review of integrated lifetime/instance ownership, scheduler timing and v11 compatibility changes. Verify review findings against the actual diff, resolve them and rerun only checks affected by resulting edits; retain unresolved limits explicitly.

## 10. GPU, authoring, performance and human acceptance

- [ ] 10.1 In an available authorized desktop/GPU environment, run ctest --preset vulkan-smoke --output-on-failure on the integrated build. Verify normal scenarios have zero unexpected validation errors through destruction, including T4 game/editor cases; retain environmental failures without treating them as acceptance.
- [ ] 10.2 After preparing fixtures and expected results, delegate creation of the second neutral variation through semantic editor controls, including a different region/link arrangement, sequence, cancellation, broken-link repair, save/reopen and undo/redo back to the saved baseline. Verify the retained scene against independent applied/saved state, clean/dirty transitions and confirm no script or runtime change manufactured its authored content.
- [ ] 10.3 With explicit authorization for the desktop run, delegate bounded region appearance/picking/placement and ordinary saved-file Play checks separately from semantic automation. Verify the second variation's visible behavior, normal/canceled routes, obstruction, reading and recovery; retain screenshots/observations without claiming listening from visual evidence.
- [ ] 10.4 Run three paired Release measurements with the same scene/hardware/settings, events disabled versus the supported-capacity workload, using existing timing scopes, warm-up and accepted T1 gates. Verify workload actually executes, retain raw CPU/GPU/frame p50/p95/p99 and all run metadata, and investigate failed gates without silently changing thresholds.
- [ ] 10.5 Record human audible-play/listening observations separately from muted/no-device functional checks. Verify captioned cue order, cancellation and retained clarity in the ordinary and second scenes; unavailable physical listening remains unverified until an explicit acceptance decision.
- [ ] 10.6 Consolidate T4 acceptance from the neutral scene, independent authoring variation, integrated checks and recorded limitations. Verify every criterion is supported by evidence or an explicit user-accepted limitation; leave unverified acceptance tasks open rather than inferring success from unit tests.

## 11. Documentation and final handoff

- [x] 11.1 Update affected gameplay, architecture, development and roadmap documentation plus relevant current-format README statements to match implemented v11 authoring, timing, cancellation and diagnostics. Verify commands and links against the delivered workflow, keeping story/checkpoint/session work deferred.
- [x] 11.2 Review the complete diff and run git diff --check and openspec validate add-narrative-state-and-sequences --strict. Verify all checklist claims match retained results, all new controls have semantic coverage, and no unresolved review finding is hidden by structural validation.
- [ ] 11.3 Prepare the accepted-change handoff with supported limits, compatibility, evidence paths and implications for P09. Verify the implementation and acceptance gates are actually closed before proposing main-spec synchronization and archival; do not mark planning readiness as implementation completion.
