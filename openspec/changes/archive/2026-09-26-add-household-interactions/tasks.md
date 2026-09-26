## 1. Definitions, compatibility and validation

- [x] 1.1 Recheck the accepted P04/P10/P07 baseline against current main specs and confirm the v9-to-v10 transition and affected capability list before code edits; verify with scoped code/spec review and `openspec validate add-household-interactions --strict`.
- [x] 1.2 Add bounded box/document/radio definitions and strict v10 decoding with empty-household normalization for exact v2-v9 inputs; verify accepted legacy shapes, rejected cross-version/unknown fields, bounds and unchanged source files in world/codec tests.
- [x] 1.3 Extend canonical v10 serialization and immutable level handoff; verify byte-identical save/load/save, preserved collection/page ordering and links, and exclusion of current velocities/hold/page state.
- [x] 1.4 Add shared initial-box clearance and document/radio validation, including actor/source ownership conflicts; verify surface contact, clear airborne starts, overlaps with entries/actors/doors/boxes, invalid references and actionable editable diagnostics without GPU/audio/Jolt construction.

## 2. Dynamic physics and existing collision contracts

- [x] 2.1 After section 1, add stable-ID-ordered ownership of up to 16 fixed-profile dynamic box bodies and backend-neutral state access; verify gravity, full rotation, sleeping/waking, box-to-box contact and partial body-construction teardown.
- [x] 2.2 Add the virtual player's physics-visible inner body with validated creation, owned destruction and transactional stance synchronization; verify stationary standing/crouched box impacts, inner-body allocation failure and failed stance replacement.
- [x] 2.3 Extend contact and query filtering for the player representation and boxes, including bounded player pushes and no box-derived player ground/stair/jump support; verify self-query exclusion, stand clearance, isolated/tilted/stacked top contacts and wall-confined landing without teleportation or penetration.
- [x] 2.4 Integrate current boxes into actor and door swept-clearance paths while preserving accepted-pose and no-crush policies; verify thin blockers, actor waiting/resume, box impacts on stationary actors/leaves and the door's new-press restart rule.
- [x] 2.5 Configure bounded speed/material/contact settings, CCD and fixed collision substeps and handle world-update errors; verify maximum linear/angular-speed impacts against 0.02 m walls and minimum-valid 0.04 m doors, 16-box settling/stress and finite state after repeated collisions.

## 3. Physical hold, input and runtime ownership

- [x] 3.1 After section 2, implement one bounded physical hold with no body replacement or collision suppression; verify clear acquisition, constraint-creation failure, blocked target motion, break-distance release and continued response after extended stillness and suspension.
- [x] 3.2 Implement drop and one-shot throw at the actual pose, preserving existing velocities and applying no drop impulse; verify nearby-wall throws, single impulse per accepted command and repeated pickup/drop/throw without identity or resource duplication.
- [x] 3.3 Extend displayed-eye nearest-target selection to free oriented boxes, documents and radios with existing reach/ties/no-fallthrough; verify rotated bounds, reordered definitions, inside origins, own-front-surface exclusion and unrelated equal/nearer blockers.
- [x] 3.4 Add the concrete one-pending-command fixed-boundary handoff, stored throw direction and simulation-eye/look motor targets; verify zero/one/multiple-step batches, failed eligibility recheck, competing input and equivalent changing-look sequences with exact action counts and documented physical tolerances.
- [x] 3.5 Add empty-handed/carrying control ownership and release gates, retaining independent flashlight behavior; verify E drop, right-mouse throw, suppressed lock/targeted actions while held, drop-before-throw priority and no inherited held-input activation.
- [x] 3.6 Coordinate suspension, minimization, cursor loss, ordinary command cancellation and owed safety release; verify cursor loss followed by zero steps, minimize and recapture still releases before the next advancing step, while suspension alone preserves the hold and excludes catch-up time.
- [x] 3.7 Integrate concrete household state into runtime construction/teardown and explicit neutral development pause/mute controls; verify fresh-run reconstruction, physics-update error handling, all render outcomes and constraint/body/content destruction order without P05/P09 adapters or P12 menus.

## 4. Full-orientation geometry and text presentation

- [x] 4.1 Extend the backend-neutral changing box representation to full orientation and adapt door producers and consumers; verify transformed corners/normals, invalid rotations, unchanged door behavior and aggregate bounds with deterministic geometry tests.
- [x] 4.2 Compose physical box, document and radio-indicator geometry with existing doors using the combined 248-box budget and household-aware material preparation; verify maximum mixed counts, household-only scenes without doors and empty-household compatibility through CPU scene-preparation tests.
- [x] 4.3 Reuse fenced changing geometry for matching color/shadow poses and preserve coherent editor replacement/recovery; verify resource ownership/failure paths with affected deterministic render tests, leaving visual/GPU acceptance to section 9.
- [x] 4.4 Add bounded readable-page and feedback layout using the existing font with independent caption lanes and checked aggregate glyph capacity; verify Russian/Latin/Ё content, title/pages/controls at all supported sizes, long-word/glyph failure and simultaneous worst-case document plus captions.

## 5. Document reading and radio behavior

