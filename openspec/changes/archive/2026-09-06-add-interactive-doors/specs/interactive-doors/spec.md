## Purpose

Defines this game's authored hinged doors, concrete player actions, locks, and coherent moving collision and presentation without a general rigid-body interaction system.

## ADDED Requirements

### Requirement: Bounded authored hinged doors
A level SHALL support zero through 32 independent hinged doors. Each SHALL have a unique case-sensitive durable identifier matching `[a-z][a-z0-9-]{0,63}`, finite hinge position and closed yaw, bounded positive leaf dimensions, a bounded nonzero signed opening angle, positive bounded angular speed, lock side `none`, `positive-z`, or `negative-z` in its closed local frame, and boolean initially-open and initially-locked values. The closed leaf SHALL extend from the hinge along local positive X, upward along Y, with its thickness centered on local Z. Initial open state SHALL select either the closed endpoint or the authored open endpoint. A locked initial door SHALL be closed and have a lock side. Identity SHALL survive save/reopen and unrelated edits independently of array order or editor handles.

#### Scenario: Authored door starts
- **WHEN** a valid door definition starts a new run
- **THEN** visible leaf, collision, target bounds, and lock appearance use its authored initial pose and lock state before the first player step or frame

#### Scenario: Definition is invalid
- **WHEN** a door has a duplicate or malformed ID, unsafe bounds, zero opening angle, invalid speed or lock side, or initially locked state while open or without a lock
- **THEN** validation reports its identifier or array location and invalid field and prevents saving and runtime handoff

### Requirement: Concrete door operation
One accepted interaction action SHALL request opening from the closed endpoint and closing from the open endpoint. Pressing interaction while the door is moving or stopped between endpoints SHALL reverse its last requested direction. A locked door SHALL refuse opening without changing its leaf pose or locked state. Lock action SHALL toggle the lock only on a fully closed stationary lockable door when the player is on its authored lock side; wrong-side, open, moving, or un-lockable attempts SHALL leave pose and lock state unchanged. Knock action SHALL produce one distinguishable knock result on the targeted door without opening, unlocking, reversing, or otherwise changing its motion. No action SHALL affect a door through an obstruction or beyond the authored-interaction reach.

#### Scenario: Unlocked door is opened and closed
- **WHEN** the player performs eligible interaction presses at the two endpoints
- **THEN** the door moves toward open and then closed at its authored angular speed, clamping at each endpoint

#### Scenario: Moving or obstructed door receives interaction
- **WHEN** a new eligible interaction press targets a door opening, closing, or stopped between endpoints
- **THEN** the requested direction reverses once without snapping the current pose

#### Scenario: Room door is locked from inside
- **WHEN** the player on the authored bolt side locks a closed stationary door and then requests opening
- **THEN** the lock visibly engages, the later opening request is observably refused, and the leaf remains closed until an eligible unlock

#### Scenario: Locking is unavailable
- **WHEN** lock action targets the wrong side, an open or moving door, or a door with no lock
- **THEN** its lock and motion remain unchanged and the refusal is distinguishable from an accepted lock change

#### Scenario: Player knocks
- **WHEN** an eligible knock action targets a closed, open, locked, or moving door
- **THEN** exactly one knock result is produced for that door without changing its current motion or lock

### Requirement: Obstructed motion without crushing
Door motion SHALL stop at a verified clear pose before its continuous sweep would penetrate structural collision, terrain, a static prop proxy, another door, or the player. Obstruction SHALL stop the current request without automatically resuming when the blocker leaves. A new interaction press SHALL reverse the last requested direction when stopped strictly between endpoints; at an endpoint it SHALL request the opposite endpoint, including retrying when an earlier obstruction allowed no progress. Opening and closing SHALL use the same safety policy. Motion SHALL NOT push, carry, crush, damage, or embed the player, jump across an intervening blocker, or move through a blocker merely because the endpoint is clear. The visible leaf and target blocker SHALL use the accepted pose. Simultaneous door updates SHALL have deterministic outcomes independent of rendering frequency.

#### Scenario: Player blocks closing
- **WHEN** a closing leaf would reach a standing, crouched, or moving player
- **THEN** it stops before penetration, produces an obstruction result, and remains stopped after the player leaves until another eligible interaction

#### Scenario: Thin obstacle lies between endpoints
- **WHEN** a proposed angular movement has clear endpoint bounds but passes through a thin obstacle
- **THEN** motion is stopped before that obstacle rather than crossing it

#### Scenario: Opening is blocked before the first increment
- **WHEN** an opening attempt remains at the closed endpoint because no clear movement was possible, the blocker leaves, and the player presses interaction again
- **THEN** the new press requests opening again without any automatic retry before that press

#### Scenario: Doors meet
- **WHEN** two moving leaves would occupy overlapping space during the same simulation interval
- **THEN** deterministic arbitration accepts only clear motion and neither leaf penetrates the other

#### Scenario: Player follows an opening leaf
- **WHEN** the player moves through an opening doorway or turns beside its hinge
- **THEN** current collision, rendered leaf, and target obstruction remain coherent and the displayed player view does not become embedded in the leaf

### Requirement: Run-local door results and feedback
Accepted open/close requests, endpoint arrival, obstruction, accepted lock changes, refused actions, and knocks SHALL produce bounded concrete results identifying the affected door for the current runtime consumers. Current feedback SHALL consume these results without a general event bus, action registry, subscription framework, or persistent event queue. The player SHALL be able to distinguish locked state, an opening refusal, and a knock using the delivered temporary visual presentation without requiring P04 audio or text. Feedback SHALL NOT change collision, replay an action, or modify authored data. Render skips and recovery SHALL preserve authoritative pose and lock state; restart SHALL restore authored initial state.

#### Scenario: Feedback is inspected without sound
- **WHEN** the player changes the lock, tries a locked handle, and knocks with audio absent
- **THEN** the lock state and the two different action results are visibly distinguishable without the leaf opening

#### Scenario: Presentation recovers during motion
- **WHEN** a frame is skipped or the swapchain recovers while a door is moving or locked
- **THEN** later frames show the current accepted state without resetting or replaying door actions

#### Scenario: Window is minimized
- **WHEN** the framebuffer becomes zero-sized during door movement or feedback
- **THEN** motion and feedback timers pause with simulation and restoration does not consume minimized wall time or activate held actions
