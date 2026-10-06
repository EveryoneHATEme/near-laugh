## ADDED Requirements

### Requirement: Accepted interaction observations
Concrete runtime consumers SHALL receive once-only accepted player outcomes with occurrence identity, typed target identity, action kind and result. Supported outcomes SHALL include door interaction/lock/knock, switch activation, radio on/off, document opened and physically applied box pickup/drop/throw. Queue acceptance SHALL NOT count as physical action completion. Refusals, target misses, held input, safety releases and author-driven desired-state commands SHALL NOT masquerade as accepted player interactions. Observations SHALL not alter targeting, action priority or repeat behavior.

#### Scenario: Pickup succeeds at its physical boundary
- **WHEN** a queued pickup is successfully applied
- **THEN** one outcome names that box and accepted pickup, regardless of later presentation count

#### Scenario: Knock changes no door state
- **WHEN** the concrete door owner accepts a player knock
- **THEN** one knock occurrence is observable even though pose and lock remain unchanged

#### Scenario: Author-driven light command executes
- **WHEN** an event sets a light enable
- **THEN** no fictitious player switch activation is emitted; state predicates may observe the light later
