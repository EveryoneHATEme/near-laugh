## Purpose

Defines finite editor-authored sequences that coordinate existing world actions, waits and owned cancellation for this game's neutral T4 scenes.

## ADDED Requirements

### Requirement: Finite sequence vocabulary
Each event SHALL contain one trigger, start guards, optional cancellation predicates, a once or rearm mode, and 1-32 ordered steps. Supported steps SHALL set a Boolean fact, light enable, door requested endpoint, door lock or radio enable; play one eligible one-shot cue to completion; run one actor-owned route to completion; delay; or wait for a nonempty supported predicate conjunction. Desired-state steps SHALL finish on accepted requests, including already-satisfied requests; they SHALL NOT imply physical completion. Steps SHALL have only their kind's required typed parameters. Unsupported kinds, nested scripts, jumps, self event references and rearming scene-entry events SHALL be rejected. Door/route/audio rules SHALL remain those of the concrete owners.

#### Scenario: Sequence waits for an actual door endpoint
- **WHEN** an open-door request is followed by a wait for door-open
- **THEN** later steps wait until the door actually reaches its stationary open endpoint

#### Scenario: Invalid sequence is authored
- **WHEN** a step has an unknown kind, wrong reference type, missing parameter, self event reference or a duration outside the supported range
- **THEN** validation identifies the event and step or field and blocks Save and Play

### Requirement: One active run and explicit repetition
An event SHALL have at most one active run. Once mode SHALL consume its opportunity when a run starts, including a subsequently canceled or failed run. Rearm mode SHALL require a fresh trigger after the preceding run is terminal; a transition trigger SHALL observe false after termination before another true. Region and interaction triggers SHALL require later occurrences. Startup SHALL initialize transition latches from authored initial state without inventing a rising edge. Triggers received while active SHALL NOT queue a later run. Run identity SHALL distinguish repeated runs.

#### Scenario: Once event is revisited
- **WHEN** the player re-enters after a once event completes or is canceled
- **THEN** it does not restart

#### Scenario: Repeat condition remains true
- **WHEN** a rearming event finishes while its trigger predicate remains true
- **THEN** it waits for a subsequent false observation and new true before starting again

### Requirement: Observable waits and failure
A busy resource SHALL leave the step waiting without acquiring foreign work or hidden retries beyond one attempt per boundary. Already-active work not started by this run SHALL be treated as foreign. After cancellation preflight, existing owned cue/route waits SHALL be classified against the immutable boundary snapshot before any new starts. A matching completed instance SHALL remain successful despite later same-boundary reuse; replacement or external cancellation already present before observed completion SHALL fail the event with context rather than adopt a new instance. Refused commands SHALL fail the event and prevent later steps. Blocked routes and unmet conditions SHALL remain waiting without implicit timeout. Door obstruction SHALL retain the stopped request until a new explicit request; a wait SHALL NOT resume it.

#### Scenario: Another route is already active
- **WHEN** an event reaches a route step and that actor already has an active route, including the requested route
- **THEN** the event waits without acquiring or canceling that route

#### Scenario: Owned action is replaced
- **WHEN** a waiting event observes a different actor or cue instance
- **THEN** its run fails with an ownership-loss reason and leaves the replacement intact

#### Scenario: Door path clears after obstruction
- **WHEN** an event waits for a door that stopped on an obstacle and the obstacle leaves
- **THEN** the door remains stopped, the wait remains explainable, and cancellation or a new explicit request is required

#### Scenario: Completed action is reused within a boundary
- **WHEN** the snapshot shows one event's owned cue or route completed and a contender starts a new instance on that source or actor in the same boundary
- **THEN** the old wait succeeds and the new instance remains separately owned in either event-ID order, while stale cancellation cannot stop it

### Requirement: Cancellation affects only owned transient actions
Canceling or failing a run SHALL discard all its pending steps and conditionally stop only matching cue/route instances it successfully started. It SHALL NOT roll back facts, light/lock/radio changes or accepted door motion targets, transfer player controls, stop unrelated audio or cancel another run's action. A cancellation predicate already true SHALL prevent a new run from starting. Region exit alone SHALL not cancel unless an authored condition requests it.

#### Scenario: Run is canceled during playback
- **WHEN** its cancellation condition becomes true while its owned cue is active
- **THEN** that cue and caption stop, no later route/delay action dispatches, and already changed light state remains

#### Scenario: Old run is canceled after replacement
- **WHEN** the actor or source now belongs to another instance
- **THEN** cancellation leaves that instance and its sound untouched

### Requirement: Active time and actual completion
Delays and elapsed predicates SHALL use monotonic active scene time excluding suspension/minimization. A delay SHALL start when its step is reached, without backdating later actions to missed wall time. Cue completion SHALL use its logical timeline, not device callbacks; physical completion SHALL use actual owner results. Zero-step active batches SHALL permit progression without inventing physical movement. The established capped physics and uncapped active audio policies SHALL remain; equivalent wall time alone SHALL NOT imply equivalent simulation history.

#### Scenario: Delay spans a long active frame
- **WHEN** active time passes the delay deadline during a long frame
- **THEN** the next action starts at the current boundary without replaying historical cues or fabricating route progress

#### Scenario: Output is muted or unavailable
- **WHEN** the same sequence runs with mute or no audio device
- **THEN** cue logical completion, captions and event ordering follow the same active-time observations

### Requirement: Neutral T4 acceptance
Acceptance SHALL include an ordinary saved-file scene in which region entry enables a light, plays a captioned one-shot, waits, runs a character route and records completion. An initially-on control radio becoming off SHALL cancel the sequence before or during its owned work. Re-entry, alternate action order, busy audio, actor and door obstruction, reading, suspension, minimization, presentation recovery and restart SHALL have recorded outcomes. A second variation SHALL be authored through the editor without runtime code changes. Automated, GPU, semantic UI, visual/listening and Release performance evidence and unavailable checks SHALL remain distinguishable; tests alone SHALL NOT establish manual acceptance.

#### Scenario: Cancellation occurs during the delay
- **WHEN** the player turns off the control radio before the delayed route step
- **THEN** the run is canceled and the character route never starts

#### Scenario: Independent variation is played
- **WHEN** an author saves and plays another supported arrangement and sequence from the editor
- **THEN** its behavior comes from the saved definitions without filename-dependent execution
