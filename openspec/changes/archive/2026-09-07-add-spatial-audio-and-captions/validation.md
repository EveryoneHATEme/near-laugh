# P04 implementation and acceptance evidence

## Status on 2026-09-07

Implementation, automated verification and documentation are delivered.
The user confirmed that everything works and explicitly authorized archiving
on 2026-09-07. Task 8.4 is accepted on that confirmation. Task 8.3 remains
partially verified because no hardware details or quantitative output
latency/drift measurements were supplied; this limitation is retained in the
archive rather than represented as a measured pass.

The starting codec and implemented main specs were v6; there was no intervening
schema conflict. The delivered codec writes v7, reads exact v2–6 profiles and
does not rewrite a file on opening. The dedicated legacy v6 fixture preserves
the prior packaged prototype. P05 narrative progression remains outside P04.

## Required build and test checks

Run from `D:/programming/near-laugh` with the debug preset on Windows:

| Command | Result on the final code |
| --- | --- |
| `cmake --preset debug` | Passed |
| `cmake --build --preset debug` | Passed, including game, editor, demo, tests and resource probes |
| `ctest --preset debug --output-on-failure` | 322/322 passed; 50.66 seconds |
| `ctest --preset vulkan-smoke --output-on-failure` | 8/8 passed; 32.14 seconds |
| `spirv-val --target-env vulkan1.3 resources/shaders/caption_vertex.spv` | Passed |
| `spirv-val --target-env vulkan1.3 resources/shaders/caption_fragment.spv` | Passed |
| `git diff --check` | Passed after final review |
| `openspec validate add-spatial-audio-and-captions --strict` | Passed |

Final resource/document review also passed: 18 UTF-8 documents, 28 local links,
all ten audio/caption manifest hashes, PCM profiles and exact durations, pinned
font/license hashes and packaged copies. The fixture's non-audio level data
matches the apartment baseline exactly.

The deterministic preset includes 313 unit cases, six dependency/public-header
checks and three process checks. Test additions exercise behavior, ownership,
failure handling and real ImGui input; the source boundary checks constrain
dependencies rather than audio implementation syntax.

The eight Vulkan cases are `vulkan_smoke`, `vulkan_audio_runtime`,
`vulkan_interior_apartment`, `vulkan_interior_lower-landing`,
`vulkan_smoke_validation_error`, `vulkan_lifecycle_smoke`,
`editor_vulkan_smoke` and `editor_vulkan_construction_failure`.
Expected-failure cases deliberately fail their child process and pass only when
the harness observes that failure. Normal paths recorded no error-severity
Vulkan validation messages. Game and editor validation sinks remain alive
through final GPU destruction. No GPU check in the required preset was skipped.

## Evidence by task group

Paths below are relative to the repository root.

| Tasks | Implemented behavior and evidence |
| --- | --- |
| 1.1–1.4 | Immutable miniaudio 0.11.25 revision `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`; private backend compilation; public consumer/header and dependency checks. Five selected mono PCM16/48 kHz clips with corresponding Russian captions. `resources/audio/fixture.sources.json` records hashes and durations; generator, text and provenance are in `scripts/prepare_audio_fixture.py` and `resources/audio/README.md`. Pinned Noto Sans hash and OFL license are in `resources/fonts/README.md`. Resource probes pass from another working directory and a copied Unicode executable directory. |
| 2.1–2.3 | `AudioDefinitions` tests cover all collection bounds, duplicate identities, broken references, finite/representable numbers, touching/overlapping rooms, gains and autoplay rules. `AudioPersistence`, `SceneAuthoring` and existing level/editor tests cover exact legacy parsing, strict v7 fields, no-write opening, canonical repeated round trips, ordered data, explicit migration and failed-load/save preservation. |
| 3.1–3.4 | `AudioPersistence` covers selected-only native-path loading, missing/corrupt PCM, unsupported spatial stereo, duration/file/decode-memory limits, deduplication and contextual failures. `AudioPlayback` uses the actual miniaudio offline mixer to check PCM energy, stereo direction/rotation, attenuation, source motion without restart, mute cursor progress, suspension, bounded voices and release. `AudioTransmission` covers half-open membership, floors/outside, strongest paths/cycles, negative authored door angles, accepted partial poses and lock independence. Controlled device/voice failures and device loss preserve cue state while releasing the backend. |
| 4.1–4.3 | `AudioPersistence` rejects malformed UTF-8, invalid timing, overlap, missing captions and overlong content. `CueCoordinator` covers explicit start/busy/cancel/complete, a single foreground cue, ambient ducking and lane selection, irregular active time, long suspension, resumed offsets, loop modulo, mute/silent order and drift fallback. A regression verifies that rapid presentation frames do not count a pending asynchronous seek as several failed corrections. |
| 5.1–5.3 | `CaptionFont` verifies the trusted font before parsing, Russian Ё/ё and punctuation, glyph coverage, all packaged caption fits at 800x600 through 3840x2160 and narrow/tall HiDPI cases, invalid UTF-8, empty/small layouts and bounded failures. Vulkan smoke draws changing foreground/ambience text on lit/dark scenes and empty frames, uses successive fenced buffers, retains the atlas through compatible recovery, rebuilds pipelines for changed formats, and checks partial atlas/descriptor/buffer/pipeline failures and balanced teardown. Both caption SPIR-V modules validate. |
| 6.1–6.4 | `vulkan_audio_runtime` exercises the actual runtime in device and silent modes: ordinary launch does not start the demo, listener/render pose composition, release of cursor without suspension, recovery without replay, minimized event waits, offset-preserving restore, close while minimized and failure after audio construction. `AudioFixture` renders the full sequence in offline audible, muted and silent modes with identical seven essential segments, nonzero versus zero PCM energy, foreground exclusivity, a complete phone call before the two-second gap and invitation, restart/cancel and inactive controls. Door gains are checked at closed/accepted partial/open poses. |
| 7.1–7.4 | `EditorAudio` covers all four audio record types, stable IDs/history, atomic renames including previously broken incoming references, delete diagnostics and undo, Unicode Save As, nearest-surface placement and wire-edge picking. Real `EditorUiInteraction` input drives add/select/duplicate/delete/undo buttons and shortcuts. Audition tests use validated snapshots and the real offline mixer; edits/history/replacement stop playback without dirtying the document. Editor Vulkan smoke covers captioned audition, edit/undo stop, minimize/restore without restart, Unicode save and selected-resource Play refusal without creating a child. Existing process tests check saved-file preparation and native child arguments. |
| 8.1, 8.2, 8.5, 8.6 | Full commands above pass; architecture, gameplay, rendering, development, roadmap and provenance documentation describe the delivered ownership, v7 migration, controls and remaining limits. Diff and local documentation/resource links reviewed. User acceptance and its evidence limits are recorded below. |

