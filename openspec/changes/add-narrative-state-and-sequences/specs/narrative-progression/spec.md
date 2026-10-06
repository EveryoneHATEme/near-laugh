## Purpose

Defines this game's bounded scene facts, accepted triggers and deterministic run-local progression without a general scripting language.

## ADDED Requirements

### Requirement: Bounded authored scene state
A level SHALL support up to 32 Boolean facts, 32 axis-aligned regions and 64 events. Facts SHALL have a durable ID and initial Boolean value; regions SHALL have a durable ID, finite center and positive finite half-extents with finite derived bounds. IDs SHALL match `[a-z][a-z0-9-]{0,63}` and be unique within each collection. Regions SHALL use half-open bounds and accepted player feet, create no collision, and require no line of sight. Definitions SHALL contain no mutable execution state, filesystem paths or arbitrary expressions.

#### Scenario: Player starts inside a region
- **WHEN** a scene initializes with the player inside a region
- **THEN** occupancy is inside but no region-entry occurrence is invented; a scene-entry trigger remains available

#### Scenario: Definition is outside the supported profile
- **WHEN** an ID, count, bound or field is invalid
- **THEN** validation rejects Save and runtime handoff with the affected record and field

### Requirement: Concrete predicates and accepted triggers
Events SHALL support scene entry, region entry, accepted player interaction, or a false-to-true transition of a nonempty predicate conjunction as their single trigger. Start guards, transition predicates, cancellation predicates and wait predicates SHALL use at most eight terms each. Predicate kinds SHALL be Boolean fact, player inside/outside region, light enabled, door open/closed endpoint or lock state, radio enabled, box held/free, current document open, actor action state, latest event terminal state, and elapsed active scene time reaching a finite threshold. Boolean/state predicates SHALL compare explicit expected values; event states SHALL be completed, canceled or failed, and actor states SHALL use the supported route lifecycle. Empty start guards SHALL be true; absent cancellation SHALL be disabled; other predicate lists SHALL be nonempty. Durations SHALL lie in [0,3600] seconds. Player interaction triggers SHALL select a valid typed target and supported accepted action. Document opening SHALL NOT imply reading comprehension. Sampled region membership transitions SHALL be recorded after each accepted simulation step, including entries hidden within a multi-step render batch. A pass entirely through a region between two samples SHALL NOT synthesize an entry. Guards SHALL use the current scheduler snapshot; a recorded entry need not still be occupied.

#### Scenario: Region is crossed within one frame
- **WHEN** accepted simulation steps enter and then leave a region before presentation
- **THEN** one entry occurrence remains available to trigger an event with satisfied current guards

#### Scenario: Requested action is refused
- **WHEN** the player requests an interaction that its concrete owner refuses
- **THEN** no accepted-interaction trigger is emitted

#### Scenario: Door movement has only been requested
- **WHEN** a door is still moving toward open
- **THEN** a door-open predicate remains false until the endpoint is actually reached and stationary

### Requirement: Deterministic observation and conflict resolution
Equal definitions, ordered accepted observations/results and active-time samples SHALL produce equal event decisions independently of collection order or rendering outcomes. Cancellation SHALL be evaluated for all active runs before new dispatch, and suppress a simultaneous restart of the same event. Events SHALL dispatch in lexicographic durable-ID order using one frozen condition snapshot; effects SHALL become trigger/condition observations only at a later boundary. Multiple state writes SHALL use that dispatch order, with the last accepted write governing. Work per boundary SHALL be bounded by the validated event/step profile, with no recursive same-boundary event execution. A refused guard, trigger while active or cancellation-suppressed trigger SHALL be consumed without backlog. Event-terminal predicates SHALL be false before the referenced event has run.

#### Scenario: Cancellation and continuation coincide
- **WHEN** a cancellation predicate and the next step's readiness are true in the same observation snapshot
- **THEN** cancellation wins and the next action never dispatches

#### Scenario: Definitions are reordered
- **WHEN** two events contend for a resource or write the same state with unchanged IDs and observations but reordered arrays
- **THEN** dispatch, contention and final state remain identical

#### Scenario: Event changes another event's condition
- **WHEN** one event sets a fact used by another event's trigger
- **THEN** the other event cannot recursively start from that mutation within the same boundary

### Requirement: Run lifetime and inspectable progress
Facts, trigger occupancy/latches and event execution SHALL be run-local and preserve immutable level definitions. Fresh launch SHALL restore authored initial state; presentation recovery SHALL retain current state. Development inspection SHALL expose facts and each event's run identity, step, status and reason, plus a bounded last-256-transition trace with identities, active time and dropped-record count. Repeated unchanged waits SHALL NOT flood that trace. Closing SHALL prevent further dispatch and terminate owned transient work. Observation overflow SHALL be reported explicitly without silently dropping accepted actions.

#### Scenario: Busy event is inspected
- **WHEN** an event cannot start its cue because another foreground cue owns the channel
- **THEN** its waiting step, target and busy reason are inspectable without forcing the competing cue to stop

#### Scenario: Process starts again
- **WHEN** a scene with completed events and changed facts is exited and freshly launched
- **THEN** authored initial values return, no run history is loaded and no level bytes have changed
