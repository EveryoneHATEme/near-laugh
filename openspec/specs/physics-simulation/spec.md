# physics-simulation Specification

## Purpose

Defines the deterministic main-thread physics simulation required for the single-player game, including explicit lifetime, fixed-step advancement, static prototype-level collision, and containment of the selected physics library.

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
The physics world SHALL create static collision for the finite terrain surface; every solid floor where present, boundary, obstacle, movement-test step, and low-clearance structure; and the one static model prop declared by the immutable prototype-level description. Corresponding visible and collision terrain surfaces SHALL share the same height samples, placement, and dimensions; corresponding generated solid structures SHALL share the same structural dimensions; and the imported prop SHALL use its separately authored finite box proxy at the declared model placement rather than deriving collision from loaded render triangles.

#### Scenario: Static collision world starts
- **WHEN** the prototype level is installed into a newly initialized physics world
- **THEN** its declared terrain, generated solid structures, and static model-prop proxy are available for character collision before the first simulation step

#### Scenario: Visible structure is tested for collision
- **WHEN** the player reaches visible terrain, a floor, boundary, obstacle, step, or low-clearance structure
- **THEN** collision is evaluated against a surface with matching placement and dimensions

#### Scenario: Imported prop is tested for collision
- **WHEN** the player reaches the visible static model prop
- **THEN** collision is evaluated against the prop's declared simple box proxy at the same world placement without loading model geometry into the physics module

### Requirement: Physics dependency boundary
Physics implementation types SHALL remain confined to the physics module and SHALL NOT appear in public runtime headers, platform input, renderer interfaces, frame requests, or immutable prototype-level data. The initial physics implementation SHALL execute without creating worker threads or requiring an engine job system.

#### Scenario: Dependency boundary is inspected
- **WHEN** public headers, project targets, and source dependencies are inspected
- **THEN** the physics-library dependency terminates at the physics module and renderer, platform, world-data, and runtime consumers exchange only project-owned types

#### Scenario: Initial simulation executes
- **WHEN** the physics world advances during the grounded-player prototype
- **THEN** all physics work completes on the calling main thread without an engine job scheduler or physics worker pool
