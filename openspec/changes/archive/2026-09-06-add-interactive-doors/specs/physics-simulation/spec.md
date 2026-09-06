## ADDED Requirements

### Requirement: Bounded changing door collision
Physics SHALL install a box leaf matching each authored door's initial pose before character simulation and SHALL accept only validated changing poses approved by the runtime's door policy. Continuous angular coverage SHALL prevent leaf penetration of static collision, other leaves, or the player; endpoint-only clearance SHALL NOT suffice. Character traversal and stance clearance SHALL use the installed accepted leaves. Door updates SHALL remain main-thread and fixed-step, retain the existing single-threaded physics dependency boundary, and SHALL NOT require render models or expose native body identities outside physics. Initial or partial door-body construction failure SHALL release every created body exactly once.

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
Interaction obstruction SHALL include installed door leaves and static terrain, solids, and authored prop proxies. Queries SHALL exclude the player's representation and SHALL distinguish the selected door from other blockers so it can be targeted at its front surface. Invalid segments or origins within blocking geometry SHALL be rejected. A door behind the target SHALL NOT prevent activation.

#### Scenario: Dynamic leaf hides a switch
- **WHEN** the current door leaf crosses the otherwise clear segment to a switch
- **THEN** the obstruction query blocks that interaction at the same pose the player sees

#### Scenario: Door is itself selected
- **WHEN** the first visible surface on a valid targeting ray belongs to the selected door
- **THEN** that door is targetable while nearer unrelated collision still prevents activation
