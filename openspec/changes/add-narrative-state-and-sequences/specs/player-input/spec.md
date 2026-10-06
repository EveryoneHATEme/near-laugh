## ADDED Requirements

### Requirement: Narrative progression preserves concrete input ownership
Authored sequences SHALL not synthesize player presses, seize the player's held box, force a reader page transition or bypass existing release gates. Reading SHALL retain its suppression of movement/look/world actions while narrative and world time continue. Cursor release SHALL retain ordinary world progression and owed box release. Explicit suspension/minimization SHALL freeze progression, consume inactive inputs and retain accepted world outcomes; resume SHALL not convert queued or held inputs into new narrative interaction triggers.

#### Scenario: Event runs while reading
- **WHEN** a timed event changes a light or radio while the player reads
- **THEN** the document/page stays open, narrative and captions progress, and held world controls cannot act on closing

#### Scenario: Pending pickup is suspended
- **WHEN** an unexecuted pickup command is discarded by suspension
- **THEN** no accepted pickup occurrence or later narrative trigger is produced
