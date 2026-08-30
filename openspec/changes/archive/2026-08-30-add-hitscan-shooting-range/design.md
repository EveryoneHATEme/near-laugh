## Context

See `proposal.md` for motivation. The runtime currently samples one FPS action snapshot per event batch, advances one collision-constrained player in fixed 1/60-second steps, interpolates position for rendering, and sends a backend-neutral camera matrix in each frame request. The physics world contains only Jolt static box bodies plus one virtual character. The renderer uploads one immutable non-indexed vertex stream for every prototype solid and draws it once using a 96-byte camera-and-light push constant.

The existing primary action is already mapped to the left mouse button and is consumed for cursor recapture while first-person controls are inactive. The renderer must remain unaware of weapon, health, damage, and physics types, and the change must not turn the immutable prototype level into a general entity or asset model.

## Goals / Non-Goals

**Goals:**

- Establish an explicit input-to-weapon-to-physics-to-target-to-presentation path on the main thread.
- Keep firing cadence, damage, recoil recovery, and hit feedback deterministic under the existing fixed-step accumulator.
- Preserve one authoritative immutable geometry description and stable mapping among authored targets, physics hits, gameplay state, and rendered surfaces.
- Preserve the renderer's single immutable scene buffer, one opaque draw, descriptor-free pipeline, and guaranteed push-constant limit.
- Make each gameplay and backend boundary testable without starting a window or Vulkan.

**Non-Goals:**

- A reusable weapon hierarchy, damage interface, entity identifier, component model, or data-driven gameplay definition.
- Physical destruction, target movement, ray penetration, surface materials, decals, particles, muzzle flash, view models, crosshairs, or audio.
- Variable-rate weapon simulation, rollback, saved target state, or a pause/menu framework.

## Decisions

### Own two concrete gameplay values in the runtime

Add one concrete prototype-rifle value and one concrete shooting-target collection to Engine composition. The rifle owns trigger latching, its fire timer, and recoil offset. The target collection owns exactly three health values and three highlight timers. Neither owns or inherits from a generic weapon, actor, health, damageable, or entity interface.

The immutable level will describe three targets through stable entries that point at distinct indices in its existing solid collection. Each referenced solid is explicitly marked as a shooting-target kind. Gameplay state copies only the stable association and configured starting health; mutable health and timers never enter `PrototypeLevel`.

Putting health on `PrototypeSolid` was rejected because the level is shared immutable structural data. A generic entity registry was rejected because three fixed plates do not justify identity, lifetime, or component machinery.

### Latch trigger input and advance the rifle only in fixed steps

Rifle input sampling will receive the same `controls_active` result as the player. A newly observed active transition sets a pending-press latch even if the current render iteration produces no simulation step. While active input remains held, the rifle automatically repeats at a fixed interval. Cursor release or recapture clears weapon input and pending state, ensuring the recapture click cannot fire.

Each fixed step first advances player movement and rifle recoil recovery. If the rifle is ready and has held or pending input, it emits at most one shot, starts its fire interval, clears the pending edge, and adds recoil after capturing the shot direction. The fixed interval will be longer than one 1/60-second step, so a single-step one-shot result is sufficient and avoids a collection of shots per step. Releasing and pressing during cooldown preserves the cooldown.

Driving cooldown from render-loop time was rejected because shot count would vary with frame rate. Consuming only the current held state was rejected because a press can occur during an iteration with no complete fixed step.

### Build shots from authoritative simulated pose and separate recoil from base look

The current, non-interpolated player foot position and stance eye height form the shot origin. Current yaw and base pitch plus the rifle's pre-shot recoil offset form a normalized forward direction. A fixed maximum distance completes the engine-owned ray value. The player presentation uses the same effective pitch, while only positional movement remains interpolated.

The rifle stores a bounded positive pitch offset. Each shot adds a fixed kick and each later fixed step moves it toward zero at a fixed recovery rate. Base mouse-look yaw and pitch remain player-owned, so recovery cannot rewrite user input. Effective pitch is clamped through the existing camera pitch limit. A shot is constructed before its new kick is applied, making its direction stable and making recoil affect subsequent aim.

