## 1. Dependency and representative content

- [x] 1.1 Recheck the implemented main specs and current level version against this design, resolving any intervening schema conflict before coding; verify the migration plan still starts from v6 and `openspec validate add-spatial-audio-and-captions --strict` passes.
- [x] 1.2 Pin miniaudio 0.11.25 to an immutable revision and add the concrete private audio target with RAII ownership; verify debug configuration/build, public-header compilation and game/editor dependency boundaries without adding backend interfaces.
- [x] 1.3 Prepare the bounded clip/caption catalog and representative radio, ring, footsteps, phone conversation and invitation assets; verify the PCM profile, Russian caption correspondence and meaningful contradiction, and deliver recording/generation instructions, asset hashes and licenses/provenance.
- [x] 1.4 Package the pinned Noto Sans font and license, add executable-relative audio/caption/font resource copying for game, editor and fixture/smoke targets; verify the resource-layout probe from another working directory and a path containing Unicode.

## 2. Versioned authored audio data

- [x] 2.1 Add concrete cue/source/room/connection definitions and shared validation with the design's bounds and reference rules; verify valid empty/full profiles, duplicate IDs, non-finite derived bounds, overlapping rooms, invalid gains, illegal autoplay and broken cue/room/door links through behavioral world tests.
- [x] 2.2 Extend the strict codec to v7 while preserving exact v2–6 parsing and mappings, including a dedicated legacy v6 fixture; verify missing/unknown fields, rejection of audio fields in old versions, clean opening without writes and preservation of all prior geometry/assets/entries/doors.
- [x] 2.3 Implement canonical v7 saving and editor migration notices, and update packaged baseline levels with empty audio; verify semantic and byte-identical repeated round trips, ordered audio records, explicit old-format conversion and save-failure preservation.

## 3. Playback and authored transmission

- [x] 3.1 Implement selected-clip native-path loading, PCM validation, deduplicated predecode and checked duration/decoded-memory bounds; verify missing/corrupt/stereo-spatial/oversized assets fail with identity/path context and unselected files are never required.
- [x] 3.2 Implement owned voices, explicit start/stop, looping, gain, mute and moving source/listener updates; use the real device-free backend to verify nonzero PCM output, left/right changes, near/far attenuation, continued cursor motion while muted, no restart during source movement and bounded voice lifetime.
- [x] 3.3 Implement deterministic room membership and strongest-path transmission from accepted door angles with gain smoothing; verify same/outside/disconnected regions, shared faces, multi-floor boxes, negative opening angles, blocked partial poses, unchanged gain on lock-only changes, alternate paths and cycles.
- [x] 3.4 Implement device warning/silent fallback and ordered callback shutdown, including failure after partial initialization; verify controlled initialization/loss failures preserve usable cue state and release acquired resources without stale accesses or automatic replay.

## 4. Cue timing and caption policy

- [x] 4.1 Implement bounded caption loading and structural validation, keeping catalog identity validation independent of device/GPU construction; verify malformed UTF-8, invalid offsets, overlap, overlong text, missing captions and clip-duration mismatches produce contextual failures.
- [x] 4.2 Implement the shared concrete coordinator with injectable active time, one foreground cue, idempotent active-source start, busy refusal, explicit cancellation and ambient ducking; verify observable start/busy/cancel/complete results and caption selection under irregular update intervals without hardware-completion-driven ordering.
- [x] 4.3 Implement mute, explicit suspension, offset-preserving resume, loop caption policy and cursor drift correction/silent fallback; verify long suspended intervals add no time, canceled/completed cues do not restart, loop offsets wrap correctly and muted/device-free runs retain essential text and identical cue order.

## 5. Game text presentation

- [x] 5.1 Implement trusted-font validation, glyph coverage, immutable atlas preparation and bounded pure text layout using the existing pinned stb dependency; verify Russian Ё/ё and punctuation, missing/corrupt fonts, unavailable glyphs, safe UTF-8 handling, wrapping and full content fit at 800x600 through 3840x2160 including HiDPI cases.
- [x] 5.2 Extend frame presentation with bounded resolved text and implement the renderer-private text pipeline, backing panels and fenced glyph buffers; verify empty text, successive distinct strings and lit/dark scene overlays in rendering tests without adding editor UI to game dependencies.
- [x] 5.3 Package compiled text shaders and implement text allocation/recovery/teardown paths; validate the SPIR-V and exercise failure injection, compatible atlas retention, changed attachment formats and multiple frames in flight in Vulkan smoke with no validation errors.

