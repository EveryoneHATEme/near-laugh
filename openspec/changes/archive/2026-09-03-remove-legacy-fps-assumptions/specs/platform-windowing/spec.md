## MODIFIED Requirements

### Requirement: Keyboard and mouse state
The platform integration SHALL expose project-owned physical keyboard-key state, physical mouse-button state, cursor movement, and cursor-capture control. Every polling or blocking event-processing operation that dispatches input SHALL form a distinct event batch whose cursor movement remains available until the runtime samples that batch exactly once. Starting the next event batch SHALL reset look movement without clearing held physical state. The platform integration SHALL NOT assign gameplay-action meaning to those physical inputs.

#### Scenario: Input is sampled
- **WHEN** the application samples input after processing a polling event batch
- **THEN** it receives current project-owned physical key and mouse-button states plus cursor movement accumulated for that batch

#### Scenario: Blocking wait dispatches input
- **WHEN** a blocking platform wait returns after dispatching keyboard, mouse-button, or cursor events
- **THEN** the runtime can sample the physical state and cursor movement from that waited event batch before the next batch resets its look movement

#### Scenario: Next event batch begins
- **WHEN** the runtime begins processing the event batch after a previously sampled batch
- **THEN** cursor movement starts at zero while held keyboard and mouse-button state remains active

#### Scenario: First-person cursor capture
- **WHEN** the application enables first-person mouse input
- **THEN** the cursor is captured and relative movement remains available without exposing a native window handle to runtime consumers
