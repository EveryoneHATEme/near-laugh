# Implementation and validation

Current progress: tasks 1.1–3.4 and 4.3–4.4 are implemented and checked (15/18).
Tasks 4.1–4.2 and 4.5, including complete P07/T2 acceptance, remain pending.

The following section-1 record is retained from the preceding session, whose
scope was tasks 1.1–1.4 only. The section-2 session is recorded separately below.

## Prerequisites (2026-09-08)

P07a and P07b are accepted and archived. Their respective task lists contain
23/23 and 25/25 completed tasks. The archived requirement blocks match the
current main specs (8 P07a and 17 P07b requirements); this change's deltas already
extend that baseline, so no requirement rewrite is needed.
See the [P07a handoff](../archive/2026-09-08-add-character-animation/handoff.md)
and [P07b handoff](../archive/2026-09-08-add-scripted-characters/handoff.md).
P07b acceptance retains the explicitly accepted limits of unverified physical
listening and hardware latency. Those limits do not block section 1.

Reuse: `LevelCharacters` and canonical v9 in `level_document.hpp`, shared
`validateCharacterDefinitions`/catalog lookups, `prepareCharacterAssets` and
`CharacterPlayback`. The editor already prepares initial poses with that sampler;
its saved-file Play path preflights selected characters and linked audio before
process creation. Section 1 adds document/UI commands without a format change,
another sampler, or a runtime/physics dependency.

## Checks

Tasks 1.1–1.4 are implemented and checked. Overall change: 4/18 tasks complete.

| Command/check | Result |
| --- | --- |
| `cmake --preset debug` | Passed; `build/character-authoring-configure.log` |
| `cmake --build --preset debug --target engine_tests level_editor near_laugh -j 4` | Passed; `build/character-authoring-build.log` |
| `ctest --preset debug --output-on-failure` | 425/425 passed: 414 unit, six boundary and five process checks; 73.39 s; `build/character-authoring-debug-tests.log` |
| `ctest --preset debug -R '^(EditorCharacters\.\|EditorUiInteraction\.)' --output-on-failure` | Final focused run after formatting: 27/27 passed, 3.42 s; `build/character-authoring-focused-tests.log` |
| `ctest --preset vulkan-smoke -R '^(editor_scripted_characters\|editor_vulkan_smoke\|editor_vulkan_construction_failure)$' --output-on-failure` | 3/3 passed, 6.50 s; `build/character-authoring-vulkan-tests.log` |
| `openspec validate add-character-authoring --strict` | Passed |
| `git diff --check` and source/diff review | Passed; independent review also checked compound history, reference rewrites and UI draft lifetimes |
| `clang-format --dry-run --Werror` on the nine changed/new C++ files | Passed; two existing long lines in the touched UI file were wrapped; `build/character-authoring-format.log` |

Nine new document-command tests and six real ImGui tests exercise flat selection,
unknown references, finite invalid values/nonfinite rejection, combined capacity
failure with unchanged data/history/selection, independent initial-mark cloning,
route duplication/order, all incoming character/source renames, non-cascading
deletion, saved revision and v9 save/reopen. New automatic IDs reserve broken
incoming links, so unrelated creation cannot silently reconnect a deleted record.
UI tests activate actual ImGui controls and use mouse/keyboard numeric editing;
they do not establish manual visual or physical game acceptance.

The initial focused run passed 25/27 tests. Two new expectations incorrectly
assumed one route and equal mark-list lengths in the existing fixture; correcting
those expectations produced the passing integrated run above. The first
configure/build attempts were blocked by sandbox execution of installed Ninja;
the authorized runs outside the sandbox passed.

The existing Vulkan smoke checks include editor recovery/resource failure and
final destruction validation. This session adds no renderer, shader, playback or
surface-placement behavior. No shader validation, new snapshot lifecycle smoke,
manual second-scene authoring/Play, physical listening or performance sampling
was performed in this section-1 session.

## Remaining work at the section-1 handoff

