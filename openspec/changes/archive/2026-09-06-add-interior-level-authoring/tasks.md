## 1. Version-4 document and compatibility

- [x] 1.1 Extend world document and immutable-level data with optional terrain, bounded named entries, and an explicit default identifier, replacing the singleton authored spawn; verify the world target builds and the public data contains no new backend, parser, or resource-path dependency.
- [x] 1.2 Implement exact v2/v3 decoding and v4 normalization/serialization, including source-version reporting, strict field sets, stable entry order, and explicit-save migration; verify codec coverage for both legacy shapes, null/present terrain, all switch states, canonical repeated saves, locale independence, malformed fields, and unsupported versions.
- [x] 1.3 Adapt existing affected consumers and test fixtures to the current document shape without restoring mandatory terrain or a second authored spawn; verify the game, editor, and test targets compile, legacy compatibility fixtures use their actual old schemas, and the prototype's authored values remain represented correctly.

## 2. Entry support, clearance, and immutable selection

- [x] 2.1 Validate entry count, identifier syntax and uniqueness, finite poses, and the default reference for every entry; verify diagnostics distinguish entries and that an invalid unselected entry blocks both save and immutable runtime handoff.
- [x] 2.2 Implement shared height-specific support and standing-clearance checks against structural floors, optional terrain, and the yawed chair proxy, and remove terrain-footprint admission rules for entries, chair, and switch; verify two entries at the same X/Z on different floors, upper/outside-terrain support, unsupported height, wall/ceiling/proxy overlap, terrain intrusion, and finite transformed bounds through deterministic world tests.
- [x] 2.3 Add validated entry lookup for explicit/default startup selection without mutating or reordering authored data; verify default lookup, alternate entry poses, missing-ID failure, and unchanged document/default values.

## 3. Runtime launch and initial player pose

- [x] 3.1 Add backend-neutral level/entry configuration and game-launcher option parsing with native path preservation and one-time relative-path resolution; verify default/explicit combinations, unknown/repeated/missing/empty arguments, Unicode and spaced paths, and usage failure before application startup.
- [x] 3.2 Resolve only the selected level alongside executable-relative shader, texture, and chair resources; verify startup resource tests from another working directory, an absent unselected packaged level, and actionable selected-file/resource failures without fallback.
- [x] 3.3 Resolve the entry before physics/player/renderer creation and initialize character position, yaw, and presentation snapshots from that pose; verify alternate-entry first-view behavior and dependency-safe cleanup when level or entry resolution fails.

## 4. Terrain-free collision and presentation

- [x] 4.1 Make terrain collision optional while retaining structural bodies, the chair proxy, static switch-obstruction queries, and current main-thread player behavior; verify actual Jolt settling at upper/lower starts, representative support/clearance agreement, wall blocking, existing terrain behavior, and partial-construction cleanup with no terrain body.
- [x] 4.2 Generate opaque world geometry with optional terrain and preserve the current chair, texture, normal, lighting, and switch paths; verify terrain-free geometry, matching structural bounds, and retained terrain/switch geometry behavior with deterministic scene tests.
- [x] 4.3 Support an absent generated editor mesh and transactional preview replacement between terrain-bearing and terrain-free documents; verify that empty invalid interiors remain inspectable, failed replacement retains usable resources, and the game/editor Vulkan smoke cases added in section 10 cover both transitions and resource lifetime.

## 5. Interior documents and entry editing

- [x] 5.1 Add New Interior with a valid starter floor, default entry, two lights, ambient, chair, and no terrain, and integrate New with dirty replacement and Save As; verify unsaved/dirty startup, Save/Discard/Cancel, failed-save preservation, clean legacy opens with migration notice, and gameplay-invalid file admission versus malformed-file rejection in document tests.
- [x] 5.2 Add concrete entry selection, add, duplicate, remove, rename, pose, and make-default commands with durable strings separate from transient editor IDs; verify entry limits, unique generated IDs, atomic rename/default undo, protected default/last deletion, list/viewport agreement, and unrelated-value preservation.
- [x] 5.3 Integrate entry/default edits with the existing 128-entry history, saved revision, validation, numeric commit, and preview workflows; verify undo/redo restores content and selection, undo-to-save clears dirty state, invalid edits remain repairable, and launch selection alone never dirties the document.
- [x] 5.4 Disable terrain-only placement and sculpting for absent terrain and clear obsolete gestures, footprints, and entry/placement selections on document replacement; verify terrain-to-interior-to-terrain UI transitions, capture suppression, and terrain strokes revalidating all entries without disturbing clear upper-floor starts.

