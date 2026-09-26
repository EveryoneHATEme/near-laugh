# game-text-presentation Specification

## Purpose

Makes Russian dialogue and essential environmental sound clues readable and synchronized so the story remains understandable with sound muted or unavailable.

## Requirements

### Requirement: Validated Russian caption content
Dialogue and essential sound cues SHALL reference packaged Russian captions whose source labels disclose only information available to the player. Caption segments SHALL be ordered, nonoverlapping and within clip duration, with finite start/end offsets, at least one second duration, and at most 160 Unicode scalar values including the label. Gaps SHALL correspond to silence rather than missing meaningful dialogue. Invalid UTF-8, unsupported glyphs, mismatched timing and missing selected caption/font resources SHALL be actionable preflight failures. Ordinary Latin and Russian Cyrillic including Ё/ё, digits and authored punctuation SHALL render correctly.

#### Scenario: Selected caption is invalid
- **WHEN** selected text contains invalid UTF-8, an unavailable glyph, overlapping segments or an end beyond the selected clip
- **THEN** preflight identifies the caption and faulty field and refuses playback rather than drawing replacement boxes or silently dropping text

#### Scenario: Unrevealed speaker is heard
- **WHEN** the invitation plays from beyond the door before its speaker is identified
- **THEN** the caption communicates the spoken words and a neutral audible-source label without naming the hidden character or threat

### Requirement: Legible bounded caption layout
Foreground dialogue and essential captions SHALL appear in a reserved lower-screen lane, with high-contrast text over a dark backing, safe margins and word wrapping. All essential content SHALL fit without truncation at framebuffer sizes from 800x600 through 3840x2160, with at most four foreground lines and two separate ambience lines. HiDPI framebuffer dimensions SHALL govern scale. Invalid content that cannot fit at the minimum supported size SHALL fail content preflight. Smaller windows SHALL remain safe even when full readability cannot be retained.

#### Scenario: Window size or display scale changes
- **WHEN** a long valid Russian subtitle is displayed at 800x600, 1920x1080 or 3840x2160, including a HiDPI window
- **THEN** the complete text remains legible inside safe margins without overlap with the ambience lane or clipping essential words

#### Scenario: Ambience captions compete
- **WHEN** multiple ambience captions are eligible during dialogue
- **THEN** foreground text is preserved and at most one ambience caption is selected by greatest effective gain with durable source ID as the tie breaker

### Requirement: Shared active cue time
Audio and captions SHALL follow the same run-local active cue timeline independent of device completion and render frequency. Muting, zero distance gain or device failure SHALL NOT hide essential captions or change cue ordering. Explicit suspension and minimization SHALL freeze voice position and text time together; resuming SHALL exclude the suspended interval and continue from the preserved offset. Cursor release alone SHALL NOT pause. Explicit cancellation SHALL remove the associated caption without a delayed replay. Loop captions SHALL show their initial authored interval once per explicit start, not every loop.

#### Scenario: Minimized speech resumes
- **WHEN** the fixture is minimized during a captioned phone line and restored later
- **THEN** the voice and text continue from the suspended cue offset without skipping the remaining words or restarting the line

#### Scenario: Playback is muted
- **WHEN** the entire fixture runs with master gain zero or without an output device
- **THEN** every essential caption and the phone-before-invitation order remain the same as in an audible run

#### Scenario: Playback drift is detected
- **WHEN** reported audio position differs from active cue time by more than 100 ms
- **THEN** playback is realigned without changing logical cue order, or a synchronization failure is reported and silent caption playback continues if synchronization cannot be maintained

### Requirement: Independent telephone clue fixture
The project SHALL provide an explicitly launched bounded fixture with spatially distinct radio, telephone and moving off-screen footstep sources, followed by a completed spoken phone conversation and then a contradictory invitation. Its representative Russian recordings and captions SHALL make that ordering and contradiction understandable, audibly and muted. The fixture SHALL support restart, mute and suspension for acceptance without introducing narrative progression into ordinary game runs. Door transmission SHALL be exercisable with open, closed and obstructed door poses.

#### Scenario: Fixture plays through
- **WHEN** the fixture runs to completion with valid representative content
- **THEN** the phone conversation completes before the invitation begins, radio and footsteps remain distinct, and the captions communicate the contradiction without revealing an unknown speaker

#### Scenario: Fixture is not selected
- **WHEN** the ordinary game launches an authored level
- **THEN** it does not implicitly run the temporary telephone sequence based on the level filename

### Requirement: Bounded readable document presentation
The text subsystem SHALL validate and present household document titles and ordered pages using the existing supported Russian/Latin glyph repertoire and trusted font. Reading SHALL show one complete authored page, title, page position and concise previous/next/close controls on a high-contrast panel. Valid text SHALL fit without truncation at framebuffers from 800x600 through 3840x2160 using framebuffer-aware scale and safe margins. Long words and unsupported glyphs SHALL fail actionable document/page preflight if they cannot be laid out. Windows below the supported minimum SHALL remain memory-safe. Navigation SHALL clamp at the first/last page and SHALL NOT skip or wrap implicitly. Resize/recovery SHALL retain the open document and page.

#### Scenario: Russian multipage document is read
- **WHEN** the reader opens a valid document and visits each page
- **THEN** its complete title/text and page controls remain legible without missing Ё/ё or silently clipped words

#### Scenario: Display is resized while reading
- **WHEN** an open document moves between supported framebuffer sizes or HiDPI scales
- **THEN** the same page remains open and all essential content fits the new layout

### Requirement: Household feedback alongside captions
Readable content, target/holding hints and action success/refusal feedback SHALL use presentation areas separate from the reserved foreground and ambience caption lanes. Household feedback SHALL NOT cancel, replace or obscure active captions. Selected radio state and carrying/drop/throw controls SHALL remain understandable with audio muted or absent. Unsupported or refused actions SHALL provide a bounded readable reason; feedback expiry SHALL use active runtime time and preserve remaining duration through explicit suspension. Empty household text SHALL require no corresponding draw.

#### Scenario: Radio caption occurs during document reading
- **WHEN** a valid radio caption is eligible while a document panel is visible
- **THEN** the page, controls and existing caption lanes remain independently legible

#### Scenario: Pickup is refused
- **WHEN** an eligible pickup cannot be accepted safely
- **THEN** readable feedback identifies refusal without altering another target or interrupting an unrelated caption
