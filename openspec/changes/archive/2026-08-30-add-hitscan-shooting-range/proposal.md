## Why

The current prototype validates first-person movement, static collision, and lit scene rendering, but it does not yet exercise the defining interaction of an FPS: aiming and shooting at something that reacts. A small built-in shooting range adds the first complete gameplay loop while the world and renderer are still simple enough to keep its boundaries explicit.

## What Changes

- Add one fixed automatic hitscan prototype rifle driven by the existing primary action, with a deterministic fire interval, unlimited ammunition, and bounded camera recoil.
- Add a backend-neutral physics ray query that reports the first static prototype solid hit without exposing Jolt types.
- Add three explicitly authored target plates to the built-in prototype level and track their fixed health and destroyed state in concrete shooting-range gameplay state.
- Give successful target hits immediate visible feedback through a brief hit tint and leave destroyed targets visibly disabled.
- Pass a small backend-neutral target-presentation snapshot with each frame while preserving the current immutable scene geometry, single opaque scene draw, and descriptor-free rendering path.
- Add deterministic coverage for firing cadence, queued trigger input, ray-hit mapping, target damage, feedback timing, recoil, and CPU/GPU presentation layouts.
- Keep ammunition, reloads, weapon switching, projectiles, enemies, AI, dynamic rigid bodies, generic damage/entity systems, crosshairs, weapon models, audio, and particle effects outside this change.

## Capabilities

### New Capabilities

- `hitscan-weapon`: Defines the single prototype rifle's trigger handling, fixed-step firing cadence, authoritative shot ray, and recoil behavior.
- `shooting-targets`: Defines the built-in target plates, their fixed health and hit handling, and their visible hit/destroyed feedback.

### Modified Capabilities

- `prototype-scene`: Extends the deterministic built-in level with fixed shooting-range target plates derived into matching render and static-collision geometry.
- `physics-simulation`: Adds a backend-neutral closest-hit ray query over the prototype's static collision bodies.
- `runtime-composition`: Integrates weapon and target state into the application-owned input, fixed-step simulation, and frame-presentation sequence.
- `vulkan-renderer`: Accepts per-frame target presentation and applies it to authored target surfaces without changing renderer ownership or the existing opaque scene path.

## Impact

- Affects the built-in level description, Jolt-backed physics boundary, runtime composition, player look/recoil policy, frame request data, generated prototype vertices, graphics push-constant layout, scene shaders, and deterministic/Vulkan smoke coverage.
- Adds concrete gameplay-owned weapon and target state; Jolt and Vulkan types remain confined to their existing modules.
- Preserves the current single-threaded fixed-step runtime, static physics world, immutable geometry upload, one opaque draw, Dynamic Rendering, Synchronization 2, and executable-relative shader resources.
- Adds no third-party dependency and introduces no ECS, generic weapon/item hierarchy, generic damage framework, render graph, descriptor system, or asset pipeline.