Section 2: character picking/bounds/overlays, surface placement, silent clip and
schematic route snapshots, preview invalidation and audio-audition arbitration.
Existing compatibility-only diagnostic anchors remain until those tasks.

Section 3: pose-only resource updates and transactional preview recovery,
expanded saved-file/Unicode Play process checks, snapshot Vulkan lifecycle and
complete UI/input interaction coverage.

Section 4: build the independent neutral scene entirely through editor actions,
record actual saved-game obstruction/release, facing, sound/captions and shadows,
run the full acceptance checks, update final workflow documentation and record
T2/P07 acceptance before archive. P06/P05 readiness remains pending that chain.

## Section 2 implementation and validation (2026-09-09)

Session scope: tasks 2.1–2.5 only. Existing section-1 working-tree changes were
preserved. Prerequisites were rechecked against this checkout: archived P07a
23/23 and P07b 25/25 tasks are accepted, their 8 and 17 requirement blocks match
main specs, and section-1 commands/UI/tests are present. The integrated checks
below also rerun those command and compatibility tests. No prerequisite blocker
was found; earlier P07b listening/latency limits remain as recorded above.

Implemented catalog visual bounds and separate capsule overlays; selectable
mark/facing/route handles and ordered links; missing-link labels anchored only
to finite resolved marks. Wholly unresolved records retain list repair access.
Actor surface placement edits its initial mark, preserving actor selection and
shared consumers in one history entry. Nearest unsuitable faces still block.

Explicit clip snapshots share the renderer's already prepared immutable assets
and CharacterPlayback sampling/transitions. A schematic route snapshot copies
ordered marks and moves/turns at authored speed without physics, door operation,
events or sound; the panel states these limits. Only its selected actor's frame
pose is substituted. Other initial poses, document revisions/history and audio
definitions remain unchanged. Edits, history, selection (including a round trip
between frames), replacement, pending document operations, minimize and Play
discard snapshots. Character Start and audio Start stop each other, and UI
requests are consumed immediately without deferred restart.

| Command/check | Result |
| --- | --- |
| `cmake --preset debug` | Passed on retry outside the sandbox. Initial Ninja execution refusal is retained in `build/character-authoring-section2-configure.log`; successful retry was reported by the command output. |
| `cmake --build --preset debug --target engine_tests level_editor near_laugh -j 4` | Passed outside the sandbox; final incremental result in `build/character-authoring-section2-build.log`. |
| `ctest --preset debug --output-on-failure` | Final state: 448/448 passed, 437 unit, six boundary and five process checks, 74.43 s; `build/character-authoring-section2-final-tests.log`. |
| `ctest --preset debug -R '^EditorUiInteraction.ActorSurfacePlacement\|^EditorCharacterPreviewTest\.' --output-on-failure` | 12/12 passed after the final preview changes and real ImGui placement/Escape test, 2.28 s; `build/character-authoring-section2-focused.log`. |
| `ctest --preset vulkan-smoke -R '^(editor_scripted_characters\|editor_vulkan_smoke\|vulkan_scripted_characters)$' --output-on-failure` | Final state: 3/3 passed, 31.38 s; `build/character-authoring-section2-vulkan.log`. Validation diagnostics include final GPU destruction. |
| `openspec validate add-character-authoring --strict` | Passed. |
| `git diff --check`, changed/new C++ `clang-format --dry-run --Werror`, source/diff review | Passed. Independent review checked snapshot invalidation, immediate commands, audio exclusion, actor/frame mapping and borrowed palette lifetime. |

New coverage: five spatial tests, six placement tests, eleven snapshot tests and
one real ImGui actor placement/Escape test. The old diagnostic-anchor test now
expects list access when no real endpoint survives instead of a marker at the
entry. Snapshot tests compare poses directly with the common sampler and cover
interrupted blends, paused seeking, ordered standing turns/travel/facing, stair
elevations, final actions, batching and named unresolved-link refusals.
The editor character smoke renders explicit changing/paused clip and route
poses and verifies frozen authored poses, mutual exclusion with silent audition,
selection/edit invalidation, minimize/restore and refused Play without restart.