## Desktop runs and visual observations

The demo ran from the unrelated working directory
`D:/programming/near-laugh/build` in device, muted and explicit `--silent` modes.
Each run passed the ring, footsteps, phone call, invitation and finished capture
checkpoints over 32 seconds and closed through its window close request without
a shutdown timeout. Device/muted logs had no audio initialization or silent
fallback warning. This establishes successful device-path execution, not what
a listener heard or which physical endpoint was active.

Vulkan reported `AMD Radeon(TM) Graphics`, graphics/present queue 0, swapchain
format 50, two images and a 1024x768 framebuffer. Local images
`build/p04-{device,muted,silent}-{ring,steps,call,invitation,finished}.png` and
the corresponding `.stdout.log`/`.stderr.log` files are ignored validation
artifacts. Inspected call/invitation images show complete Russian text and
player-safe labels in the lower foreground lane, readable white glyphs and a
dark backing over the dark room. The finished capture has no stale caption.
Earlier ring captures show foreground and ambience lanes together.

The call caption says “Я за городом. Сегодня не вернусь. Никому не открывай.”
The later caption says “Это я. Я уже дома. Открой дверь, пойдём на кухню.”
The latter label is “Голос за дверью”; it does not reveal a hidden identity.
Full sequence ordering and the completed-call segment are independently checked
by the deterministic fixture test. Screenshots alone do not establish speech
intelligibility or perceived narrative effect.

Desktop stderr contains two environment warnings: injected layers
`VK_LAYER_OW_OVERLAY` and `VK_LAYER_OW_OBS_HOOK` advertise Vulkan 1.2 while the
application requests 1.3. No error-severity messages appeared in those logs.

## User acceptance and archive authorization: 2026-09-07

After the implementation report identified the outstanding human checks, the
user invoked `openspec-archive-change` and stated:

> Все работает, можно архивировать

This is recorded as qualitative acceptance of the delivered fixture/editor
behavior and authorization to archive. It closes task 8.4 without inventing a
per-scenario listening transcript, a tested display-size list or measurements.

Task 8.3 remains unchecked for its quantitative evidence: the active physical
endpoint, hardware/settings, measurement method, output latency and observed
caption/audio drift were not supplied. The design's 100 ms plus measured device
latency target has therefore not been established by a desktop measurement.
Windows enumerated Realtek High Definition Audio, AMD High Definition Audio
Device, AMD Streaming Audio Device and Nahimic mirroring device; enumeration
does not identify the active physical output or measure latency. The user
authorized archive after being informed that this acceptance work was open.

All seven capability deltas were synchronized before archival: two new main
specs and five updated specs, with 18 added and four modified requirements.
Comparison verified every delta and preservation of prior unrelated
requirements/scenarios. Strict main-spec validation passed for all 25 specs.
Archival changes only planning records and documentation; the earlier build,
322 deterministic tests and eight Vulkan results remain the implementation
evidence, without an unnecessary rebuild of unchanged code.
