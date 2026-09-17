## ADDED Requirements

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