No required section-2 check was unavailable. Configure/build needed authorized
execution outside the sandbox because installed Ninja could not run inside it.
Shaders and their interfaces did not change, so shader recompilation/validation
was not required. These are automated results, not manual visual or physical
acceptance of a second authored scene; no listening, hardware latency or new
performance measurements were performed in this session.

## Remaining work after section 2

Tasks 3.1–3.4 remain unchecked: full acceptance of pose-only resource updates and
coherent replacement/failure behavior; selected-content, dirty Save and Play,
freshness and Unicode process cases; the extended snapshot GPU recovery/lifetime
matrix; complete real ImGui preview controls, focus/capture and history coverage.
Section 2 uses the delivered P07b frame/resource API and adds only its necessary
snapshot/overlay hooks and scoped checks; it does not accept the section-3 matrix.

Tasks 4.1–4.5 remain unchecked: author and retain the second neutral scene solely
through editor operations, save/reopen/launch it and record physical obstruction,
release, facing, sound/captions and matching shadows; run integrated acceptance,
finalize workflow documentation and record T2/P07 acceptance before archive.
P06/P05 readiness still requires that complete chain. Next implementation scope
is section 3, starting with task 3.1; it was not started in this session.

## Section 3 implementation and validation (2026-09-09)

Session scope: tasks 3.1–3.4 only. All preceding section-1/2 working-tree changes
were preserved. P07a/P07b prerequisites were rechecked: their archived checklists
have 23/23 and 25/25 completed tasks, and all 8 and 17 archived requirement blocks
still match current main specs. Sections 1–2 are present in the checkout and
their affected checks pass in the integrated run below. No prerequisite blocker
was found. The previously accepted P07b listening/latency limitations remain
recorded in its handoff; they do not block this section.

Task 3.1 retains the existing renderer/sampler boundary. CPU light-enable storage
is now allocated before GPU scene installation, alongside candidate actor assets,
IDs, palettes and borrowed frames. Only successful replacement moves all matching
owners into place. Playback and presentation recovery update fenced changing
vertices without replacing static geometry, character indices/materials or
lighting. The editor smoke now captures actual submitted GPU pixels with a fixed
camera and collapsed panels: seeking changes the visible pose, pause retains the
same image, and seeking back restores it exactly. Missing/malformed GLB and
injected index-upload, frame-slot allocation and shadow-allocation failures
retain a labeled usable old scene. The failed candidates change both actor and
light counts; old asset/ID/palette/light state remains coherent and fresh Start
is refused until correction/undo installs a current scene.

Task 3.2 extracts the application's selected-content launch transaction into
`launchEditorPlay`, called after GPU scene preflight. It validates selected
character/audio/caption/font resources and rechecks the saved document after
preparation before native process creation. Sixteen added behavioral cases use
a real argument-probe child with an accumulated launch count and a copy of the
bytes it actually read. They cover Save and Play/Save As cancellation and failure,
invalid character/audio links, changed prepared data, actor/mark/route/audio/entry
freshness, file removal, missing/unsupported selected assets and successful fresh
retry. Failures/cancellation create zero children; success creates exactly one,
including after polling process exit. Successful requests use literal Unicode
executable, resource and level paths from an external working directory, retain
the full saved character scene and do not require unselected character/audio
files or an output device. This probe verifies the process boundary; it does
not establish physical acceptance of a second game scene.

Task 3.3 extends editor Vulkan smoke with clip and route pause/recovery, actual
resize and alternate attachment format, running playback recovery, minimize,
an edit while minimized (deferred until restoration), edit/undo/redo, four/three/
zero-actor document replacement and shutdown with active inspection. No stopped
snapshot restarts implicitly. Character buffers, index memory, textures, lighting
and device lifetimes balance after final application/GPU destruction; Vulkan
diagnostics are checked afterward. The smoke explicitly requires validation.