## 6. Runtime composition and playable fixture

- [x] 6.1 Compose selected-content preflight, audio/coordinator ownership, autoplay ambience, the interpolated listener and accepted door transmission into the runtime; verify immutable level data, existing player/door behavior, coherent poses and resource cleanup when later construction fails.
- [x] 6.2 Suspend before minimized event waits and resume from preserved cue offsets; verify minimize/restore, close while minimized, cursor release, consumed inactive input and skipped/recovered render outcomes do not advance suspended time or replay cues.
- [x] 6.3 Add the separate apartment-based audio fixture level and explicit test/demo executable with a compiled bounded sequence and documented restart/mute/suspend controls; verify its required-source checks, moving footsteps, full phone-before-invitation ordering and absence of implicit fixture execution in ordinary game launches.
- [x] 6.4 Run the same fixture sequence in deterministic audible-rendering and silent modes; verify essential caption order, explicit cancellation/restart, foreground exclusivity and open/closed/obstructed-door transmission without depending on a physical device or P05 narrative state.

## 7. Editor authoring and audition

- [x] 7.1 Add concrete audio selections, property controls, source markers/placement, room wireframes and connection links; verify real editor UI interactions, nearest-surface placement, cancellation/capture, bounds, list/viewport agreement and no collision/runtime simulation dependency.
- [x] 7.2 Extend command history for audio add/edit/duplicate/delete and atomic cue/room/door reference updates on rename; verify allocated IDs persist through undo/redo, deletion retains broken incoming references with diagnostics, undo restores validity and saved-state identity follows the existing history rules.
- [x] 7.3 Add isolated snapshot audition with Start/Stop, mute, pause/resume, editor-camera listener and Russian caption display; verify explicit-only playback, clean dirty/history state, camera-driven spatial changes, and stop/clear on edits, undo/redo, replacement, minimize and Play without automatic restart.
- [x] 7.4 Extend validation presentation and saved-file Play preflight to selected audio/captions/font; verify broken resources launch no process, malformed loads retain the prior document, safe invalid links remain editable, Unicode paths work and no-device availability alone does not block Play.

## 8. Integrated acceptance and documentation

- [x] 8.1 Follow `docs/DEVELOPMENT.md`: run `cmake --preset debug`, `cmake --build --preset debug` and `ctest --preset debug --output-on-failure`; verify affected targets and the full deterministic suite pass, with tests protecting behavior and lifetime boundaries rather than incidental source syntax.
- [x] 8.2 Run `ctest --preset vulkan-smoke --output-on-failure` with captioned game/editor fixtures, resize, minimize/restore, recovery and partial-construction/teardown coverage; retain results including validation errors observed through final destruction, and explicitly record unavailable GPU checks.
- [ ] 8.3 Perform desktop listening acceptance with headphones/speakers: distinct source positions, listener rotation, moving footsteps, open/closed/blocked door gains, ambient ducking, pause/minimize/resume and shutdown; measure caption/audio drift and output latency against the design's 100 ms plus device-latency target and record hardware and findings.
- [x] 8.4 Complete and record the audible, muted and no-device telephone fixture, checking intelligible temporary speech, the completed-call contradiction, player-safe labels, Russian text at supported sizes and editor author/save/audition/Play from an unrelated working directory; identify any acceptance that still needs human observation rather than claiming it from compilation.
- [x] 8.5 Update architecture, rendering, gameplay, development/resource provenance and roadmap documentation to the delivered P04 behavior, format migration, controls and known limits; verify links and keep full narrative progression assigned to P05.
- [x] 8.6 Review `git diff` and run `git diff --check` plus `openspec validate add-spatial-audio-and-captions --strict`; verify every implemented requirement has recorded evidence or an explicit outstanding limitation, and archive only after the implementation and required acceptance are finished.

Archive note, 2026-09-07: the user confirmed “Все работает, можно архивировать”.
Task 8.4 is accepted on that confirmation. Archive is explicitly authorized with
8.3's quantitative latency/drift evidence still unavailable; its checkbox stays
open. See [validation.md](validation.md) for the evidence and its limits.
