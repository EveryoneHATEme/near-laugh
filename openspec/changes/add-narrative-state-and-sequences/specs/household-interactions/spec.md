## ADDED Requirements

### Requirement: Authored radio state and household observation
Authored events SHALL set radio on/off through its concrete owner independently of player hand/reading controls, preserving its exclusive source, fixed position and caption rules. Setting the current value SHALL not restart or duplicate audio. Other household ownership and physical state SHALL remain unchanged. Events SHALL observe accepted player box actions, held/free state and document opening/current state without moving boxes, acquiring the player's hands, inferring comprehension or forcing reading to open/close.

#### Scenario: Radio command executes during reading
- **WHEN** an event sets an off radio on while the player reads
- **THEN** one radio instance starts with its caption while the same document/page remains open

#### Scenario: Repeated on request reaches an active radio
- **WHEN** an event requests on for a radio already on
- **THEN** its existing source instance and offset remain unchanged

#### Scenario: Physical item is observed
- **WHEN** a scene predicate observes a held box
- **THEN** it reads actual accepted ownership and cannot teleport, throw or steal that box
