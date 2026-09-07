## Context

See [proposal.md](proposal.md) for motivation and acceptance. P03 and P02 are
archived; the baseline is the strict version-6 document, including authored
doors and apartment props. `Engine::tick` already owns fixed simulation,
interpolated eye pose, door state, minimized waits and renderer outcomes.
`AuthoredInteraction::update` returns concrete door results which the runtime
currently discards. There is no audio, game text, pause menu or narrative runner.
The editor has transactional documents, bounded command history, ImGui overlays,
and selected-asset preflight before launching a separate game process.

## Goals / Non-Goals

**Goals:** integrate audible, captioned authored cues with explicit ownership;
make the P04 fixture independently playable; make its assets and room links
editable and diagnosable; preserve current world, input and GPU guarantees.

**Non-Goals:** P05 story progression, P07 character animation or footstep gait,
P09 save-game state, P12 menus/settings, final voice production, music systems,
HRTF, reverberation, diffraction, ray-traced occlusion, streaming, arbitrary
code/scripts, a backend interface, or an event bus. Moving footsteps in this
milestone use a concrete off-screen fixture source. Ordinary door interactions
retain current behavior; interaction sound mappings remain later content work.

## Decisions

### 1. One concrete audio owner, shared by game and audition

Add `near_laugh_audio`, privately linking a pinned miniaudio 0.11.25 source
revision. Pin the full commit/hash during dependency integration, never a
floating branch. Its non-copyable RAII owner keeps backend objects at stable
addresses, owns decoded clips and voices, and exposes only the operations used
here: start/stop cue, source pose/gain, listener pose, mute, suspend/resume and
playback status. World definitions contain no backend types or resource paths.
Runtime and editor application composition link this module; editor document
code retains no audio device, runtime or physics dependency.

