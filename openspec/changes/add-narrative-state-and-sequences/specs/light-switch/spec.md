## ADDED Requirements

### Requirement: Explicit authored light enable
Authored events SHALL be able to set one referenced light's run-local enabled state explicitly. Setting its current value SHALL be an accepted no-op. The next ordinary switch interaction SHALL toggle that same shared value. Other lights, authored parameters, shadows configuration, ambient and flashlight SHALL remain unchanged; event cancellation SHALL not restore an earlier value.

#### Scenario: Enabled light receives repeated enable
- **WHEN** two event steps request enable=true for the same already-enabled light
- **THEN** it remains enabled without a toggle or change to other lighting

#### Scenario: Player acts after an event
- **WHEN** an event disables a light and the player activates a linked switch
- **THEN** the same light becomes enabled from its current shared state
