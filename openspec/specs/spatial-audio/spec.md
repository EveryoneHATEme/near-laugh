# spatial-audio Specification

## Purpose

Provides owned playback and bounded authored spatial cues so the apartment's sounds communicate location, atmosphere and narrative clues consistently with doors and captions.

## Requirements

### Requirement: Bounded authored audio definitions
The level audio object SHALL contain only required cues, sources, rooms and connections arrays, bounded respectively to 128, 64, 32 and 64 records, with empty collections valid. IDs SHALL follow the existing durable-ID syntax and be unique within their collection. Cues SHALL contain only ID, catalog clip, nullable catalog caption, dialogue/essential/ambience kind, loop and spatial flags. Sources SHALL contain only ID, cue reference, finite position, gain in [0,1], finite near/far distances satisfying 0 < near < far <= 100 metres, and autoplay flag. Only looping ambience SHALL autoplay; dialogue and essential cues SHALL be captioned one-shots. Rooms SHALL contain only ID, finite center and positive finite half extents with nonoverlapping interiors. Connections SHALL contain only ID, distinct nullable room endpoints, nullable door reference, and finite closed/open transmission gains satisfying 0 <= closed <= open <= 1. Null room SHALL mean outside all rooms; a door SHALL have at most one connection. Unknown catalog identities and unresolved level references SHALL prevent saving and runtime handoff with field and record context. These records SHALL NOT supply collision, entry support or filesystem paths.

#### Scenario: Authored audio is valid
- **WHEN** a document contains bounded valid sources, cue references and room/door connections
- **THEN** validation accepts it without constructing a device, parsing render models or changing collision

#### Scenario: Audio reference or field is invalid
- **WHEN** a cue ID is duplicated, a source refers to an absent cue, rooms overlap internally, a connection names a missing room/door, a door has two connections, or a field exceeds its bound
- **THEN** validation identifies the record and reason and refuses saving and runtime handoff while safely decoded editor content remains repairable

#### Scenario: Legacy silent level is used
- **WHEN** a supported older level normalizes to empty audio collections
- **THEN** it remains valid and requires no unreferenced audio clips

### Requirement: Selected packaged audio resources
Selected clips SHALL use signed 16-bit 48 kHz PCM WAVE, mono for spatial playback and mono or stereo for nonspatial ambience, with durations greater than zero and at most 120 seconds. Selected decoded float audio SHALL fit a checked aggregate 128 MiB budget. Playback SHALL resolve catalog identities below the executable resource root, validate selected clips and caption compatibility before starting them, and reuse prepared data during playback. Missing, corrupt, unsupported or oversized selected assets SHALL report their identity and resolved path and prevent runtime startup or editor audition/play preflight; unreferenced clips SHALL NOT be required. Dialogue and essential clips SHALL have meaningful matching Russian captions.

#### Scenario: Selected resource is unsupported
- **WHEN** a spatial cue selects a stereo clip, an invalid WAVE file, a clip exceeding the duration bound, or content exceeding the decoded budget
- **THEN** preflight rejects it with asset context before playback begins

#### Scenario: Resources are packaged independently of the working directory
- **WHEN** the game or editor runs from another working directory with valid selected audio assets beside the executable
- **THEN** the selected clips load without requiring source recordings or unselected catalog files

### Requirement: Listener and source spatial presentation
Spatial cues SHALL use one listener matching the presented eye position and orientation and their current run-local source position. Rotation SHALL change left/right presentation coherently, translation SHALL change distance attenuation, and moving a source SHALL NOT restart its playback. Gain SHALL be full through the authored near distance, decrease linearly to zero at the far distance, and remain zero beyond it. Master mute SHALL silence output without stopping cues or captions. Nonspatial ambience SHALL ignore listener pose and room transmission.

#### Scenario: Listener turns toward a source
- **WHEN** a source is initially to the listener's right and the listener turns to face it
- **THEN** output moves coherently from right-biased toward centered without restarting the clip

#### Scenario: Source moves beyond its range
- **WHEN** a playing spatial source moves continuously from near the listener beyond its far distance
- **THEN** its gain reaches zero while cue time continues without a new playback instance

### Requirement: Authored room and door transmission
Room membership SHALL follow current positions with deterministic boundary classification and outside membership when no room contains the position. Same-region transmission SHALL be unity. Between connected regions transmission SHALL use the strongest product of authored connection gains; disconnected regions SHALL use 0.1 transmission. Doorless connections SHALL use open gain. Door-linked gain SHALL interpolate from closed to open using actual accepted angular openness, including blocked intermediate poses; lock state alone SHALL NOT change gain. Gain changes SHALL be smoothed without altering cue time.

#### Scenario: Door is blocked mid-swing
- **WHEN** collision stops a connecting door between its endpoints
- **THEN** sound transmission reflects the accepted partial openness rather than the requested open state

#### Scenario: Alternate room paths exist
- **WHEN** two paths connect the source and listener regions
- **THEN** the path with the greater product determines transmission regardless of array order, with cycles causing no amplification or unbounded traversal

#### Scenario: Boundary or disconnected region is reached
- **WHEN** a listener crosses a shared room face or enters a region without a connecting path
- **THEN** membership is deterministic and the specified connection or disconnected gain applies without requiring geometric acoustic queries

### Requirement: Bounded cue lifetime and foreground arbitration
Playback SHALL permit at most one active instance per source and one dialogue/essential foreground cue globally. Repeated start of an active source SHALL be idempotent. Competing foreground starts SHALL return busy without stealing, queuing or interrupting the existing clue. Explicit cancel SHALL stop its voice and captions together; a later explicit start SHALL create a new instance from the beginning. Completed one-shots SHALL stay completed until explicitly started again. Ambience SHALL be reduced to one quarter of its otherwise effective gain while foreground content is active and restored afterward. Source definitions SHALL remain immutable.

#### Scenario: Foreground requests compete
- **WHEN** a second foreground cue is requested during a phone line
- **THEN** the request reports busy and the current line and captions continue unchanged

#### Scenario: Cue is canceled and presentation recovers
- **WHEN** a foreground cue is explicitly canceled and the swapchain subsequently recovers
- **THEN** neither its voice nor caption restarts, and only a new explicit start can replay it

#### Scenario: Ambience overlaps a clue
- **WHEN** a foreground clue starts while the radio loop is active
- **THEN** radio playback continues at reduced gain and recovers its prior effective gain after the clue ends

### Requirement: Device failure and owned teardown
Audio device initialization failure or later loss SHALL report an actionable warning and continue in silent mode with cue timing and essential captions intact. Automatic recovery SHALL NOT restart completed or interrupted cues. Device callbacks SHALL NOT determine gameplay outcomes. Shutdown and failed construction SHALL stop playback activity before releasing referenced clip data and SHALL release every acquired resource exactly once. Editor audio failure SHALL NOT close or discard the document.

#### Scenario: No playback device is available
- **WHEN** selected assets are valid but the output device cannot initialize
- **THEN** the game remains playable with captions and a warning and the cue fixture follows the same order as audible playback

#### Scenario: Device fails during speech
- **WHEN** the device is lost during the phone conversation
- **THEN** captions and active cue timing continue without replaying the line and shutdown releases acquired audio resources safely

#### Scenario: Construction fails after acquiring resources
- **WHEN** audio initialization fails after preparing some clips or voices
- **THEN** acquired resources are released once without callback access to destroyed data