Task 3.4 adds five real ImGui cases for clip/mark selection, start, pause/resume,
restart, stop, numeric seek, schematic-route controls and unresolved/stale Start
refusal. They drive keyboard and pointer capture through the camera input mapping
and verify unchanged authored data/history for preview controls. Tests exposed a
lost character draft when another list record was selected before Properties
could commit it. Character, ordinary-object and audio list selection now commits
the outgoing character field first. Regression cases cover incoming-reference
updates, character-to-route/audio selection, history and snapshot invalidation.

| Command/check | Result |
| --- | --- |
| `cmake --preset debug` | Passed outside the sandbox; initial installed-Ninja refusal is retained in `build/character-authoring-section3-configure.log`, successful retry in command output. |
| `cmake --build --preset debug --target engine_tests level_editor near_laugh -j 4` | Passed; final integrated build in `build/character-authoring-section3-build.log`. |
| `ctest --preset debug --output-on-failure` | 469/469 passed: 458 unit, six boundary and five process checks; 85.06 s; `build/character-authoring-section3-debug-final.log`. The 16 new native-child cases are registered under the unit label. |
| `ctest --preset debug -R 'EditorCharacterUiInteraction\.\|EditorUiInteraction\.' --output-on-failure` | 24/24 passed, 4.58 s; `build/character-authoring-section3-ui-final.log`. |
| `ctest --preset vulkan-smoke -R '^(editor_scripted_characters\|editor_vulkan_smoke\|editor_vulkan_construction_failure\|vulkan_character_animation\|vulkan_scripted_characters)$' --output-on-failure` | 5/5 passed, 61.38 s; `build/character-authoring-section3-vulkan-final.log`. Includes GPU pixel checks and validation after final destruction. |
| `openspec validate add-character-authoring --strict` | Passed. |
| `git diff --check`, changed C++ `clang-format --dry-run --Werror`, actual diff review | Passed; format output in `build/character-authoring-section3-format.log`. Independent resource-lifetime review led to the actual GPU pose check and a capture-size guard. |

The first focused run passed 45/47 cases. The two new capture tests activated
widgets by keyboard without moving the pointer onto a window, so ImGui correctly
reported no mouse capture. Sending real pointer-position events fixed the test
setup; all UI and process cases pass in the final integrated run. An intermediate
capture build tried to include ImGui directly in the application; the final code
keeps that dependency inside EditorUi via its small smoke capture helper.
Configure/build required execution outside the sandbox because installed Ninja
could not run inside it. No required section-3 check remains unavailable.

Shaders and their interfaces were unchanged, so shader regeneration/validation
was not required. These are automated correctness results, not new performance
samples, manual animation observations, listening or hardware-latency evidence.
No independent scene was authored or physically accepted during this session.

## Remaining work after section 3

Only tasks 4.1–4.5 remain unchecked. They were not started in this session:

- Author and retain the second neutral scene entirely through editor operations,
  including actor/marks/routes/audio/lights/door; Save As, reopen and launch it.
- Inspect that saved scene in the ordinary game for real player/door obstruction
  and release, facing, localized action sound/captions and matching actor shadows;
  retain the authoring/Play sequence and preview limits.
- Run integrated acceptance checks against the final section-4 state, finalize
  workflow documentation and review/validate the completed change. This session's
  checks are section-3 evidence and do not pre-complete tasks 4.3 or 4.4.
- Record the T2/P07 acceptance decision and archive handoff. P06/P05 readiness
  still requires evidence from the complete P07 chain. No archive was performed.

## Section 4 partial validation (2026-09-09)

Session scope: section 4 only. Prerequisites remain satisfied: archived P07a
23/23 and P07b 25/25 tasks are closed, and independent comparison confirms all
8 and 17 archived requirement blocks match the corresponding current main specs.
The accepted P07b listening/hardware-latency limits remain recorded in its
handoff. Existing implementation changes from sections 1–3 were preserved.

