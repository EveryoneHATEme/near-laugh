Integration note: P03 introduced the v5 door shape; the authorized combined
P02 implementation supersedes its writer with v6 while retaining exact v5
reading and every door field. Tasks 1.2/1.4 are verified against that final
composed contract. See validation.md for evidence and the remaining manual
acceptance item.

## 1. Door definitions and version-5 persistence

- [x] 1.1 Add the bounded authored door record, concrete identity lookup, and shared yawed-leaf geometry helpers to world data; verify ID/count/range constraints, both signed opening directions, finite corners, and initial endpoint transforms with behavioral world tests.
- [x] 1.2 Extend the strict codec with exact version-5 `doors` and canonical serialization while retaining exact v2/v3/v4 readers normalized to no doors; verify missing/unknown fields, newer fields in older versions, byte-identical round trips, preserved order/IDs, locale independence, and opening without source writes using independent legacy fixtures.
- [x] 1.3 Extend shared validation for contradictory initial locks, initial leaf overlap with terrain/structure/prop/other leaves and every standing entry, excluding doors from entry support; verify terrain-free and upper-floor cases, an unselected blocked entry, safe later swing obstruction, and door-specific diagnostics.
- [x] 1.4 Carry immutable door definitions into the validated runtime level and editor source-version notice; verify normalization remains clean on open, explicit save writes v5, and existing no-door levels preserve authored behavior.

## 2. Moving collision and continuous clearance

- [x] 2.1 Add physics-owned kinematic leaf bodies with bounded thin-box geometry, project-owned lookup, zero-velocity accepted transforms, and complete partial-startup cleanup; verify initial open/closed collision, stance clearance, no render-model loading, and injected failures after several leaf bodies exist.
- [x] 2.2 Implement conservative full-arc clearance and bounded clear-prefix refinement against static terrain/solids/proxy and other leaves; verify a thin blocker missed by endpoint-only checks, both rotation signs, max speed/dimensions, frame-gap/floor clearance, no progress on immediate obstruction, and deterministic door-ID arbitration.
- [x] 2.3 Include the previous/current player capsule presentation envelope in motion acceptance; verify standing, crouched, stance-changing, diagonal and vacated-space cases preserve a clear interpolated player view without pushing, carrying, or embedding the character.
- [x] 2.4 Extend segment obstruction to accepted leaves with the selected-door self-surface exception and origin-inside rejection; verify targeting the door itself, a door hiding a switch, a blocker at the target surface, unrelated nearer geometry, a door behind the target, and existing static obstruction behavior.

## 3. Concrete interaction and runtime state

- [x] 3.1 Add physical R and the concrete lock action, and centralize release latches for interaction/lock/knock while preserving flashlight behavior; verify startup held keys, held misses, cursor transitions, minimized waited batches, simultaneous-action priority, and release-before-repress behavior.
- [x] 3.2 Implement nearest door/switch ray arbitration from the displayed eye with the two-metre inclusive reach and stable min-distance tie selection; verify multiple candidates, storage-order independence, inside origins, unsupported nearest actions, and no fallthrough to objects behind a refused door.
- [x] 3.3 Implement run-local angle/direction/lock state and bounded concrete results; verify endpoint clamping and endpoint-first retry after zero-progress obstruction, reversal while moving/stopped between endpoints, closed-only bolt-side locking, locked refusals, knock without state changes, and no automatic resume after a blocker leaves.
- [x] 3.4 Integrate player-first fixed steps, deterministic door clearance/install, and one post-simulation action dispatch; verify equivalent scripted action batches with zero/one/several steps, 100 ms accumulation bounds, minimize/restore without catch-up, and unchanged authored level data through motion and restart.
- [x] 3.5 Consume concrete results in temporary generated bolt/refusal/knock feedback with simulation timers; verify distinguishable presentation values, replacement of transient feedback without a queue, no leaf/collision changes, and no replay across renderer outcomes.

## 4. Game and editor presentation

- [x] 4.1 Add the bounded source-independent opaque-box frame description and common world-space vertex generation for leaves/feedback; verify finite positions, matching accepted leaf bounds, outward normals, UV/surface selection, empty input, and capacity limits with data-oriented geometry tests.
- [x] 4.2 Add one reusable changing-geometry buffer owner per existing frame slot and draw it through the existing opaque pipeline; verify slot-fence reuse, absent-stream handling, partial allocation cleanup, and static-resource retention with Vulkan smoke and failure injection without extending the 128-byte push block.
- [x] 4.3 Integrate authored initial door presentation into editor scene replacement and terrain-only rebuilds; verify open/closed and lock previews, empty-to-present transitions, safely omitted invalid geometry, retained usable resources on replacement failure, and resize/recovery smoke behavior.

## 5. Door authoring

- [x] 5.1 Extend the existing flat object list, transient selection, creation, duplication, removal, and ID editing for doors; verify unique `door-N` allocation, durable identity after undo/redo and save/reopen, count limits, selection restoration, and no changes to unrelated objects.
- [x] 5.2 Add bounded door properties and initial-state editing through existing commit/history rules; verify typed/dragged numeric edits, rejected unsafe values, repairable initial open/locked contradictions, diagnostic initial overlaps, and refused save/play until correction or undo.
- [x] 5.3 Add door-leaf viewport picking, hinge/open-arc/lock-side overlays, and upward-surface placement with visible floor clearance; verify nearest hit selection, upper-floor and terrain placement, unsuitable wall/underside blocking, canceled/UI-captured input, and one history entry per committed placement.
- [x] 5.4 Exercise the existing saved-file Play transaction with door edits from both named starts; verify dirty save/cancel behavior, fresh-file equality checks, immutable running state after editor saves, and unchanged standalone editor dependency boundaries.

## 6. Playable acceptance and documentation

- [x] 6.1 Extend the packaged apartment/stairs level with the initially closed unlocked room door, interior bolt side, and reachable door-blocked switch view; verify shared validation and deterministic traversal from both entries after opening without jumping, crouching, or altering movement policy.
- [x] 6.2 Update ARCHITECTURE, GAMEPLAY, RENDERING, and DEVELOPMENT for v5 migration, moving state/ownership, controls, door authoring, temporary feedback, and exact acceptance positions; review them against implemented behavior and preserve P02's subsequent v6 integration boundary.

## 7. Integrated validation and handoff

- [x] 7.1 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`; resolve affected failures and record results for codec/world, physics, interaction, runtime, editor, and geometry tests without source-text checks that freeze implementation syntax.
- [x] 7.2 Run `ctest --preset vulkan-smoke --output-on-failure` with repeated door movement and feedback through resize, minimize/restore, recovery, and injected resource failures in game/editor; verify no error-severity validation messages and document any unavailable desktop step.
- [x] 7.3 Manually exercise both sides of the packaged doorway: open/close, moving reversal, lock/unlock from each side, locked refusal, knock with no audio, player-blocked opening/closing, vacated-space stops, reaching the switch through/around the leaf, flashlight independence, and restart; record observable results and any remaining limitations.
- [x] 7.4 Review `git diff`, validate `add-interactive-doors` with `openspec validate add-interactive-doors --strict`, and check proposal/spec/design/task consistency; deliver the validation record and shared v5 contracts for P02 to rebase before any later archive/apply handoff.
