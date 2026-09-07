## MODIFIED Requirements

### Requirement: Gameplay-independent point-light enable request
For every scene frame, the runtime SHALL supply backend-neutral enabled state for every light in the loaded bounded authored set, initialized from each light's own initial value. The request SHALL NOT expose switch definitions, input actions, physics hits, gameplay controllers, or backend types. The number and order of submitted enables SHALL match the immutable light set exactly; missing or mismatched data SHALL be rejected before GPU submission, and the empty set SHALL be valid. The runtime SHALL retain light state across renderer outcomes; the renderer SHALL only present the supplied state.

#### Scenario: Switch changes a light
- **WHEN** runtime interaction changes the linked point-light state
- **THEN** the next frame request contains the resulting point-light enable values without identifying the gameplay source

#### Scenario: Frame is skipped or recovered
- **WHEN** a request after a toggle results in skipped work or presentation recovery
- **THEN** the runtime retains its current light state and includes it in the next requested frame without replaying the press

#### Scenario: Default frame has no overrides
- **WHEN** a caller supplies no point-light enable data for a nonempty authored light set
- **THEN** the boundary rejects the incomplete request before submission; runtime startup must explicitly initialize enables from the authored light values

#### Scenario: Enable data does not match the scene
- **WHEN** a frame supplies fewer or more enable values than the immutable light set
- **THEN** the boundary reports the mismatch before submission rather than enabling a different light or reading outside the data
