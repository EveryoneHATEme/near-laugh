## ADDED Requirements

### Requirement: Owned narrative coordination
Runtime composition SHALL initialize validated event definitions and concrete owners before progression, collect accepted simulation/interactions and logical action results, and perform cancellation preflight after ordinary interaction handling but before pending character audio handoff. Surviving character audio and narrative continuations SHALL then dispatch before presentation. Conditions SHALL use an immutable boundary snapshot; changes from dispatch SHALL enter later snapshots. Intermediate fixed-step region entries SHALL survive frame batching. No-window deterministic tests SHALL be able to supply the same observations and results without GPU, physics backend or audio-device ownership. Empty narrative definitions SHALL preserve prior scene behavior. Shutdown SHALL close dispatch and release event users before referenced controllers/audio/level data.

#### Scenario: Batch has no simulation step
- **WHEN** a player interaction is accepted in an active zero-step batch
- **THEN** it is observed once by progression, while any requested physical movement waits for later simulation

#### Scenario: Rendering recovers after a one-shot
- **WHEN** presentation skips or recovers after the event started or completed its cue
- **THEN** event/cue identity is retained and no start is replayed

#### Scenario: Initialization fails
- **WHEN** a selected narrative reference or resource fails startup validation
- **THEN** no event dispatch occurs and already-created owners are released in dependency order

### Requirement: Common development suspension for narrative scenes
One explicit development suspension control SHALL freeze player, boxes, doors, actors, narrative delays and cue time together, preserving already accepted occurrences, current pages and active action identities. Minimize SHALL use the same policy. Resume SHALL discard suspended wall time and not replay inactive inputs. Reading and cursor release alone SHALL keep world/event/audio time active. No player-facing session menu SHALL be required for this development control.

#### Scenario: Paused scene resumes
- **WHEN** a route, cue and pending narrative delay are suspended and restored
- **THEN** each resumes its retained state without catch-up, repeated accepted occurrences or stale world actions

#### Scenario: Scene closes while suspended
- **WHEN** close is requested during suspension
- **THEN** pending event dispatch is disabled and owned actions are canceled without resuming the scene

#### Scenario: Cancellation coincides with a character sound marker
- **WHEN** the current accepted observations cancel a sequence whose owned route has a pending interaction sound
- **THEN** the matching route is canceled before audio handoff and that pending sound never starts
