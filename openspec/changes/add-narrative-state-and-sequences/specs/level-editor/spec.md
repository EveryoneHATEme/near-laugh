## ADDED Requirements

### Requirement: Ordinary narrative authoring and preflight
The editor SHALL expose facts, regions, triggers, start/cancel predicates, repetition and ordered steps with typed reference controls and actionable event/step diagnostics. Safe invalid definitions SHALL remain repairable; unsupported choices SHALL not be silently substituted. Preview SHALL be schematic and silent without running narrative simulation. Save and Play SHALL apply shared definition, reference and ownership validation. Play SHALL additionally apply selected-resource preflight through the existing saved-file transaction, preserving dirty decisions, launch freshness and separate process ownership. Missing resource files alone SHALL NOT introduce a new restriction on saving otherwise valid narrative definitions.

#### Scenario: Unknown link is repaired
- **WHEN** an event refers to a deleted actor, cue, region or fact
- **THEN** its unresolved ID remains visible, Save/Play are blocked, and an explicit relink or undo repairs it

#### Scenario: Dirty narrative launch is canceled
- **WHEN** the author cancels the save decision before Play
- **THEN** definitions, history and dirty state remain and no child process starts

### Requirement: Semantic narrative controls
Every new standard narrative control SHALL expose semantic identity, supported operations, draft/applied values and validation through the existing automation interface, with independent inspection of applied document state. Reproducible scenarios SHALL cover create/select, reference edits, step reorder, invalid input, cancel, undo/redo, save/reopen and broken-link repair. Runtime start, viewport placement/picking and appearance SHALL have separate evidence rather than inferred semantic coverage.

#### Scenario: Invalid duration remains a draft
- **WHEN** a semantic scenario enters an invalid duration before committing
- **THEN** draft/error state is observable and the applied document and history remain unchanged

#### Scenario: Steps are reordered and undone
- **WHEN** a semantic scenario commits reordering then undo
- **THEN** independent document inspection observes the reordered sequence as dirty and undo to the saved baseline restores the exact sequence and clean state; redo restores the edit and dirty state

### Requirement: Independent neutral sequence authoring
T4 acceptance SHALL retain a second scene variation created through ordinary editor controls, saved, reopened and played from its saved definitions. It SHALL exercise a different region/link arrangement, steps and cancellation, plus undo/redo and a deliberately broken reference repaired before launch. No runtime code change, filename trigger or script rewriting that authored scene SHALL be needed. Functional semantic evidence SHALL remain separate from visual placement and human listening acceptance.

#### Scenario: Second variation survives reopen
- **WHEN** the UI-authored variation is saved, reopened and launched
- **THEN** its IDs, order, conditions and links survive and ordinary runtime behavior follows those saved values