Task 4.4 is checked. DEVELOPMENT now includes character shortcuts alongside
the existing detailed controls. The character asset README describes current
catalog-based authoring and silent inspection and links to the control/evidence
records. Both distinguish schematic routes from physical saved-file Play.
ROADMAP reflects implemented sections 1–3 and archived P07b; the chosen remaining
order begins at P07c. T2 and later P06/P05 readiness still require the second-scene
acceptance and evidence from all three P07 stages.

| Command/check | Result |
| --- | --- |
| `cmake --preset debug` | Passed outside the sandbox. Initial installed-Ninja refusal is retained in `build/character-authoring-section4-configure.log`; the successful retry is in the tool output. |
| `cmake --build --preset debug -j 4` | Passed; includes game, editor and default checking targets. `build/character-authoring-section4-build.log`. |
| `ctest --preset debug --output-on-failure` | 469/469 passed: 458 unit (including real ImGui and native-child transaction cases), six boundary and five process checks; 91.94 s. `build/character-authoring-section4-debug-tests.log`. |
| `openspec validate add-character-authoring --strict` | Passed. |
| `git diff --check` and actual diff review | Passed. Independent review found no concrete defect in the changed saved-file Play transaction, coherent CPU/GPU replacement, borrowed frame-palette lifetime, fence-protected upload and validation after final destruction. This is static review, not new GPU evidence. |

The desktop authoring attempt launched the ordinary editor with its working
directory outside the repository, opened File > New Interior and observed the
valid unsaved v9 starter document. Capture:
`build/character-authoring-section4-desktop/01-new-interior.png`.
No second character scene was saved, reopened or launched. No runtime source or
level JSON was edited to define a scene. This partial attempt does not satisfy
4.1 or establish manual gameplay acceptance.

The temporary desktop helper made the editor always-on-top, obstructing access
to the user's Codex console. On the user's interruption, desktop automation was
paused and that behavior was removed from the helper. The editor process had
already closed when cleanup ran; no editor process remained. Desktop checks
must use a normal window and preserve access to the console when resumed.

Task 4.3 remains unchecked: configure/build and deterministic/UI/process checks
passed, but this session did not run Vulkan smoke after the desktop interruption.
No shader or shader-interface changes exist, so shader regeneration/validation
is not applicable. Previous section-3 GPU results remain historical evidence.

Tasks 4.1 and 4.2 still require a retained second scene authored through editor
operations, Save As/reopen/Unicode saved-file Play, initial editor/game agreement,
real player/door obstruction and release, facing, localized action sound/captions
and matching actor shadows. There are no new physical listening, gameplay or
shadow observations from this attempt. Task 4.5 remains unchecked until those
checks and integrated GPU validation support the T2/P07 decision and archive
handoff. No acceptance, spec sync, archive or subsequent change was performed.

### Resumed checks and UI routing (2026-09-09)

The current AGENTS.md routes existing UI tests to `ui_test_runner` and
screenshot interaction to `ui_driver`, and requires explicit authorization for
each run on the user's active desktop. Build and scenario preparation can run
before that authorization; foreground authoring and Vulkan runs remain pending.

`cmake --build --preset debug -j 4` passed outside the sandbox after the
interrupted retry. Only the packaged-resource copy was needed; the successful
result is in tool output. The initial installed-Ninja refusal remains in
`build/character-authoring-section4-resume-build.log`.

The configured `ui_test_runner` executed:

```powershell
ctest --preset debug -R 'EditorCharacterUiInteraction\.|EditorUiInteraction\.' --output-on-failure
```

Result: 24/24 passed, exit 0, 5.09 s;
`build/character-authoring-section4-resume-ui.log`. This is automated ImGui
evidence, not visual acceptance. A draft `handoff.md` records outstanding acceptance and archive
conditions without marking 4.5 complete.