## 6. Structural-surface placement

- [x] 6.1 Extend placement picking to return actual structural/terrain hit position, normal, distance, and target context while excluding the moved object; verify nearest-surface selection, stacked floors, wall/underside rejection for entries, occlusion without hidden fallback, terrain-only behavior, and deterministic ties.
- [x] 6.2 Implement the design's floor and wall anchors, visible light/switch offsets, and outward-facing switch wall mounting while preserving unrelated properties; verify each supported object/face combination, retained terrain offsets, deterministic repeated placement, and a single undo restoring the complete prior pose.
- [x] 6.3 Expose placement mode and candidate face/elevation feedback in the editor and route the displayed hit to the committing action; verify real UI capture, camera navigation, miss, unsuitable-target, and Cancel paths produce no edit, and an upper-floor placement previews and commits the same target.

## 7. Saved-file playtesting

- [x] 7.1 Add editor playtest-entry selection and the prepare/validate/Save-and-Play-or-Cancel/Save-As transaction, including normalized disk-content comparison; verify clean play, dirty confirmation, canceled and failed save, unknown or invalid entries, external disk changes, and one consumed launch request per activation through recorded launch intents.
- [x] 7.2 Implement the editor-owned native child-process owner using the sibling executable and literal level/entry arguments on the repository's existing desktop hosts; verify a real argument-recording child handles spaces, non-ASCII and host-valid shell-significant characters without shell interpretation, and missing-executable/process-creation failures retain useful path context.
- [x] 7.3 Wire process status and non-blocking exit observation into the editor, restrict it to one active child, and preserve independent child lifetime on editor shutdown; verify no duplicate launch, responsive authoring, successful/nonzero exit handling, released or reaped process resources, and continued child execution when its launching editor closes.
- [x] 7.4 Connect the Play UI, save dialogs, validation/launch feedback, and application launch boundary; verify real UI actions consume pending edits once, Cancel launches nothing, Save As uses the selected path, errors do not arm delayed launches, and game/editor dependency checks still pass.

## 8. Packaged M1 acceptance scene

- [x] 8.1 Explicitly migrate the packaged prototype to canonical v4 and add `apartment-stairs.level.json` with the required room/corridor/kitchen/stairs/landing route and `apartment`/`lower-landing` starts; verify deterministic load/save, retained prototype fixture semantics, full entry validation, and actual Jolt traversal of the complete stairs in both directions without jumping or crouching.
- [x] 8.2 Extend resource-copy and process-layout coverage for the acceptance scene while retaining the default prototype; verify both files are available beside affected executables and explicit selection works from outside the source tree without changing the packaged prototype bytes.

## 9. Documentation and requirement review

- [x] 9.1 Update affected current-implementation sections of `docs/ARCHITECTURE.md`, `docs/GAMEPLAY.md`, `docs/RENDERING.md`, and `docs/DEVELOPMENT.md` for v4 compatibility, named starts, optional terrain, process ownership, launch commands, and author-save-launch controls; verify the documented commands match the implemented interfaces and no instructions still require overwriting the packaged prototype to play an authored level.
- [x] 9.2 Review the final change against all P01 deltas and roadmap M1 acceptance, and update roadmap planning/implementation status without declaring later milestones delivered; verify proposal/design/tasks remain coherent and `openspec validate add-interior-level-authoring --strict` passes.

## 10. Integrated validation and acceptance evidence

- [x] 10.1 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`; verify affected codec, world, physics/player, resources, editor/UI, process, and dependency-boundary tests pass, and review `git diff` for scope and incidental implementation-shape assertions.
- [x] 10.2 Extend and run game/editor Vulkan smoke for the interior and retained prototype, including alternate starts, empty editor geometry, scene replacement, resize/minimize/restore, swapchain recovery, switch state, and partial startup failures; verify `ctest --preset vulkan-smoke --output-on-failure` passes with no error-severity validation messages and record the device/layer availability and results.
- [x] 10.3 Perform and record the M1 manual exercise: create and author an interior, place entries on both floors, mount a switch on a wall, save, reopen, launch the chosen entry, traverse the required apartment/stair route in both directions from both starts, test blocked invalid entries and canceled dirty Play, and confirm packaged-prototype independence from another working directory; verify the observable outcomes in an acceptance record and leave unavailable checks explicitly pending.
