## ADDED Requirements

### Requirement: Instance-conditional authored route cancellation
An authored route requester SHALL acquire ownership only when it starts a new route instance. Already-active or busy results SHALL not grant ownership. Completion and conditional cancellation SHALL match both actor and expected instance; a mismatched cancel SHALL leave the current route, pose and owned audio unchanged. External cancellation or replacement SHALL be distinguishable from completion of the requested instance. Existing blocked traversal, final interaction and character audio arbitration SHALL remain in force.

#### Scenario: Another requester uses the same route
- **WHEN** a sequence requests an already-active identical route
- **THEN** it waits without taking ownership or gaining cancellation rights

#### Scenario: Stale cancellation arrives
- **WHEN** a cancellation expects an earlier instance after a new route has started
- **THEN** the new route and its sounds continue untouched

#### Scenario: Owned blocked route is canceled
- **WHEN** its event cancels the matching blocked route instance
- **THEN** pending route movement/interaction sounds end and the accepted actor pose remains
