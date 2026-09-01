## 1. Dependency and Module Foundation

- [x] 1.1 Add Jolt Physics `v5.6.0` through `FetchContent`, disable optional samples/tests/viewers/object streams/GPU compute paths, build only the static CPU library, and verify `cmake --preset debug --fresh` succeeds with the required Clang/MSVC-compatible toolchain.
- [x] 1.2 Add concrete `near_laugh_world` and `near_laugh_physics` targets with the designed dependency direction, link Jolt privately only to physics, and verify target-interface and compile-command checks expose neither Jolt nor new third-party requirements to runtime consumers.
- [x] 1.3 Extend source/header boundary checks to permit Jolt only inside the physics module and forbid FPS input, GLFW, and Vulkan dependencies there; verify the positive project sources pass and a deliberately Jolt-leaking fixture is rejected.

## 2. Immutable Prototype World

- [x] 2.1 Define the engine-owned immutable prototype level as axis-aligned colored solids plus a non-overlapping scene-facing player spawn, including floor, boundaries, existing obstacles, a 0.30 m-or-lower walkable step, and a crouch-only low-clearance route; verify world tests cover valid dimensions, containment, semantic feature presence, and spawn clearance.
- [x] 2.2 Replace renderer-private scene authoring with deterministic triangle expansion from the prototype solids while retaining the current vertex format, immutable vertex buffer, shaders, and one draw call; verify scene/vertex-layout tests cover solid face generation, colors, bounds, and depth-overlap invariants.
- [x] 2.3 Pass the same immutable level instance to renderer and physics construction without adding Jolt or Vulkan fields to world data, and verify structural count/placement tests demonstrate that both consumers derive from the same solids.

## 3. Jolt Lifetime and Static Collision

- [x] 3.1 Implement a single RAII Jolt runtime guard for allocator registration, factory ownership, type registration, diagnostics hooks, and reverse teardown; verify focused tests cover repeated create/destroy cycles and injected partial-initialization failures without leaked or duplicate global ownership.
- [x] 3.2 Implement the concrete single-threaded physics world with Jolt's library-provided temporary allocator, `JobSystemSingleThreaded`, and minimal `NonMoving`/`Moving` collision filters; verify a headless test advances the world on the calling thread with no worker pool or engine job system.
- [x] 3.3 Create and own one static Jolt box body for every prototype solid, optimize the broad phase after installation, and verify headless tests cover body count, transforms/extents, floor support, boundary blocking, obstacle collision, and dependency-safe teardown after injected construction failure.

## 4. Physics Character Boundary

- [x] 4.1 Add the physics-owned `CharacterVirtual` with the specified standing/crouched capsules, foot-position conversion, ground-state reporting, step/floor settings, and engine-owned motion/result structures; verify physics headers expose no Jolt types and headless tests cover spawn, falling, settling, and stable supported state.
- [x] 4.2 Implement collision-constrained movement and sliding plus the configured 0.30 m step and 50-degree walkable limit through the virtual-character update path; verify headless tests cover wall blocking, tangential sliding, obstacle rejection, and low-step traversal without tunneling or embedding.
- [x] 4.3 Implement stance shape changes that preserve foot position and reject standing when obstructed; verify headless tests cover crouching, crouch-only passage, blocked stand-up, and eventual standing after clearance.

## 5. Runtime Player Policy and Camera

- [x] 5.1 Replace free-fly translation policy with a concrete runtime player controller using normalized yaw-relative movement, 4.0 m/s walk, 7.0 m/s sprint, 18.0 m/s^2 gravity, 6.5 m/s jump, and bounded 8.0 m/s^2 air control; verify deterministic and headless tests cover each movement axis, diagonal normalization, sprint, falling, grounded motion, and limited airborne steering.
- [x] 5.2 Add rising-edge jump latching that survives a render iteration with no simulation step and is consumed only by an eligible grounded step; verify tests cover press/release, held jump through landing, press while airborne, and exactly-once consumption across multiple catch-up steps.
- [x] 5.3 Retain mouse yaw/pitch and perspective-matrix behavior while deriving camera position from player foot position and standing/crouched eye height; verify camera tests retain Vulkan clip/storage/aspect behavior and cover immediate single-batch look, pitch limits, and stance-relative eye placement.
- [x] 5.4 Preserve cursor capture/release transitions while neutralizing player controls only when released and continuing gravity/collision simulation; verify tests cover release precedence, recapture tracking reset, no movement/look/stance actions while released, and an airborne player continuing to fall.

## 6. Fixed-Step Runtime Integration

- [x] 6.1 Implement a deterministic 60 Hz accumulator with a 100 ms contribution cap, retained fractional remainder, at most six complete steps per sampled interval, interpolation alpha, and explicit reset; verify pure timing tests cover sub-step, exact-step, multi-step, capped-stall, and reset cases.
- [x] 6.2 Track previous/current valid player foot positions and eye heights around fixed steps and produce an interpolated camera position with latest look orientation; verify tests cover zero-step render iterations, multi-step iterations, standing/crouch transitions, endpoint alpha values, and discontinuity-state collapse support.
- [x] 6.3 Recompose Engine lifetime as `Platform -> Window -> PrototypeLevel -> PhysicsWorld -> PlayerController -> Renderer` and integrate input, cursor transitions, fixed simulation, interpolation, and one frame request into the main-thread loop; verify runtime tests cover normal iterations with zero/one/multiple steps, close-before-update, all renderer outcomes, and reverse/partial-construction teardown.
- [x] 6.4 Reset clock and simulation accumulation across minimized blocking waits while sampling the waited input batch before the next poll; verify runtime tests cover minimize/restore without catch-up, waited close, held physical state, and waited cursor delta preservation.
- [x] 6.5 Remove the obsolete `FreeFlyCamera` translation path and update source/target boundaries so physics cannot control events/rendering and rendering cannot consume player input, simulation time, or Jolt types; verify all boundary fixtures still detect their intended violations.

## 7. Documentation and Integrated Validation

- [x] 7.1 Update `README.md` and architecture, gameplay, rendering, and development documentation with Jolt ownership, World/Physics targets, fixed-step policy, grounded controls, shared prototype solids, collision limitations, and the absence of dynamic bodies or free-fly movement; verify documented commands, controls, and runtime behavior match the implementation.
- [x] 7.2 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`; verify all world, physics, character, timing, camera, runtime, process, source, header, and target-boundary tests pass under Clang.
- [x] 7.3 Run `ctest --preset vulkan-smoke --output-on-failure`; verify the changed prototype geometry renders through swapchain recreation with zero unexpected Vulkan validation errors and the existing injected-error/lifecycle cases still pass.
- [x] 7.4 Run the debug `fps` executable on a presentation-capable desktop and verify walking, sprinting, wall sliding, obstacle blocking, low-step traversal, jumping/landing, air control, crouch-only passage, blocked standing, resize interpolation, and cursor release/recapture; record any part that cannot be performed.
  - Validation note (2026-08-30): the unattended executable run initialized validation, selected the presentation device, and created a 1024x768 swapchain. This session could not supply and visually judge the manual keyboard/mouse feel pass; deterministic and headless tests cover the listed movement, collision, timing, and cursor invariants, but a human feel pass remains recommended.
- [x] 7.5 Review `git diff` for unrelated refactors, unplanned abstractions, dependency leakage, or unsupported Jolt features, then run `openspec validate add-grounded-player-controller --strict` and verify the implementation and artifacts remain within the approved single-player prototype scope.
