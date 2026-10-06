## MODIFIED Requirements

### Requirement: Concrete door operation
One accepted interaction action SHALL request opening from the closed endpoint and closing from the open endpoint. Pressing interaction while the door is moving or stopped between endpoints SHALL reverse its last requested direction. A locked door SHALL refuse opening without changing its leaf pose or locked state. Lock action SHALL toggle the lock only on a fully closed stationary lockable door when the player is on its authored lock side; wrong-side, open, moving, or un-lockable attempts SHALL leave pose and lock state unchanged. Knock action SHALL produce one distinguishable knock result on the targeted door without opening, unlocking, reversing, or otherwise changing its motion. No player action SHALL affect a door through an obstruction or beyond the authored-interaction reach.

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
Door motion SHALL stop at a verified clear pose before its continuous sweep would penetrate structural collision, terrain, a static prop proxy, a current physical household box, another door, the player, or a scripted actor. Obstruction SHALL stop the current request without automatically resuming when the blocker leaves. A new interaction press SHALL reverse the last requested direction when stopped strictly between endpoints; at an endpoint it SHALL request the opposite endpoint, including retrying when an earlier obstruction allowed no progress. Opening and closing SHALL use the same safety policy. Motion SHALL NOT push, carry, crush, damage, or embed the player, actors or physical boxes, jump across an intervening blocker, or move through a blocker merely because the endpoint is clear. The visible leaf and target blocker SHALL use the accepted pose. Simultaneous door updates SHALL have deterministic outcomes independent of rendering frequency.

#### Scenario: Player blocks closing
- **WHEN** a closing leaf would reach a standing, crouched, or moving player
- **THEN** it stops before penetration, produces an obstruction result, and remains stopped after the player leaves until another explicit accepted player or authored endpoint request

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

#### Scenario: Actor blocks a swinging door
- **WHEN** a moving actor or its accepted stationary proxy obstructs an opening or closing leaf
- **THEN** the leaf stops before penetration and retains P03's requirement for a new explicit accepted player or authored endpoint request after clearance, while a blocked actor may independently retry its route

#### Scenario: Free or held box obstructs a door
- **WHEN** a physical household box occupies part of a requested door sweep
- **THEN** the leaf stops at a verified clear pose without shoving or crushing the box, and clearing the box alone does not resume the request

#### Scenario: Box hits an accepted leaf
- **WHEN** a thrown box contacts the current accepted door pose
- **THEN** physical contact is resolved against that pose without changing the door's lock or motion intent

## ADDED Requirements

### Requirement: Explicit authored door targets
Authored events SHALL request open/closed endpoints and locked/unlocked values without synthesizing a player eye or input press. A desired value already satisfied SHALL be an accepted no-op; an unchanged active endpoint target SHALL not reverse motion. Opening while locked SHALL be refused. A lock-state change SHALL require a lockable, fully closed stationary door but no player-side/reach test. Refusal SHALL leave pose, target and lock unchanged and fail the requesting event rather than retry automatically. Accepted endpoint requests SHALL preserve sweep/obstruction rules and complete their command on acceptance, not endpoint arrival. An explicit new endpoint request after obstruction SHALL be allowed to retry the same direction. Event cancellation SHALL not roll back accepted door targets or locks.

#### Scenario: Moving door receives its current target again
- **WHEN** an authored request asks an opening door to open
- **THEN** it continues toward the same endpoint without reversing or snapping

#### Scenario: Event tries to lock a moving door
- **WHEN** the requested lock value differs and the leaf is not stationary at closed
- **THEN** the command is refused with a reason and the event fails without changing motion

#### Scenario: Already unlocked door receives unlock
- **WHEN** an open door already has the requested unlocked state
- **THEN** unlock is an accepted no-op without moving or relocking it

#### Scenario: Explicit request retries an obstructed door
- **WHEN** an obstacle has cleared and a new authored request specifies the previously obstructed endpoint
- **THEN** a new sweep-constrained movement attempt starts without interpreting the command as a toggle
