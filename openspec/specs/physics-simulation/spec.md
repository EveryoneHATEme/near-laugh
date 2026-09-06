# physics-simulation Specification

## Purpose

Defines deterministic main-thread physics for one local player, authored structural and prop collision, and coherent moving door collision while containing the selected physics library.

## Requirements

### Requirement: Explicit physics lifetime
The runtime SHALL own exactly one physics-library lifetime and one physics world, SHALL initialize them before creating physics-dependent world state, and SHALL destroy all characters and bodies before releasing the world and library lifetime. A partially completed physics startup SHALL release each successfully created resource exactly once.

#### Scenario: Successful physics lifetime
- **WHEN** the application starts and later shuts down normally
- **THEN** the physics world and its characters and bodies are released in dependency-safe order before the physics-library lifetime ends

#### Scenario: Physics startup fails
- **WHEN** physics initialization or static collision creation fails
- **THEN** startup reports an actionable error and releases all already-created physics and runtime resources without entering the main loop

### Requirement: Bounded fixed-step simulation
The runtime SHALL advance player and physics state on the main thread in fixed one-sixtieth-second steps, SHALL retain only the sub-step remainder needed for a later iteration, and SHALL bound each sampled wall-clock interval to 100 milliseconds before accumulating it. The runtime SHALL reset the accumulator after a blocking minimized-window wait so blocked time is never simulated as catch-up work.

#### Scenario: Rendering runs faster than simulation
- **WHEN** one render-loop interval contributes less than one fixed simulation step
- **THEN** no partial physics step is executed and the elapsed remainder is retained for a later iteration

#### Scenario: Rendering runs slower than simulation
- **WHEN** a bounded render-loop interval spans multiple fixed steps
- **THEN** the runtime executes the corresponding complete steps in order before producing the next rendered player state

#### Scenario: Runtime resumes after a blocking wait
- **WHEN** the framebuffer becomes renderable after the runtime blocked for minimized-window events
- **THEN** the first restored iteration begins with an empty simulation accumulator and does not apply the blocked duration to player or physics state

### Requirement: Static prototype collision
The physics world SHALL create static collision for terrain only when the selected immutable level declares it, every declared solid floor, boundary, obstacle, step, or low-clearance structure, and every declared static placement box. It SHALL NOT synthesize terrain or an invisible floor for an interior. Corresponding visible and collision terrain surfaces SHALL share the same height samples, placement, and dimensions; corresponding generated solid structures SHALL share the same structural dimensions; and each imported placement SHALL use its separately authored finite local boxes transformed by its translation, yaw and uniform scale rather than deriving collision from loaded render triangles. Character initialization SHALL use the resolved entry pose after level and entry validation. Partial collision construction SHALL remain safe both with and without a terrain body.

#### Scenario: Static collision world starts
- **WHEN** the selected level is installed into a newly initialized physics world
- **THEN** its declared terrain when present, generated solid structures, and static placement boxes are available for character collision before the first simulation step

#### Scenario: Visible structure is tested for collision
- **WHEN** the player reaches visible terrain, a floor, boundary, obstacle, step, or low-clearance structure
- **THEN** collision is evaluated against a surface with matching placement and dimensions

#### Scenario: Imported prop is tested for collision
- **WHEN** the player reaches the visible static model prop
- **THEN** collision is evaluated against that placement's declared simple boxes at the same world placement without loading model geometry into the physics module

#### Scenario: Interior collision starts without terrain
- **WHEN** a validated terrain-free interior starts at its lower-landing or apartment entry
- **THEN** the character begins at the selected authored feet position with structural collision already installed and no terrain body or hidden floor

#### Scenario: Terrain-free collision startup fails
- **WHEN** collision construction fails after some interior solids or some placement boxes have been created
- **THEN** all successfully created bodies and physics owners are released exactly once without relying on a terrain body having existed

#### Scenario: Placement has no collision boxes
- **WHEN** a decorative placement is present with an empty collision-box list
- **THEN** the placement creates no physics body and does not obstruct player movement or visibility queries

#### Scenario: Repeated static prop blocks a moving door
- **WHEN** a P03 door attempts to swing through a box of any authored static placement
- **THEN** the existing bounded door-obstruction behavior prevents penetration without deriving collision from that model

### Requirement: Physics dependency boundary
Physics implementation types SHALL remain confined to the physics module and SHALL NOT appear in public runtime headers, platform input, renderer interfaces, frame requests, or immutable prototype-level data. The initial physics implementation SHALL execute without creating worker threads or requiring an engine job system.

#### Scenario: Dependency boundary is inspected
- **WHEN** public headers, project targets, and source dependencies are inspected
- **THEN** the physics-library dependency terminates at the physics module and renderer, platform, world-data, and runtime consumers exchange only project-owned types

#### Scenario: Initial simulation executes
- **WHEN** the physics world advances during the grounded-player prototype
- **THEN** all physics work completes on the calling main thread without an engine job scheduler or physics worker pool

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
