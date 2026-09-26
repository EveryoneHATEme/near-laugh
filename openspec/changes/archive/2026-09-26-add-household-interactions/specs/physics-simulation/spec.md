## MODIFIED Requirements

### Requirement: Bounded changing door collision
Physics SHALL install a box leaf matching each authored door's initial pose before character simulation and SHALL accept only validated changing poses approved by the runtime's door policy. Continuous angular coverage SHALL prevent leaf penetration of static collision, other leaves, current physical boxes, the player, or scripted actors; endpoint-only clearance SHALL NOT suffice. Player and actor traversal and player stance clearance SHALL use the installed accepted leaves. Door updates SHALL remain main-thread and fixed-step, retain the existing single-threaded physics dependency boundary, and SHALL NOT require render models or expose native body identities outside physics. Initial or partial door-body construction failure SHALL release every created body exactly once.

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
Interaction obstruction SHALL include installed door leaves and static terrain, solids, authored prop proxies, current physical household boxes, and current scripted actor proxies. Queries SHALL exclude the player's representation and SHALL distinguish the selected door or physical box from other blockers so it can be targeted at its front surface. Only the selected object itself SHALL be excluded; nearer unrelated blockers and invalid inside origins SHALL remain effective. Invalid segments or origins within blocking geometry SHALL be rejected. A door behind the target SHALL NOT prevent activation.

#### Scenario: Dynamic leaf hides a switch
- **WHEN** the current door leaf crosses the otherwise clear segment to a switch
- **THEN** the obstruction query blocks that interaction at the same pose the player sees

#### Scenario: Door is itself selected
- **WHEN** the first visible surface on a valid targeting ray belongs to the selected door
- **THEN** that door is targetable while nearer unrelated collision still prevents activation

### Requirement: Bounded accepted scripted actor collision
Physics SHALL own zero through four actor proxies alongside exactly one local player in the existing single physics world. Proxies SHALL use the model catalog's upright capsule at the accepted feet position and SHALL NOT depend on render mesh loading or individual animated bones. Actor motion SHALL use continuous swept clearance and support checks against static terrain/solids/prop boxes, current physical household boxes, installed doors, other actor proxies and the player's swept stance envelope. Endpoint-only overlap checks SHALL NOT allow tunneling. Accepted actor poses SHALL block player traversal/standing and interaction queries; Actor proxies SHALL NOT be treated as walkable player ground; neither actor proxies nor physical household boxes SHALL provide actor route support. Physical household boxes SHALL obstruct actor movement without becoming targets for pushing or manipulation, and their impacts SHALL NOT displace actors from accepted route poses. Doors SHALL respect current actor proxies and any swept envelope needed for coherent display. Updates SHALL be deterministic on the main thread, with the physics world advanced once per fixed step rather than once per actor. Invalid initial placement or partial proxy creation SHALL fail safely with actor context.

#### Scenario: Actor crosses a thin obstacle between endpoints
- **WHEN** a requested route displacement has clear endpoints but sweeps through a thin wall or player envelope
- **THEN** it stops at a supported clear pose before the obstacle and reports only actual accepted displacement

#### Scenario: Actor transitions between slope and flat ground
- **WHEN** accepted support changes the vertical offset between ground feet and the catalog capsule
- **THEN** physics sweeps the actual capsule placement through that change, preserves supported ground feet for presentation/arrival, and rejects any lift or movement through overhead geometry, a player, another actor or a door

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

#### Scenario: Box blocks an actor route
- **WHEN** a free or held physical box enters an actor's next swept movement
- **THEN** the actor waits at its last clear supported pose and resumes only through its existing route policy after clearance, without shoving the box


## ADDED Requirements

### Requirement: Bounded dynamic household box lifetime
Physics SHALL own at most 16 dynamic household box bodies in the existing world, with the supported dimensions/mass, gravity, full orientation, contact response, bounded velocities and sleeping. Bodies SHALL be created in stable durable-ID order and expose only project-owned physical state. Boxes SHALL collide with static collision, other boxes, installed doors, accepted actors and both standing and crouched representations of a stationary or moving player. A player representation usable by dynamic bodies SHALL exist before the first world step; failed creation or stance-shape replacement SHALL retain a valid preceding state or fail startup safely. Boxes SHALL remain bodies while held; holding SHALL NOT remove world collision or require a recreated body. Failed startup/hold setup and teardown SHALL release constraints before referenced bodies and every resource once. Physics update errors SHALL be reported rather than accepted as successful progression.

#### Scenario: Box strikes a stationary player
- **WHEN** a supported-speed thrown box contacts the standing or crouched player without new player movement
- **THEN** it collides with the current player shape rather than passing through the camera or waiting for later movement to discover contact

#### Scenario: Construction fails after some boxes exist
- **WHEN** player collision representation, a later box body or the hold constraint cannot be created
- **THEN** no invalid body is used, acquired resources are released in dependency order, and failed hold acquisition leaves the box free

### Requirement: Collision-respecting physical holding and release
Physics SHALL apply bounded holding force/torque and one-shot throw impulses at fixed-step boundaries. The held body SHALL remain responsive to new targets after extended stillness and suspension; sleeping SHALL NOT prevent target following. The held body SHALL approach the requested pose through physical motion without teleportation, disabling collision or transferring its penetration to the player. Drop and throw SHALL remove hold forces before subsequent physical advancement, with drop adding no impulse and throw adding exactly one bounded impulse. Invalid target state or excessive separation SHALL release safely at the actual pose. Accepted body state SHALL remain finite after repeated obstruction and release.

#### Scenario: Fast camera turn traps a held box
- **WHEN** an abrupt turn requests a hold pose across intervening collision
- **THEN** the physical box cannot be pulled through it, bounded forces cannot accumulate an unbounded release velocity, and break-distance policy releases the existing body safely

### Requirement: Supported physical contact and simulation stability
The fixed 0.30 metre box profile SHALL support falling, off-centre impacts, box-to-box contact and stable rest under declared bounded linear/angular speeds. Continuous collision handling and, where needed, fixed collision substeps SHALL prevent tunneling through supported walls and door leaves at those speeds, including rotating impact tests. Equivalent fixed-step inputs on the same supported build SHALL produce matching logical transitions and physical states within documented numeric tolerances across render-batch partitioning; cross-platform bit-identical rigid-body simulation SHALL NOT be promised. No additional simulation time SHALL be introduced by presentation recovery. Boxes SHALL not propel the player or actors; player movement SHALL be able to impart bounded pushes to free boxes without allowing penetration or physical pickup of other objects.

#### Scenario: Maximum-speed spinning impact meets a thin wall
- **WHEN** a spinning box at the supported speed limits hits a 0.02 metre wall or a minimum-valid 0.04 metre door leaf
- **THEN** it stays on the reachable side with finite pose/velocity and does not cross through the blocker between accepted states

#### Scenario: Resting capacity scene is perturbed
- **WHEN** 16 boxes settle on floors or in small stacks and one is picked up, dropped or thrown into another
- **THEN** affected bodies wake and respond without explosive energy growth, unbounded resource use or duplicate body identities

#### Scenario: Event batches partition the same fixed steps
- **WHEN** the same commands and accepted simulation-eye/look sequence are applied at the same fixed boundaries under different render-batch groupings
- **THEN** hold/drop/throw counts agree exactly and accepted physical states agree within the declared numeric tolerance


#### Scenario: Stationary held body is moved again
- **WHEN** a held box remains still beyond normal sleeping time and the fixed-step look/target changes, including after suspension
- **THEN** the same held body follows through bounded physical motion without requiring a drop and second pickup