After explicit authorization for this desktop run, `ui_test_runner` executed
`ctest --preset vulkan-smoke --output-on-failure`: 12/12 passed, exit 0,
217.90 s. Log: `build/character-authoring-section4-resume-vulkan.log`.
This includes game/editor character rendering, editor construction failure and
recovery, runtime lifecycle and validation after final GPU destruction. The
runner confirmed no test GUI processes remained before transferring desktop
ownership to `ui_driver`. No new Vulkan or shader check is unavailable; shaders
remain unchanged. Task 4.3 is checked using the integrated build, 469/469 full
Debug tests, resumed 24/24 UI cases and this full Vulkan run. Scene authoring and
physical observations are still separate acceptance requirements.

### Save-path failure found during authoring

The Save As attempt exposed an actual editor failure: an empty Path reached
the throwing `std::filesystem::absolute` overload and terminated the editor.
`37-path.png` and `editor-errors.log` under
`build/character-authoring-section4-resume/` show the empty field and exception.
The attempted text injection did not populate the field; no scene was saved.
Captures taken after that process exited are not editor acceptance evidence.

EditorDocument now rejects empty paths and reports path-resolution errors as
filesystem diagnostics before replacing the document, saved path or pending
action. Two regression tests cover unsaved interiors and previously saved
documents, including retained edits, dirty/saved revision, pending Close and
undo. Independent review found no defect in the new path-resolution handling.
`ctest --preset debug -R '^EditorDocument\.' --output-on-failure` passed 9/9,
exit 0, 3.70 s. The full default Debug build passed after closing an orphan
editor from an earlier sandbox launch that had locked the executable.

The temporary input helper now sends native Unicode keyboard events and waits
after releasing Ctrl; typed values must be observed before Save. Desktop input
uses the already authorized batch wrapper, with a normal editor window and no
always-on-top. These helper changes do not create or modify level JSON.

After the fix, `ui_test_runner` ran the full non-Vulkan Debug suite again:
471/471 passed, exit 0, 125.56 s (460 unit, six boundary, five process).
Log: `build/character-authoring-section4-path-fix-tests.log`. This includes the
existing ImGui and native-child cases and the two new document regressions.
Strict OpenSpec validation, diff whitespace and formatting checks also passed.

## Final independent-scene acceptance (2026-09-09)

Tasks 4.1, 4.2 and 4.5 are accepted with the observation limits below. This
section supersedes the earlier pending/interrupted section-4 status; those
entries remain as implementation history, not current blockers.

### UI-authored scene and saved-file Play

The retained second scene is
[`Нейтральный маршрут 02.level.json`](evidence/Нейтральный%20маршрут%2002.level.json).
Its SHA-256 is
`935d4eaba3587d73419a772aac9f597b74733bb0ab3becc1080f6790d5249b78`.
It was created from New Interior and changed only through supported editor UI
operations. Read-only JSON inspection confirmed the resulting values; neither
scene JSON nor runtime/editor source was edited to create or repair the scene.
The earlier `Нейтральный маршрут 01.level.json` is not the accepted scene.

The scene contains a floor and six boundary solids, a closed unlocked door,
two initially enabled shadow-casting point lights, one prepared mannequin,
four marks, one ordered route, two named starts, and two distinct actor-linked
spatial audio sources. The non-looping footstep cue is ambience with no caption;
the non-looping interaction cue is essential and has its matching caption.
Both sources have autoplay disabled. All fields were authored, saved, reopened
through the literal Unicode path and accepted by the editor validator.

The initial actor mark is `(0,0,3)`; its route visits `(0,0,1.5)`, `(0,0,-1.5)`
and `(1.8,0,-2.8)` at 0.5 m/s, ending with yaw 0 and `interact`. The additional
`entry-1` start at `(0,0,1.5)`, player yaw 90, supplies a reproducible player
obstruction. The `default` start remains separate at `(-1.5,0,4)`, yaw -45.

The editor was launched from `%TEMP%`. Windows process inspection confirmed
that editor PID 24640 spawned ordinary `near_laugh.exe` PID 16696 with the
literal saved Unicode `--level` argument and `--entry "entry-1"`. The child
inherits the editor working directory; it was not a fixture or direct CLI
substitute. A second ordinary Play process, PID 20936, supplied the final
uninterrupted event capture. The editor subsequently reported game exit code 0.