- [x] 5.1 After sections 3 and 4, add reader identity/page state and E/Escape close plus A/D navigation, retaining stance and suppressing world controls while active time continues; verify page limits, open/close edges, held movement/action rearming, look reset and concurrent falling boxes/actor/audio activity.
- [x] 5.2 Preserve reader/feedback state through resize, minimize and explicit suspension without replaying navigation or activating Escape twice; verify current page/remaining feedback duration and safe return to captured exploration.
- [x] 5.3 Resolve exclusive radio source/prop ownership, initial-on start, source-position override and on/off cancellation through the existing audio coordinator; verify one instance, stop/restart, immutable definitions, silent-device/muted behavior and no interference with actor or unrelated cues.
- [x] 5.4 Resolve household hints/results and captions after accepted state changes in the same batch; verify stale radio captions are absent on off frames and essential text remains visible alongside document and carrying feedback.

## 6. Complete editor workflow

- [x] 6.1 After section 1, extend EditorDocument concrete object values, IDs, add/duplicate/remove and property/page commands; verify one-entry commits, bound failures, stable IDs/selection across undo/redo and preservation through unrelated edits.
- [x] 6.2 Extend prop/source rename and deletion handling to radio links and exclusive ownership; verify atomic incoming-link updates, retained broken references, repair and undo, including duplicated controls that temporarily conflict.
- [x] 6.3 Add box/document picking and upward-surface placement, radio-control versus prop selection and initial-state preview; verify upper floors, unsuitable nearer faces, invalid linked objects, initial-only presentation and no editor physics construction.
- [x] 6.4 Add readable Russian title/page UI with shared layout preview, fixed-profile box properties, radio selectors and actionable diagnostics; delegate deterministic real-ImGui test execution to `ui_test_runner` and verify committed editing, input capture, history and error recovery.
- [x] 6.5 Extend selected-resource Play preflight to household content and preserve the saved-file transaction; verify no child on invalid text/assets/links, Save-and-Play/Cancel, external file mismatch, one-child consumption and independence of running physical state from later editor edits.

## 7. Fixtures, packaging inputs and documentation

- [x] 7.1 After runtime/editor integration, add a neutral authored P06 scene with multiple boxes, a multipage document, a radio, collidable table/floor, thin-wall and actor/door contacts; verify ordinary explicit-level startup uses definitions without a filename-triggered sequence and include reproducible automated capacity/physics controls.
- [x] 7.2 Update new-interior defaults, current packaged levels and their deterministic preparation tools to v10 while preserving legacy compatibility fixtures; verify regeneration/semantic comparison and startup from an unrelated working directory with only selected resources.
- [x] 7.3 Update affected sections of GAMEPLAY, ARCHITECTURE, RENDERING, DEVELOPMENT and ROADMAP to describe the physical box profile, controls, reading-time policy, compatibility, limits and concrete validation commands; verify against the implemented behavior and retain truthful T3 acceptance status and P06/P05/P09 boundaries.

## 8. Integrated automated acceptance

- [x] 8.1 Configure and build affected Debug targets, then run the integrated non-GPU suite with `ctest --preset debug --output-on-failure`; retain commands, exit status and relevant diagnostics, including existing player/door/actor/audio/editor regressions.
- [x] 8.2 Run deterministic integrated household scenarios spanning input, physical state, radio/text and recovery with equivalent fixed-boundary command/look sequences; verify exact identity/action counts, declared state tolerances, held-body wake, suspension precedence and construction/update failure paths.
- [x] 8.3 Delegate the complete automated editor UI scenario to `ui_test_runner` after preparing its fixtures; retain the result for text editing, household history/reference repair and saved-file Play, and report unavailable execution distinctly from a pass.
- [x] 8.4 Obtain independent review of the actual integrated physics/player/constraint lifetimes, step ordering and shared full-orientation frame interface; resolve critical findings, review `git diff` and run strict OpenSpec validation before hardware/manual acceptance.

## 9. GPU, runtime, T3 and performance acceptance

- [x] 9.1 Build/run affected runtime and editor Vulkan smoke/readback paths with `ctest --preset vulkan-smoke --output-on-failure`; verify spinning box color/shadows, door-free and maximum-capacity scenes, combined text at 800x600/1920x1080/3840x2160, recovery/failure/teardown and any changed shader validation, retaining artifacts and unavailable checks.
- [x] 9.2 Verify hold/drop/throw, contact/obstruction, reading/radio, mute, suspension and recovery through reproducible code checks; retain coverage, results and earlier desktop observations, with subjective feel/listening and human T3 acceptance distinguished. On 2026-09-11 the user stopped further manual testing and selected automated verification; see the validation record for the method change and its limits.
- [x] 9.3 Prepare a separate blank/neutral authoring starting point, then delegate creation of a second scene through the UI, save/reopen and ordinary Play; retain the actual UI-authored file and evidence of box/document/radio behavior without script or source edits substituting for authoring.
- [x] 9.4 Run the design's three-repeat Release baseline and 16-awake-box workloads under the supported hardware/resolution conditions with no concurrent build/GPU work; retain raw timings, constants, hardware and per-run gate results, investigate failures and leave unavailable measurements open.
- [x] 9.5 Consolidate automated, GPU, manual and performance evidence in this change's validation record and obtain the required T3 acceptance; verify every required task or explicitly accepted limitation is accounted for before requesting archive, without treating artifact readiness or passing tests alone as acceptance. Accepted on 2026-09-26 using the retained evidence with subjective hold/throw feel and physical listening explicitly unverified; see the final acceptance decision in `validation.md`.
