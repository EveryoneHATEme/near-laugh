## ADDED Requirements

### Requirement: Sequence source eligibility and instance ownership
Direct sequence playback SHALL reference non-autoplay sources with one-shot cues, excluding actor-owned, radio-owned or privately fixture-owned sources. Shared validation/preflight SHALL reject those conflicts and waits on looping direct cues. Distinct sequences SHALL be allowed to contend for one eligible source through ordinary arbitration. Only a successful new start SHALL grant an owned instance; already-active and busy results SHALL not. Conditional cancellation and completion checks SHALL identify the expected instance, and stale cancellation SHALL not stop a newer cue or caption. Foreground arbitration, mute, silent-device and logical active-time behavior SHALL remain unchanged.

#### Scenario: Source is reserved by a radio
- **WHEN** a direct sequence step references that source
- **THEN** validation identifies the conflicting ownership and blocks launch

#### Scenario: New cue replaces an old owned instance
- **WHEN** an older event later attempts cancellation using its retained instance
- **THEN** the current cue and caption continue and the old instance is reported no longer owned

#### Scenario: Two events contend for foreground audio
- **WHEN** their cue steps become ready together
- **THEN** stable event ordering admits one, the other waits, and neither preempts unrelated foreground content
