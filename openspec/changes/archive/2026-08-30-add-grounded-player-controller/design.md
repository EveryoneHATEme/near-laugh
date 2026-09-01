## Context

See `proposal.md` for motivation and the delta specs for required behavior. The current Engine owns `Platform -> Window -> Renderer`, updates an unconstrained `FreeFlyCamera` once per renderable loop iteration, and gives the renderer one backend-neutral view-projection matrix. The prototype scene is renderer-private world-space vertex data, so no engine-owned level description currently exists from which physics collision can be built.

This change crosses dependency configuration, subsystem lifetime, fixed-step timing, input edge handling, camera presentation, scene construction, and deterministic testing. It must preserve the direct Vulkan renderer boundary, remain primarily single-threaded, avoid a generic physics abstraction, and keep the result small enough for one developer to understand.

## Goals / Non-Goals

**Goals:**

- Establish one validation-friendly Jolt lifetime and one main-thread physics world with static prototype collision.
- Produce predictable grounded FPS movement with testable constants and explicit state transitions.
- Give rendering and physics one authoritative set of prototype structural dimensions without introducing entities or a scene graph.
- Separate fixed simulation advancement from rendering while keeping mouse look responsive to each event batch.
- Preserve backend containment and dependency-safe RAII across successful and partial startup.

**Non-Goals:**

- Dynamic rigid bodies, doors, projectiles, ragdolls, vehicles, destructible objects, triggers, or moving platforms.
- A physics-backend interface, RHI-like physics abstraction, generic collision component system, ECS, job system, or custom allocator framework.
- Loading level or collision data from files, general mesh cooking, serialization, save/load, or a debug/editor UI.
- Networking determinism, rollback, replay, or platform support beyond the existing desktop target.
- Weapon/view-model rendering, camera shake, recoil, head bob, leaning, or a general camera framework.

## Decisions

### 1. Pin Jolt Physics 5.6.0 and compile only the required CPU library

Fetch Jolt from immutable tag `v5.6.0` through the existing CMake dependency pattern and link it privately into a new concrete `near_laugh_physics` target. Disable samples, viewer/test programs, object-stream facilities, GPU compute paths, and other optional integrations not required by the CPU rigid-body and character APIs. Treat Jolt headers as third-party/system headers so project warning policy remains strict for project-owned code without attempting to repair upstream diagnostics.

Jolt was selected over PhysX because it has a smaller source/build surface, a first-class virtual character, an official CMake embedding path, and an upstream single-thread job implementation. Bullet was rejected because its current character and integration story is less attractive for new FPS code. ReactPhysics3D was rejected because it would leave stair, stance, and grounded-player behavior largely project-owned.

### 2. Add concrete World and Physics modules without interchangeable interfaces

Add `near_laugh_world` for immutable engine-owned prototype data and `near_laugh_physics` for Jolt-backed simulation. `near_laugh_world` contains only scalar/vector-like value data and has no rendering, physics, platform, or gameplay dependency. Rendering and physics may both consume that data; they do not depend on each other. `near_laugh_runtime` composes them and owns player policy.

The dependency shape is:

```text
                      +--------------------+
                      | near_laugh_runtime |
                      +----+-----------+---+
                           |           |
              +------------+           +------------+
              v                                     v
    +--------------------+                 +-------------------+
    | near_laugh_physics |                 | near_laugh_render |
    +----------+---------+                 +---------+---------+
               |                                     |
               +-----------------+-------------------+
                                 v
                       +------------------+
                       | near_laugh_world |
                       +------------------+
```

This is a meaningful data-ownership boundary used immediately by two consumers, not a framework for hypothetical worlds. No `IPhysicsBackend`, generic body facade, component registry, or renderer-facing gameplay object is introduced.

### 3. Represent the prototype level as immutable axis-aligned solids

Replace the renderer-private authored vertex stream with one immutable `PrototypeLevel` value containing a short list of solid axis-aligned boxes, packed display colors, and one player spawn. Floor and walls become thin solids rather than zero-thickness quads. The list also includes the existing colored obstacles, a low walkable step, and a low-clearance passage sized to distinguish standing and crouched shapes.

