## ADDED Requirements

### Requirement: Household document editing and preflight
The standalone editor SHALL expose household definitions and their diagnostics through its ordinary document workspace. Readable title/pages SHALL be editable as Russian text, with ordered page addition/removal and a preview of the same bounded layout used in play. Radio controls SHALL expose their prop/source references and initial on/off value. Box authoring SHALL show its fixed physical profile and initial pose, without running live physics in the editor. Invalid safe references SHALL remain selectable and repairable; failed text/asset preparation or preview replacement SHALL retain the document and last coherent preview with an actionable stale/error indication. Save and Play SHALL validate household definitions and all selected text/font/model/audio resources before child creation, using the existing saved-document transaction and freshness checks.

#### Scenario: Document page is edited and restored
- **WHEN** the author commits a page edit and then undoes and redoes it
- **THEN** content, layout preview, validation and dirty state follow the corresponding document revisions without losing other pages

#### Scenario: Radio link is invalid
- **WHEN** a radio's source or prop reference cannot be resolved or violates ownership
- **THEN** the radio remains selectable with the precise link diagnostic and Save/Play remain unavailable until repair

#### Scenario: Play requires invalid selected text
- **WHEN** a selected document contains a missing glyph or a page that cannot fit the supported minimum framebuffer
- **THEN** preflight names its document/page, launches no process and keeps the editable contents

### Requirement: Complete neutral household authoring acceptance
P06 acceptance SHALL include creation of a second neutral scene through the editor UI, containing at least two boxes, a readable multipage document and an operable radio. The author SHALL exercise object creation, placement, reference editing, undo/redo, save/reopen and ordinary saved-file Play. The game SHALL allow reading, pickup/drop/throw, physical contact and radio operation from those saved definitions without code changes, a hidden filename trigger or a preparation script rewriting the authored scene. Editing or saving the level while Play runs SHALL NOT change that running scene. Automated UI success SHALL remain distinct from manual visual/interaction/audio acceptance.

#### Scenario: Independent scene is authored and played
- **WHEN** an author completes the second scene using the editor and launches its saved file
- **THEN** its household objects and initial states agree in editor and game and all three supported interaction types are usable

#### Scenario: Existing scene is edited during Play
- **WHEN** household definitions change in the editor while its earlier saved scene is running
- **THEN** the game retains its own definitions and physical state and the next explicit fresh launch uses the later saved definitions
