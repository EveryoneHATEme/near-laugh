## MODIFIED Requirements

### Requirement: Bounded changing door collision
Physics SHALL install a box leaf matching each authored door's initial pose before character simulation and SHALL accept only validated changing poses approved by the runtime's door policy. Continuous angular coverage SHALL prevent leaf penetration of static collision, other leaves, the player, or scripted actors; endpoint-only clearance SHALL NOT suffice. Player and actor traversal and player stance clearance SHALL use the installed accepted leaves. Door updates SHALL remain main-thread and fixed-step, retain the existing single-threaded physics dependency boundary, and SHALL NOT require render models or expose native body identities outside physics. Initial or partial door-body construction failure SHALL release every created body exactly once.

#### Scenario: Player encounters a closed or moving door
- **WHEN** the player walks, crouches, or tries to stand at a door
- **THEN** collision uses its accepted leaf bounds and does not allow walking through or standing into it

#### Scenario: Partial door collision startup fails
- **WHEN** door-body creation fails after static geometry and some doors have initialized
- **THEN** startup reports the failing door and releases all created bodies and owners in dependency-safe order

#### Scenario: Rotating leaf crosses a thin blocker
- **WHEN** a fixed-step candidate rotation sweeps through a thin blocker while both endpoint poses are clear
- **THEN** the candidate is not accepted across that blocker

### Requirement: Current door visibility obstruction
Interaction obstruction SHALL include installed door leaves and static terrain, solids, authored prop proxies, and current scripted actor proxies. Queries SHALL exclude the player's representation and SHALL distinguish the selected door from other blockers so it can be targeted at its front surface. Invalid segments or origins within blocking geometry SHALL be rejected. A door behind the target SHALL NOT prevent activation.

#### Scenario: Dynamic leaf hides a switch
- **WHEN** the current door leaf crosses the otherwise clear segment to a switch
- **THEN** the obstruction query blocks that interaction at the same pose the player sees

#### Scenario: Door is itself selected
- **WHEN** the first visible surface on a valid targeting ray belongs to the selected door
- **THEN** that door is targetable while nearer unrelated collision still prevents activation

## ADDED Requirements

### Requirement: Bounded accepted scripted actor collision
Physics SHALL own zero through four actor proxies alongside exactly one local player in the existing single physics world. Proxies SHALL use the model catalog's upright capsule at the accepted feet position and SHALL NOT depend on render mesh loading or individual animated bones. Actor motion SHALL use continuous swept clearance and support checks against static terrain/solids/prop boxes, installed doors, other actor proxies and the player's swept stance envelope. Endpoint-only overlap checks SHALL NOT allow tunneling. Accepted actor poses SHALL block player traversal/standing and interaction queries; actor proxies SHALL NOT be treated as walkable ground. Doors SHALL respect current actor proxies and any swept envelope needed for coherent display. Updates SHALL be deterministic on the main thread, with the physics world advanced once per fixed step rather than once per actor. Invalid initial placement or partial proxy creation SHALL fail safely with actor context.

#### Scenario: Actor crosses a thin obstacle between endpoints
- **WHEN** a requested route displacement has clear endpoints but sweeps through a thin wall or player envelope
- **THEN** it stops at a supported clear pose before the obstacle and reports only actual accepted displacement

#### Scenario: Player approaches a stationary actor
- **WHEN** the player walks or tries to stand into the actor proxy
- **THEN** collision prevents overlap without moving the actor or letting its head act as a traversable floor

#### Scenario: Actor and player exchange sides in one step
- **WHEN** their attempted paths would cross even though their final requested positions do not overlap
- **THEN** protected swept motion prevents either from passing through the other

#### Scenario: Capacity actors are reordered
- **WHEN** equivalent four-actor definitions are reordered before the same fixed input sequence
- **THEN** accepted positions and obstruction results agree by durable actor ID

#### Scenario: Actor construction fails partway
- **WHEN** actor proxy creation fails after static geometry, the player and earlier proxies exist
- **THEN** all created physics resources release once before the world and library lifetime end