### Observed behavior

- The actual mannequin mesh appeared in the editor's silent clip snapshot.
  Start/pause/stop and schematic route start/stop left the document clean.
  The final stopped capture has no active transient time or segment status.
- Two stationary game captures 10.6 seconds apart show the actor stopped before
  the player. After the player stepped aside, the actor advanced to the door.
- Two further captures 14 seconds apart show the actor waiting at the closed
  door with its posed shadow on the leaf. An ordinary `E` interaction opened
  the door; the actor continued through it without changing authored data.
- The uninterrupted fresh-run sequence shows the walking actor and matching
  animated shadow on the opened leaf/floor. After the route, the actor is in
  the rear room with its final idle presentation and coherent floor shadow.
- The essential caption `Манекен: Тихий щелчок.` appears in frames 040-042 of
  `section4-fresh-door-finale`, between neighboring caption-free frames.
  The complete 100-frame working series ran from 12:55:24.5688762Z to
  12:55:54.9166877Z. Bright-pixel analysis located candidates; the acceptance
  decision used visual inspection of the actual caption, not the heuristic.
- The prepared character faces +Z at yaw 0, unlike the player camera's yaw
  convention. Final presentation is consistent with that direction. Existing
  controller tests establish the 0.02 m/1 degree final-interaction gate and
  unchanged physics-to-render yaw; no angle was measured from a screenshot.
- The user confirmed physical audibility during ordinary Play: `да, звук есть`.
  Authored cue/source links, the retained event tests and the actual caption
  establish the software event path. No separate per-cue listening matrix,
  loopback recording or hardware-latency measurement was collected. This does
  not upgrade P07b's accepted hardware-timing limitation into measured evidence.

The [retained evidence index](evidence/final-authoring/README.md) identifies
selected unmodified PNGs, the scene checksum, process provenance and copied
action journals. Editor route inspection remains explicitly schematic: no
collision, door operation or sound. It is not physical Play acceptance.

Several automation attempts were excluded: wrong numeric-field targeting and
missing text-input coordinates were corrected through UI and checked after
reopen; an early capture began after the one-second event; F5 does not restart
routes in ordinary `near_laugh`. F5/F6/P/M belong to the interactive character
fixture, so the final evidence used a fresh editor Play process instead.
Default key fields in a capture log row are not executed key events.

### Checks, decision and handoff

The implementation-check results already recorded above remain the applicable
baseline: 471/471 Debug tests (including 24 real ImGui authoring checks), and
12/12 Vulkan smoke tests after final GPU teardown. The Vulkan run preceded the
subsequent non-rendering empty-save-path fix; the 471-test Debug run includes
that fix. This final authoring run changed scene/evidence/documentation only,
not application source, and did not rerun those builds or suites. Strict
OpenSpec validation was also confirmed during this resumed session.

P07c authoring/ordinary Play acceptance is complete with the explicit audio
observation limits above. T2/P07 readiness rests on all three stages: P07a's
prepared animation/rendering evidence, P07b's accepted route/collision/audio
and nine-sample Release evidence, and this P07c independent authoring workflow.
See the [acceptance handoff](handoff.md). P06 still needs its detailed plan and
review against P04/P10/P07; P05 remains dependent on P06 and the complete P07
chain. No dependent implementation, main-spec synchronization or archive was
performed by this apply step.
## Archive record (2026-09-09)

After explicit user approval, the `level-editor` and
`level-object-placement` deltas were synchronized into their main specs.
The post-sync comparison found no remaining delta operations, with existing
scenarios preserved. OpenSpec main-spec validation passed: 28 passed, 0 failed.

The complete change, including its accepted scene and evidence, was archived as
`2026-09-09-add-character-authoring`; roadmap and handoff links were updated.
All artifacts and all 18 tasks were complete. The archive operation did not
rerun application builds, tests, Vulkan checks or physical listening, and it
does not change the accepted audio-observation and hardware-latency limits.
