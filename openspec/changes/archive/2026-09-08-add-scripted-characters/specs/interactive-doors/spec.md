## MODIFIED Requirements

### Requirement: Obstructed motion without crushing
Door motion SHALL stop at a verified clear pose before its continuous sweep would penetrate structural collision, terrain, a static prop proxy, another door, the player, or a scripted actor. Obstruction SHALL stop the current request without automatically resuming when the blocker leaves. A new interaction press SHALL reverse the last requested direction when stopped strictly between endpoints; at an endpoint it SHALL request the opposite endpoint, including retrying when an earlier obstruction allowed no progress. Opening and closing SHALL use the same safety policy. Motion SHALL NOT push, carry, crush, damage, or embed the player or actors, jump across an intervening blocker, or move through a blocker merely because the endpoint is clear. The visible leaf and target blocker SHALL use the accepted pose. Simultaneous door updates SHALL have deterministic outcomes independent of rendering frequency.

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

#### Scenario: Actor blocks a swinging door
- **WHEN** a moving actor or its accepted stationary proxy obstructs an opening or closing leaf
- **THEN** the leaf stops before penetration and retains P03's requirement for a new eligible interaction after clearance, while a blocked actor may independently retry its route
