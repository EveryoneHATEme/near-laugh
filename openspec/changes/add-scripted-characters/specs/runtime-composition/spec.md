## ADDED Requirements

### Requirement: Owned run-local character coordination
Runtime composition SHALL resolve validated character definitions and selected resources before simulation, own independent mutable actor action/pose state and release all users before their referenced owners. Character motion, turning, phase transitions and action-marker decisions SHALL advance on the existing bounded one-sixtieth-second simulation steps. Each step SHALL coordinate the player, actors and doors to preserve collision and presentation safety; action outcomes SHALL not depend on rendering frequency or collection order for equivalent fixed steps and inputs. Characters SHALL use current accepted feet/yaw for collision, rendering and audio with no separate actor placement interpolation ahead of collision. Light, door and actor presentation SHALL describe one coherent accepted state.

#### Scenario: Several simulation steps precede rendering
- **WHEN** one frame contains multiple fixed steps or another contains none
- **THEN** route decisions and contact identities follow completed simulation steps, while the frame presents the resulting accepted actor poses without duplicate action dispatch

#### Scenario: Actor initialization fails
- **WHEN** selected character preparation or the third actor's physics creation fails
- **THEN** runtime reports the model/actor context, enters no frame loop and releases all previously created resources safely

### Requirement: Character suspension and fresh-run semantics
Explicit development suspension and minimization SHALL freeze player/door/actor simulation, clip transitions, pending marker retries and cue time together, consume inactive input and discard suspended wall time. Resume SHALL retain accepted poses and action/cue identities without catch-up or duplicate markers. Cursor release alone SHALL retain the current ordinary game policy of allowing world and audio time to continue. Active long frame intervals SHALL retain the existing 100 ms simulation contribution cap; already-started P04 cues SHALL retain their uncapped active-time policy, and missing simulation time SHALL NOT generate overdue walking events. Fresh process entry SHALL reconstruct authored initial actors/routes once. Presentation recovery SHALL preserve running state; cancellation or completed actions SHALL not be replayed. Inspection controls SHALL remain a development entry, with no new player-facing pause menu required.

#### Scenario: Window is minimized at an interaction marker
- **WHEN** the actor is waiting for a cue or has just started it and the window minimizes
- **THEN** actor and cue time freeze together and restoration resumes the same marker/cue state once

#### Scenario: Active frame stalls
- **WHEN** a renderable interval exceeds the bounded simulation contribution
- **THEN** accepted movement advances by only the bounded fixed steps, existing sounds use their defined active time, and no discarded-motion footsteps or artificial arrivals are emitted

#### Scenario: Game starts again
- **WHEN** a run with moved or canceled actors exits and a new process loads the same level
- **THEN** the file is unchanged and authored initial actor poses and initial routes are restored
