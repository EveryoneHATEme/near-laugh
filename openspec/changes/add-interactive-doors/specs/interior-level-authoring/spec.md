## MODIFIED Requirements

### Requirement: Apartment and stairs acceptance scene
The project SHALL package a temporary interior acceptance level alongside the prototype. It SHALL contain Lena's room, a corridor, a kitchen, rear stairs, and a lower landing; no terrain; a default `apartment` entry; and a `lower-landing` entry. It SHALL include an initially closed unlocked generated door for Lena's room with an interior lock side, reachable from both sides, and a reachable light-switch arrangement whose view can be blocked by that leaf. After opening the room door from either side, the route SHALL be traversable in both directions with ordinary walking and the existing step behavior, without requiring jumping, crouching, terrain manipulation, or a movement-policy change. It SHALL retain the current two-light and single-chair profile and use temporary structural content.

#### Scenario: Both floors are exercised
- **WHEN** the saved acceptance scene is launched separately from each entry
- **THEN** the player starts on the correct floor, can open the room door from the approach side and traverse the connected route in both directions, and is blocked by the authored walls and slabs

#### Scenario: Authoring acceptance is repeated
- **WHEN** an author creates an interior, edits structural geometry and both entry poses, saves it, reopens it, and plays it from the editor
- **THEN** the saved authored values determine the playable scene without replacing the packaged prototype or modifying another open document


#### Scenario: Door acceptance is exercised
- **WHEN** the player opens and closes the room door, locks it from inside, attempts opening while locked, unlocks, knocks, blocks its swing, and tests the switch through the leaf
- **THEN** the concrete action, no-crushing obstruction, and target arbitration requirements can be checked in the packaged scene without imported door assets or changes to the player movement policy
