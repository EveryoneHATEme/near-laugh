## 1. Prerequisite and v9 definitions

- [ ] 1.1 Confirm P07a is accepted/archived and rebase these artifacts on its resulting main specs/catalog metadata; verify the selected model, calibrated contacts and current level baseline agree with this plan.
- [ ] 1.2 Add bounded actor/mark/route definitions and catalog-based reference validation; verify null/empty cases, capacity, IDs, route ownership, source exclusivity and unsupported clip/reference diagnostics.
- [ ] 1.3 Add strict v9 decoding and canonical writing with exact v2-v8 normalization; verify old version fixtures, wrong-version fields, byte-identical round trips and unchanged source files on Open.
- [ ] 1.4 Add mark support/static-clearance and initial actor/player-entry/door overlap validation; verify multi-floor marks, missing support, invalid starts and valid routes blocked by later door poses.
- [ ] 1.5 Migrate packaged levels/preparation scripts and retain v8 fixtures; verify deterministic regeneration, preserved prior light/audio/geometry values and explicit-save migration notices.

## 2. Accepted actor collision

- [ ] 2.1 Separate once-per-step world advancement from the existing player step as required for actor coordination; verify existing player/door regression tests and deterministic zero/multiple-step batches.
- [ ] 2.2 Add privately owned catalog capsule proxies and startup cleanup for zero/four actors; verify initial states and injected partial construction failures release all bodies before world teardown.
- [ ] 2.3 Implement swept route displacement with supported landing, step and slope checks; verify thin-wall/prop obstruction, partial accepted distance, stairs, over-height steps and unsupported gaps without tunneling.
- [ ] 2.4 Coordinate player swept stance, actor-ID ordering and door sweeps; verify player/actor path crossings, opposing actors, reordered definitions and opening/closing/reversed/obstructed doors.
- [ ] 2.5 Include actor proxies in player stance/traversal and interaction obstruction while excluding actor support; verify no standing inside/on actors and no E/R/knock through an actor to a door/switch.

## 3. Concrete route state

- [ ] 3.1 Implement actor-owned start/cancel/results and initial-route startup; verify repeat start, busy different route, cancel while blocked, restart from current pose and fresh authored reset.
- [ ] 3.2 Implement segment heading, accepted grounded walking, mark facing and final clip; verify arrival tolerance, 180-degree tie, repeated marks and no snap across blockers or premature interaction.
- [ ] 3.3 Drive walk phase/contact crossings from accepted distance and blend idle on blockage; verify waiting creates no travel/footsteps and clearance resumes the saved phase without teleportation.

## 4. Audio and runtime composition

- [ ] 4.1 Prepare the neutral short footstep and interaction/caption assets with provenance and actor-link preflight; verify duration/contact-spacing/source-kind limits and missing selected versus unselected content.
- [ ] 4.2 Connect owned contact/interaction events to accepted source positions and P04 arbitration; verify one marker start, Busy hold/retry, action-local cancellation, mute and silent-device equivalence.
- [ ] 4.3 Compose actors and P07a pose presentation in Engine with safe selected-resource lifetime; verify zero/four-actor startup, current accepted render/audio poses and selected failure cleanup.
- [ ] 4.4 Add explicit development cancel/restart/suspension controls and joint minimized-wait policy; verify player/door/actor/cue freeze, inactive-edge consumption, cursor-release policy, long-frame cap and no recovery replay.
- [ ] 4.5 Preserve/render initial actor definitions in the editor and extend selected Play preflight; verify unrelated edit/save/undo round trips, no editor autoplay and asset failure creates no child.

## 5. Runtime acceptance

- [ ] 5.1 Package neutral one/four-actor route fixtures with turn, stairs, operated door and final interaction; verify ordinary --level/--entry launches use authored initial routes without filename behavior.
- [ ] 5.2 Add integrated route/audio/recovery and Vulkan smoke cases; verify moving actor/door shadows, repeated startup, capacity and partial failure with validation through teardown.
- [ ] 5.3 Walk/listen to player/static/door/actor blockage, release and cancellation cases; retain captures, caption/source observations, measured footstep latency/drift and explicitly unavailable evidence in validation.md.
- [ ] 5.4 Measure one/four moving actors against the same light-only baseline using P07a conditions; retain raw timings, per-scope costs and explicit frame-target results without hiding failures.
- [ ] 5.5 Follow docs/DEVELOPMENT.md: configure/build affected targets, run affected deterministic and Vulkan smoke tests, validate any changed shaders and strict OpenSpec artifacts; record actual commands and outcomes.
- [ ] 5.6 Update implemented v9/ownership/control documentation and review git diff; verify old module/version summaries are accurate and T2 remains pending P07c.
- [ ] 5.7 Prepare the accepted P07b handoff record for the subsequent archive workflow; verify P07c's dependencies match the delivered runtime, persistence and editor-compatibility contracts.