Using the interpolated render pose for gameplay was rejected because it depends on render timing. Mutating the player's base pitch for recoil was rejected because recovery would need to distinguish user input from weapon motion.

### Expose one closest-static-hit physics query

The physics module will expose an engine-owned ray input and an optional hit containing only the prototype solid index and distance. It validates finite input, normalizes the non-zero direction, and queries only the non-moving collision layer without stepping the physics system. Static bodies retain their originating prototype-solid index as backend-private user data, allowing the Jolt hit to map directly back to immutable world data.

The Engine resolves the returned solid index against the three target descriptions. Non-target hits and misses end processing without damage. Destroyed plates remain static bodies and therefore continue to stop closest-hit rays.

Returning Jolt body identifiers was rejected because it leaks the backend. Reimplementing ray-versus-box tests over level data was rejected because gameplay should query the same collision representation that constrains the player.

### Translate target state into gameplay-free solid masks

Before rendering, the runtime converts active target highlight timers and destroyed states into a `PrototypeScenePresentation` with two 32-bit solid masks: highlighted and dimmed. No health, damage, target, rifle, or physics type crosses the frame boundary. The built-in prototype is validated to fit within the mask width; the three target descriptions refer to maskable solid indices.

Each generated vertex gains its source solid's one-bit mask. The scene push constant gains one aligned 16-byte lane containing highlighted and dimmed masks plus padding, growing from 96 to 112 bytes and remaining below Vulkan's guaranteed 128-byte minimum. The vertex stage forwards the mask as a flat unsigned value. The fragment stage applies the highlight when its bit is present, otherwise applies dimming, then retains the existing opaque depth and directional/ambient lighting behavior. This preserves the immutable vertex buffer, one pipeline, one draw, and descriptor-free path.

Re-uploading or mapping vertex data after hits was rejected because it introduces GPU-write lifetime concerns for tiny per-frame state. A target-specific renderer API was rejected because rendering must not interpret health or gameplay concepts. A uniform buffer or descriptor set was rejected because the two masks fit in the remaining guaranteed push-constant space.

### Keep target timing and final-hit precedence deterministic

Target highlight timers advance once per fixed step. A damaging hit resets that target's timer to the fixed highlight duration. The hit that reaches zero health still starts the highlight; while both masks contain that solid, highlight wins. When the timer expires, a destroyed target remains only in the dimmed mask. Hits on an already destroyed target do not refresh feedback or alter another target.

Wall-clock timers were rejected because they would elapse during minimized-window waits and make deterministic tests depend on scheduling.

## Risks / Trade-offs

- [A solid-mask shift overflows or CPU/GPU masks disagree] -> Validate the built-in solid count and target indices against 32 bits, use fixed-width fields, assert the C++ layout, and test vertex attributes and shader-facing offsets.
- [Jolt body user data maps to the wrong authored solid] -> Assign indices during the existing deterministic body-creation loop and test representative closest hits against known level geometry and all three targets.
- [A quick trigger press is lost or a recapture click fires] -> Unit-test no-step latching, release/repress cooldown, inactive controls, and capture transitions independently of GLFW.
- [Recoil aim and rendered pitch diverge] -> Derive both from the same base look plus rifle offset and test shot directions and camera matrices at zero, partial, and maximum recoil.
- [The 32-solid presentation mask becomes a premature scene limit] -> Treat it as a validated built-in-prototype constraint, not a public renderer architecture; replace it only when a concrete authored level exceeds it.
- [Targets remain physically present after destruction] -> Make this deliberate shooting-plate behavior visible through dimming; physical destruction remains a separate future feature.

## Migration Plan

Add target descriptions, physics hit mapping, gameplay state, frame presentation, vertex/push layouts, GLSL changes, and rebuilt SPIR-V assets as one atomic change so old and new CPU/GPU layouts cannot mix. No public application API, external level data, or saved state requires migration. Rollback consists of reverting the change together; no persistent data is modified.