The renderer expands each solid into the existing position/color triangle format and retains one immutable vertex buffer and one draw call. Physics expands the same solids into static box shapes. This guarantees matching structural transforms without sharing vertex buffers or Jolt/Vulkan types. A triangle-mesh level representation was rejected because every required prototype structure is box-shaped and mesh cooking would introduce unsupported asset-pipeline concerns.

### 4. Contain Jolt's process setup and world ownership with RAII

`near_laugh_physics` owns a concrete Jolt runtime guard followed by the library factory/type registration, temporary allocator, `JobSystemSingleThreaded`, collision-layer filters, `PhysicsSystem`, static body identifiers, and one `CharacterVirtual`. Member/declaration order and focused injected-failure tests enforce reverse destruction. The unavoidable Jolt process-level factory pointer remains confined behind the single runtime guard and is restored during teardown.

Use only `NonMoving` and `Moving` object layers with matching broad-phase filters. Prototype solids are non-moving; the virtual character queries them as moving gameplay state but does not initially create an inner rigid body because sensors and dynamic-body interaction are outside this slice. Use Jolt's library-provided temporary allocator rather than introducing project allocator policy.

Initialization/assert/trace failures are converted into actionable project diagnostics at the physics boundary. Jolt types appear only in physics implementation files or the private implementation of physics-owned classes; public application headers, frame requests, world data, platform input, renderer APIs, and runtime policy use engine-owned scalar structures.

### 5. Keep movement policy runtime-owned and collision resolution physics-owned

A concrete `PlayerController` in `near_laugh_runtime` owns yaw, pitch, jump-edge latching, stance intent, horizontal velocity policy, and previous/current presentation poses. A concrete `PhysicsWorld` owns the Jolt character and exposes the narrow operations this player needs using engine-owned values: advance a character motion request, report position/velocity/ground state/actual stance, and test or apply a stance shape change. It does not receive physical key codes or `FpsActionSnapshot`.

Use `CharacterVirtual` because the local FPS player needs precise update ordering, explicit velocity policy, stair/floor helpers, and camera-independent collision. Use its extended update path for floor sticking and step traversal. A rigid-body character was rejected because stronger reciprocal interaction with dynamic bodies is not yet required and would cede more movement control to the solver.

Initial tuning constants remain private, named, and directly covered by deterministic tests:

- capsule radius: 0.35 m;
- standing height / eye height: 1.80 m / 1.65 m;
- crouched height / eye height: 1.20 m / 1.05 m;
- walk / sprint speed: 4.0 m/s / 7.0 m/s;
- gravity: 18.0 m/s^2 downward;
- jump speed: 6.5 m/s;
- maximum step height: 0.30 m;
- maximum walkable slope: 50 degrees;
- mouse sensitivity and 89-degree pitch limit retained from the prototype camera;
- grounded velocity follows normalized requested velocity directly, while airborne horizontal velocity approaches the request with a bounded 8.0 m/s^2 acceleration.

The controller treats position as the grounded foot position even though Jolt stores the character at its shape origin. Physics performs that conversion in one place. Crouch swaps to the shorter capsule without changing the foot position. Standing uses Jolt's shape-change collision check and remains crouched when obstructed. A jump press is latched on the input transition and consumed by the first eligible fixed step; holding jump cannot enqueue another press until release.

### 6. Advance simulation at 60 Hz with bounded accumulation and render interpolation

Replace the free-fly `FrameClock` policy with a deterministic fixed-step accumulator. Each renderable main-loop iteration samples `steady_clock`, clamps the contribution to 100 ms, and executes complete 1/60-second steps. The clamp inherently limits one iteration to at most six steps; remaining time below one step is retained. Blocking minimized-window waits reset both the clock origin and accumulator. Cursor release does not pause physics or discard time; it substitutes neutral player-control input while simulation continues.

Held movement/sprint/crouch state is reused across all steps in one iteration. A newly observed jump edge remains queued if the iteration has no complete fixed step and is consumed at most once when a grounded step accepts it. Look delta is applied exactly once before stepping and affects both movement orientation for those steps and the rendered orientation; it is never multiplied by the number of substeps.

