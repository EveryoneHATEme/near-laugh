## Why

The prototype demonstrates camera-controlled 3D rendering but still allows the viewer to fly through every surface, so it cannot exercise the grounded movement and collision behavior required by the FPS. Adding one physics-backed player now turns the rendering prototype into the first gameplay-oriented vertical slice and establishes only the World and Physics boundaries that this concrete requirement needs.

## What Changes

- Add a pinned Jolt Physics dependency behind a concrete engine-owned physics module, using Jolt's single-threaded job implementation and library-provided allocation facilities for the initial simulation.
- Add one fixed-step physics world containing static collision for the built-in prototype environment and one Jolt `CharacterVirtual` capsule for the local player.
- Replace free-fly translation with grounded walking, sprinting, gravity, collision sliding, jumping, limited air movement, and hold-to-crouch behavior, including clearance checks before standing.
- Keep mouse look and the backend-neutral camera frame runtime-owned, but derive camera position from the simulated player and use an eye-height offset appropriate to standing or crouching.
- Move the prototype environment's structural dimensions into one small immutable world description used to derive both renderer geometry and physics collision, and add simple movement-test geometry without introducing a scene hierarchy or asset pipeline.
- Run simulation independently of rendering through a bounded fixed-step accumulator, avoid catch-up after minimized or blocked waits, and interpolate player presentation state for render frames.
- Preserve the existing cursor release/recapture behavior; while released, player control input is neutral but active simulation continues.
- **BREAKING**: the executable's prototype controls no longer provide unconstrained vertical/free-fly movement. Space performs a grounded jump and Left Control crouches instead of moving the camera vertically.

## Capabilities

### New Capabilities

- `physics-simulation`: Owns Jolt initialization and shutdown, single-threaded fixed-step simulation, collision filtering, the static prototype collision world, and backend containment.
- `player-controller`: Defines the one local grounded FPS player, including movement, gravity, collision, jump, crouch, air control, look orientation, and camera placement.

### Modified Capabilities

- `runtime-composition`: Adds explicit World/Physics lifetime and bounded fixed-step simulation coordination to the Engine-owned main-thread loop.
- `prototype-scene`: Makes the built-in visible environment and its static collision derive from one immutable prototype-level description and changes inspection from unconstrained flight to collision-constrained player movement.
- `free-fly-camera`: Removes the temporary unconstrained free-fly camera behavior now that the prototype has a physical first-person player.

## Impact

The change affects root dependency configuration, build targets, Engine subsystem composition, frame timing, camera code, prototype scene construction, deterministic tests, boundary checks, the executable's controls, and architecture/gameplay/development documentation. Jolt is linked privately by a concrete physics target; Jolt types do not enter public headers, the renderer, platform input, or gameplay-facing data. Public `near_laugh::Application` and `RuntimeConfig` APIs and the Vulkan frame contract remain unchanged.