miniaudio supplies playback, spatialization and a device-free engine that can
render PCM for tests. Use its normal device path in the application and offline
PCM rendering for behavioral tests of the same implementation. Its device
callbacks must never mutate gameplay or destroy owners; notifications transfer
only bounded status to the main thread. Stop/join device activity before voice,
decoded-buffer and engine destruction. Do not add project worker threads.
These integration facts follow the [upstream manual](https://miniaud.io/docs/manual/index.html)
and [0.11.25 release](https://github.com/mackron/miniaudio/releases/tag/0.11.25).

Alternative: a low-level device plus handwritten spatial mixer duplicates
available work. Commercial middleware and an interchangeable-backend interface
add tooling and ownership concepts with no current caller.

### 2. Small packaged clip and caption profile

Accept only RIFF/WAVE signed 16-bit PCM at 48 kHz: mono for localized cues,
mono or stereo for nonspatial ambience. Predecode selected clips once before
playback, checking duration (0 < duration <= 120 seconds), byte arithmetic and
a 128 MiB aggregate decoded-float budget. Reject other profiles even if the
library can decode them. No streaming or load/decode work occurs in a playback
callback. Logical clip and caption IDs resolve through a finite game-owned
catalog under the executable-relative resource root, using native file reads
so Unicode resource paths survive. Missing unreferenced clips are irrelevant.

Caption catalog entries contain ordered nonoverlapping start/end offsets,
UTF-8 text and a player-safe source label, relative to their clip. Each segment
has at most 160 Unicode scalar values including its label, lasts at least one
second and ends no later than the clip. Selected dialogue and essential cues
require complete meaningful captions, with gaps allowed for actual silence.
This last semantic check needs manual review, beyond structural validation.
Ambience captions are optional. The catalog is packaged content, not an editor
for dialogue writing; the level editor selects clip/caption references.

Package representative radio, telephone ring, footstep, and temporary
spoken phone/invitation clips with matching Russian captions and provenance.
Use owned recordings or appropriately licensed assets; record generation or
recording steps and attribution. Tones alone cannot validate the spoken clue.
Final wording and acting can change without altering this design.

Alternative: compressed/streamed formats save space, but the bounded fixture
does not justify decoder variation, streaming lifetime or seek uncertainty.

### 3. Version 7 contains concrete audio authoring data

Add a required `audio` object to the v7 level, containing these arrays:

| Collection | Bound | Fields |
| --- | --- | --- |
| `cues` | 0–128 | durable `id`, catalog `clip`, nullable catalog `caption`, `kind` (dialogue/essential/ambience), `loop`, `spatial` |
| `sources` | 0–64 | durable `id`, `cue` reference, `position`, `gain`, `near_distance`, `far_distance`, `autoplay` |
| `rooms` | 0–32 | durable `id`, finite `center`, positive `half_extent` |
| `connections` | 0–64 | durable `id`, nullable `room_a`/`room_b`, nullable `door`, `closed_gain`, `open_gain` |

IDs use the existing `[a-z][a-z0-9-]{0,63}` syntax and are unique within each
collection. Nullable room endpoints denote the outside region; endpoints must
differ. A door can belong to at most one connection. Gains are in [0,1], with
closed_gain <= open_gain; a doorless connection uses open_gain. Spatial source
distances satisfy 0 < near_distance < far_distance <= 100 metres. Room interiors
cannot overlap, but faces can touch; sources need no collision or support.
`autoplay` is allowed only for looping ambience. Other starts are explicit.
Dialogue/essential cues are finite one-shots and must reference captions.
Spatial cues require mono clips. Clip/caption compatibility is checked during
selected-resource preflight, while the world validates catalog identities,
field bounds and level references without opening devices or render resources.

Exact v2–6 shapes retain all prior mappings and normalize to empty audio arrays.
Opening stays clean and never writes a file. Explicit saves write canonical v7,
including stable array ordering and every prior authored field. Preserve v6 as
a legacy branch instead of merely changing the current-version constant. Older
versions reject `audio`; v7 rejects missing `audio` and unknown fields.

Alternative: sidecar source placement complicates transactional save and
reference identity. Serializing middleware nodes or arbitrary asset paths would
violate the existing authored-data boundary.

### 4. Authored transmission, with no physics acoustic queries

Listener position/orientation comes from the same interpolated eye pose used
for the view; explicitly test coordinate handedness and left/right orientation.
Source position has a run-local override used by the moving fixture. Room
membership is derived from each current position using half-open bounds; any
residual boundary tie selects the lexicographically smallest room ID. No match
means outside. Room volumes only classify positions; they do not affect physics.

For a linked door, openness is clamp(abs(accepted angle / authored opening
angle), 0, 1). Interpolate closed_gain to open_gain by this value. Lock state
alone has no acoustic effect. Between different regions choose the largest
product of connection gains over paths in the small bounded graph; gains <= 1
make cycles unhelpful. Same-region transmission is 1; disconnected regions use
a fixed 0.1 wall-leak gain. This is an authored attenuation convention, not a
claim to model real propagation. Recompute from current accepted poses.

Use linear distance attenuation: full gain through near_distance, linearly
falling to zero at far_distance, and zero beyond. Output gain multiplies source,
distance, transmission and master gain. Nonspatial ambience bypasses position,
distance and rooms. Disable Doppler; smooth changing gains over 50 ms to avoid
clicks. Test relative output energy and channel balance, not backend internals.

Alternative: geometric occlusion would couple audio to collision proxies and
require a more complex physical model without improving this milestone's test.

### 5. Cue clock, ordering, interruption and failure policy

A small concrete cue coordinator owns run-local playback instances and caption
selection, independently of device completion and rendering. It advances from
an injected monotonic active-time clock, freezing during explicit suspension or
minimization. It is not the fixed-step accumulator: ordinary simulation
catch-up limits must not gradually desynchronize recorded dialogue. Native
device completion is diagnostic, never a story transition.

There is at most one active voice per source and one foreground dialogue or
essential cue globally. Starting the same already-active source is idempotent;
replay after completion requires a new explicit start. A competing foreground
request is refused with a busy result, never silently queued or stolen. The
caller can explicitly cancel, then start another cue. Cancel stops both voice
and text; canceled/completed instances do not resume after recovery. Up to 64
voices can coexist, bounded by source count. While foreground content runs,
ambient gain is multiplied by 0.25. Foreground text has a reserved display lane;
one optional ambience caption uses a separate lane, chosen by greatest effective
gain with source-ID tie break. Loop captions show on start for their authored
interval and do not reappear every loop. Essential text remains visible even
when its audio gain is zero or the source is out of range.

Compare active audio cursor and cue time on the main thread. Seek to the
expected offset on resume or drift exceeding 100 ms; use modulo clip duration
for loops. Report repeated inability to synchronize and enter silent mode,
preserving the cue timeline and captions. Normal audible acceptance targets
caption alignment within 100 ms plus measured output-device latency. Muting is
gain zero and never pauses time. Missing/corrupt selected content or font is a
startup/preflight error; device initialization/loss instead reports an actionable
warning and continues silently with captions. Do not retry or restart voices
automatically on device recovery in this milestone; restarting the run or an
explicit new editor audition retries initialization.

Before a minimized event wait, suspend playback and freeze cue time; restore
from the same offset without including the blocked duration. Cursor release
retains its existing meaning and is not a pause action. Expose coordinator
suspension for the fixture/tests and later session composition without adding
a menu now. Ordinary resize/recovery retains the cue clock and all instance
identities, and must not replay starts. Prolonged unexpected main-thread stalls
are recorded as synchronization defects during listening acceptance; no new
render/audio scheduling architecture is introduced speculatively.

Alternative: hardware completion as the cue clock would make muted/headless
runs and story ordering device-dependent. General command queues and priority
schedulers are unnecessary for one foreground clue.

### 6. Russian text without a game dependency on editor UI

Package Noto Sans Regular and its license/provenance from the
[upstream font collection](https://github.com/notofonts/noto-fonts/blob/main/hinted/ttf/NotoSans/NotoSans-Regular.ttf).
Use `stb_truetype` from the already pinned stb revision to bake the selected
Latin, Cyrillic (including Ё/ё), digits and punctuation into immutable atlases
at startup. Only the packaged trusted font is accepted; verify its recorded
content hash before parsing and report a missing/corrupt font with its path.
Glyph coverage of selected captions must validate instead of drawing boxes.
The [upstream header](https://github.com/nothings/stb/blob/master/stb_truetype.h)
provides glyph metrics and bitmap rasterization; layout and Vulkan upload
remain project code.

Use a pure bounded UTF-8 layout helper and a renderer-private alpha-blended
screen-space glyph pipeline after the scene, with depth test/write disabled.
`FrameRequest` borrows only bounded resolved text/label presentation for the
synchronous call, not cue IDs, audio handles or gameplay objects. Bake 24, 32,
48 and 64 pixel sizes once; choose by framebuffer height. White glyphs over a
dark panel, 5% safe margins, word wrapping and at most four foreground lines
plus two ambience lines support 800x600 through 3840x2160, including HiDPI
framebuffer scaling. Reject selected caption content that cannot fit at the
minimum supported size; below that size render a bounded best-effort layout
without unsafe draws. No silent truncation of essential content at supported
sizes. Editor property/validation UI also loads the packaged Cyrillic font.

Font atlas/descriptor owners outlive draws and survive compatible swapchain
recovery. Each frame slot's glyph vertices are overwritten only after its fence;
pipeline recreation handles attachment format changes. Failed construction
cleans up partially acquired resources. Existing scene descriptors, lighting
push range and static resource guarantees stay intact.

Alternative: linking the current editor UI into the runtime conflicts with
the standalone-editor boundary. A full UI toolkit or complex-script shaping
system exceeds the present Russian-caption requirement.

### 7. Authoring and the independent P04 fixture

Extend concrete editor selections and commands for audio sources, cues, rooms
and connections. Source markers are viewport-pickable and placeable on the
nearest suitable surface using an explicit height/outward offset. Rooms use
numeric box editing and selectable wireframes; connections and cue definitions
are list-editable records, with links drawn when selected. No portal mesh tool.
Renaming cues, sources, rooms or doors updates existing affected audio references
in the same undo step. Deletion leaves incoming references broken and visible
for repair, never cascades or silently retargets them; undo restores validity.
Duplication allocates a fresh durable ID and copies outgoing references only.

Audition is explicit Start/Stop with mute and pause/resume, one selected source
at a time, using a validated snapshot, current editor camera as listener and
authored initial door angles. No automatic playback on Open, selection or edits.
Any committed edit/undo/redo, document replacement, minimization or Play request
stops audition and clears its captions; restoring the window does not restart
it. Camera navigation can update the audition listener without dirtying the
document. Audition uses the shared coordinator and displays the same caption
content in an editor panel. Failure leaves the document editable. Play preflight
decodes selected audio and validates captions/font as well as current assets;
device availability is not a preflight requirement.

Add a separate packaged `audio-captions.level.json` based on the apartment and
a small test/demo executable composed with the existing runtime through a
repository-internal fixture entry point. Its explicit launch selects a compiled
P04 sequence: radio loop and localized phone ring; moving off-screen footsteps;
completed temporary phone conversation; a pause; then the contradictory
invitation behind the door. The authored fixture contains the required source
IDs; missing IDs refuse fixture startup. The ordinary game does not recognize
level filenames or run this sequence automatically. The fixture can be restarted,
muted and suspended through documented fixture-only controls. It uses concrete
cue calls, not serialized sequence actions or a new public runtime API.
Door state affects transmission, but door action sound mappings are deferred;
the fixture exercises explicit cue calls without adding implicit interaction
bindings or further serialized fields.

This fixture proves P04; full M2 progression remains P05 work. Its captions use
temporary neutral labels such as a telephone voice or a voice beyond the door,
never a hidden actor's identity. Run it with the door closed/open/obstructed,
with audio muted and with no device; assess the contradiction manually.

Alternative: embedding fixture actions in every ordinary level run or building
a generic timeline editor prematurely would consume P05 scope.

## Risks / Trade-offs

- Authored room boxes can misclassify thresholds → show volumes and current
  region/gain during audition, test shared boundaries and multi-floor rooms.
- Gain-only doors lack muffling and diffraction → listen to the fixture first;
  additional effects require evidence and an updated design, not hidden scope.
- Device buffering and stalls can impair synchronization → offline cursor tests,
  measured desktop latency/drift and explicit silent fallback; retain findings.
- Temporary voices can obscure the clue → verify intelligible Russian recordings,
  caption content and the muted playthrough; record provenance and limitations.
- New text resources affect GPU lifetime → allocation-failure and multi-frame
  resize/minimize/recovery smoke coverage, including teardown validation.
- Other planned changes also evolve levels → recheck the actual main schema at
  apply time; revise this plan before assigning an already-used version number.

## Migration Plan

Implement from the current v6 baseline and retain exact v2–6 test fixtures.
Ship v7 packaged levels with empty audio where unused and the separate P04
fixture with selected clips, captions, font, licenses and text shader binaries.
Do not rewrite user-authored files except through explicit editor Save/Save As.
Document that older executables cannot read v7; retain a backup or use Save As
before converting work needed by an older build. Reverting the implementation
requires restoring matching v6 packaged content, not stripping audio silently.

Follow `docs/DEVELOPMENT.md`: configure/build debug, run affected deterministic
and full debug tests, validate new SPIR-V, run Vulkan smoke, run device playback
and the audible/muted fixture, and inspect the diff/OpenSpec validation. Record
unavailable desktop/audio checks explicitly. Update architecture, rendering,
gameplay, development and roadmap documentation during implementation to state
the delivered behavior, not merely the intended design.