Store the previous and current valid foot position and stance eye height around each fixed step. Render with `alpha = accumulator / fixed_step` and linearly interpolate those presentation values while using the latest yaw and pitch. The renderer still receives only its current `CameraFrame`; it remains unaware of simulation, Jolt, interpolation, and player actions. Teleports are not part of this slice; a later teleport operation must explicitly collapse previous/current presentation state.

### 7. Compose lifetime as Platform, Window, World, Physics, Renderer

The Engine owns members in construction order `Platform -> Window -> PrototypeLevel -> PhysicsWorld -> PlayerController -> Renderer`, with the player controller's physics reference non-owning and bounded by member order. Renderer and physics both receive the immutable level during construction. Shutdown reverses that order, ensuring rendering stops before physics and level data disappear, and physics destroys the character and bodies before Jolt teardown.

The application loop retains all decisions: poll, map input, apply cursor transition, update look/control state, advance fixed steps, interpolate a camera frame, request at most one render, and exhaustively consume the renderer outcome. A close request exits before simulation. A zero framebuffer enters the existing blocking wait without simulation or rendering.

### 8. Test physics deterministically without requiring Vulkan or a window

Keep pure tests for the accumulator, jump latch, movement normalization, look update, interpolation, and cursor policy. Add headless physics integration tests using the real Jolt world and prototype level for startup/teardown, falling and ground support, wall blocking/sliding, obstacle rejection, low-step traversal, jump/landing/held-jump behavior, air control, crouch foot stability, blocked standing, and eventual standing after clearance.

Extend source/header/target boundary tests to reject Jolt includes or link requirements outside `near_laugh_physics` and to reject FPS input dependencies in physics or rendering. Scene tests verify every authored solid produces expected renderer geometry and a matching physics solid. The Vulkan smoke run remains responsible for validation-clean rendering and lifecycle behavior after the visible scene geometry changes; a manual run validates movement feel and visible/collision agreement.

## Risks / Trade-offs

- **[CharacterVirtual tuning may feel sticky, floaty, or too permissive at edges]** -> Keep all movement, step, contact, and recovery constants localized; cover geometric invariants automatically and require a manual feel pass before completion.
- **[Jolt's global factory/type registration can undermine explicit lifetime]** -> Permit exactly one RAII runtime guard, keep it inside the physics module, test partial construction and repeated create/destroy cycles, and forbid other code from touching Jolt globals.
- **[A fetched dependency can expand build time or enable unwanted features]** -> Pin immutable `v5.6.0`, build only the static CPU library, disable samples/tests/viewers/GPU paths, and inspect target interfaces and compile commands.
- **[Visible and collision geometry can drift during conversion]** -> Author dimensions once in `PrototypeLevel`, derive both outputs deterministically, and compare structure counts/transforms in tests.
- **[Fixed-step interpolation adds one simulation-step of presentation latency]** -> Accept at most 16.7 ms of positional latency for smooth movement while applying look immediately; measure before considering prediction or a variable-step player path.
- **[Interpolating between two collision-valid poses can visually cut a very tight corner]** -> Keep the capsule eye point inside both endpoint shapes, use the small fixed interval, and validate the low-clearance and wall-slide cases; fall back to current pose for discontinuities.
- **[The initial two-layer collision model is intentionally narrow]** -> Add layers only when a concrete sensor, projectile, enemy, or dynamic-body interaction requires them rather than anticipating a general taxonomy.

## Migration Plan

1. Pin and configure Jolt, establish the World/Physics targets and boundary checks, and prove Clang/MSVC-ABI configuration before changing runtime behavior.
2. Move prototype structural data into the immutable world description and derive renderer geometry and headless static collision from it.
3. Add RAII Jolt lifetime, the single-thread physics world, virtual character, and deterministic physics tests.
4. Add player movement, stance, jump, look, fixed-step accumulation, and presentation interpolation with focused tests.
5. Replace the Engine free-fly path, update documentation/boundary checks, then run deterministic and Vulkan validation plus a manual movement pass.

There is no persisted-data migration. Rollback removes the Jolt and World/Physics targets, restores renderer-private prototype vertices and the free-fly camera path, and removes the grounded-player delta behavior as one source-level reversal.
