## ADDED Requirements

### Requirement: Gameplay-independent animated pose request
Animated presentation SHALL contain bounded backend-neutral model-instance selection, finite world placement and validated skeletal pose data for zero through four selected character instances. It SHALL contain no actor actions, route IDs, native physics objects or animation-loader/backend types. Its selected resource and pose sizes SHALL be validated together before submission, with no unknown, duplicate or mismatched instance data. Borrowed frame data SHALL be consumed synchronously without retaining caller storage. The caller SHALL own time and transitions; rendering outcomes SHALL NOT advance or restart animation.

#### Scenario: Caller supplies four poses
- **WHEN** four selected instances supply valid independent skeletal poses
- **THEN** rendering consumes each supplied placement/pose without interpreting route policy or sharing mutable playback state

#### Scenario: Supplied pose is incompatible
- **WHEN** a selected instance lacks its pose, has an invalid transform or references the wrong skeleton size
- **THEN** presentation fails before GPU submission with the affected instance identified

#### Scenario: Presentation is skipped
- **WHEN** a frame is skipped or recovered
- **THEN** the next request uses the caller's current pose without renderer-generated animation advancement or replay
