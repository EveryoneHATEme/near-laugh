## ADDED Requirements

### Requirement: Owned physical household coordination
Runtime composition SHALL initialize validated household definitions and selected text/audio resources before their consumers, own concrete box holding/radio/reading state independently of the immutable level, and coordinate it on the existing main thread. The physics owner SHALL be the authority for actual box poses, velocities and sleeping state. Accepted pickup/drop/throw requests SHALL be consumed once and applied at the next fixed boundary before world advancement; they SHALL NOT repeat across render batches or survive suspension as delayed actions. Hold targets SHALL derive from the accepted simulation eye/look at each fixed boundary rather than the render-interpolated camera. A queued throw SHALL retain its accepted direction instead of resampling aim on execution. The player, actors and doors SHALL share a coherent view of current box collision. Targeting, visible geometry and shadows SHALL use the same accepted box pose, including full rotation, without separate visual motion ahead of collision.

#### Scenario: An action arrives in a zero-step batch
- **WHEN** a pickup or throw press is accepted in a renderable batch with no fixed step
- **THEN** it is retained at most once for the next fixed boundary, its eligibility is rechecked there, and presentation shows actual current state until the action is applied

#### Scenario: Suspension interrupts a pending request
- **WHEN** a pending household command has not reached its fixed boundary before controls are suspended, minimized or closed
- **THEN** the ordinary input command is discarded without a later throw, acquisition or duplicate state transition; an already-required safety release remains owed before the next advancing step

#### Scenario: Rendering is skipped after a physics action
- **WHEN** physics has applied a throw but presentation is skipped or recovered
- **THEN** subsequent frames consume that same body's current state without applying another impulse

### Requirement: Household frame and teardown boundary
Frame presentation SHALL contain only bounded resolved geometric/text data, with no household controllers, durable gameplay IDs, body handles, throw requests or document-navigation actions. The renderer SHALL consume borrowed data synchronously without retaining caller storage. Household radio changes SHALL reach audio before frame captions are resolved. Hold constraints and pending commands SHALL be released before referenced physics bodies; audio users and reading views SHALL end before their content owners. Partial startup and failed physical hold acquisition SHALL release acquired resources exactly once, with no half-installed ownership or main-loop use of invalid bodies.

#### Scenario: Radio stops in a displayed batch
- **WHEN** an accepted radio-off action is presented
- **THEN** that frame's resolved feedback and captions agree with the canceled source

#### Scenario: Holding resource creation fails
- **WHEN** hold setup fails after some dependent resources are acquired
- **THEN** the free box remains at its physical pose with no ownership transfer, partial resources are released and a later eligible retry remains possible


### Requirement: Mandatory safety release across suspension
A release already required by loss of active carrying controls SHALL remain distinct from pending input commands. It SHALL complete at the actual body pose before the next advancing physics step, even if a zero-step batch, minimization, suspension or recapture intervenes. Suspension without an earlier release obligation SHALL preserve the held body and hold relationship. Completing a safety release SHALL never add a throw impulse or reacquire a box automatically.

#### Scenario: Cursor loss precedes minimization without a step
- **WHEN** cursor release requires a drop, no fixed step runs, the window minimizes and capture is restored before the next advancing step
- **THEN** the previously owed safe release still completes once before physics advances, with no throw or automatic reacquisition
