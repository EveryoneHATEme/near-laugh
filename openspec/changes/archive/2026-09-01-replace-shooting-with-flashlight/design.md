## Context

The runtime currently routes the held primary action through an automatic rifle, advances player/rifle/target state together at 60 Hz, and sends target highlight/dim masks with each frame. The renderer keeps two level-authored point lights in an immutable startup-only uniform buffer; its per-draw push constant is 80 bytes and carries a 64-byte camera matrix plus 16 bytes of target masks. The fragment shader performs one textured opaque draw with Lambert point-light shading.

The new behavior crosses input, player presentation, runtime composition, the runtime-render boundary, shader data, and removal of shooting-only physics/gameplay code. The renderer must remain independent of gameplay concepts, and Vulkan guarantees only 128 bytes of push-constant storage.

## Goals / Non-Goals

**Goals:**

- Establish one small, backend-neutral `SpotLight` frame value whose pose and parameters are independent of the object that produced it.
- Use the player flashlight as the first producer while preserving a clean path for a later concrete object to provide the same value.
- Align the flashlight with the interpolated rendered camera rather than the non-interpolated simulation eye.
- Keep dynamic lighting updates in the existing single draw without mutable per-frame descriptors.
- Remove shooting-only runtime, physics-query, target-state, recoil, and presentation machinery completely.

**Non-Goals:**

- A light registry, entity component, scene hierarchy, generalized lighting manager, or arbitrary number of lights.
- More than one active dynamic spot light in a frame.
- Directional/sun lighting, shadow mapping, volumetric scattering, a visible emitter model, batteries, flicker, audio, or authoring files.
- Removing or redesigning the three existing plate solids or their fixed texture role.

## Decisions

### 1. Separate the generic spot-light frame from player flashlight state

Define a standard-layout, backend-neutral spot-light frame alongside the existing frame request. It contains only world-space position, normalized direction, range, inner and outer cone cosines, color, intensity, and enabled state. Its name and validation rules do not mention the player or flashlight. `FrameRequest` carries at most one such value.

A concrete player-flashlight runtime value owns only toggle/arming state and fixed flashlight parameters. It combines those parameters with a player view pose to produce the generic spot-light frame. A future door, enemy, or scripted game object can produce the same frame value without changing the renderer, although simultaneous multiple spot lights require a separate future requirement.

This creates a meaningful runtime-render boundary without introducing a registry or base-class hierarchy. A camera-specific flashlight field was rejected because it would couple the renderer contract to the current producer. A multi-light array was rejected because no current behavior needs concurrency and it would force more per-frame GPU storage and synchronization.

### 2. Toggle from primary-action edges during input sampling

The flashlight samples the raw primary-action level together with the existing `controls_active` decision once per processed input batch. It starts armed and disabled. A down sample consumes the arm; it toggles only when controls are active. Any down sample while controls are inactive, including cursor recapture, consumes the arm without toggling. A release rearms it.

This preserves recapture suppression even when the button remains held after capture and makes the flashlight respond in the next render request without waiting for a fixed simulation step. Edge state belongs to the flashlight rather than the input mapper because the mapper intentionally reports physical action levels, not gameplay semantics.

### 3. Produce camera and light from one interpolated player view pose

Player presentation exposes one internal view pose containing the interpolated eye position and the latest yaw/pitch direction. Camera-matrix construction and flashlight presentation consume that same pose. The fixed-step loop advances the player directly after shooting-range coordination is removed.

This avoids visible separation between the beam and the rendered view during movement or stance interpolation. Reusing the existing non-interpolated shooting aim was rejected because it would make the light origin jitter relative to the camera. Reconstructing a light pose from the view-projection matrix in rendering was rejected because it is indirect, projection-dependent, and violates the supplied-pose contract.

### 4. Replace target masks with a 128-byte camera-plus-spot push constant

Remove the 16-byte presentation-mask member and represent the spot light as four aligned `vec4` records:

```text
camera matrix                 64 bytes
position.xyz + range         16 bytes
direction.xyz + inner cosine 16 bytes
color.rgb + intensity        16 bytes
outer cosine + enabled/pad   16 bytes
                             --------
total                        128 bytes
```

The engine validates finite values, a non-zero normalized direction, positive active range/intensity, non-negative color, and ordered cone cosines before submission. Disabled frames use a canonical zero-contribution representation. The existing immutable point-light uniform and descriptor remain unchanged.

Push constants are recorded with each draw after its frame slot is safe to reuse, so they need no descriptor allocation, update, mapping, or additional frame-in-flight ownership. A mutable uniform buffer was rejected because the data fits the Vulkan minimum push-constant capacity exactly and the current pipeline already records one push per frame.

### 5. Add a finite spotlight term to existing Lambert shading

For an enabled spot light, the fragment shader computes the vector from the light to the fragment for cone membership and the opposite direction for the surface Lambert term. It applies the same smooth bounded distance falloff style as point lights and a smooth interpolation from full strength at the inner cone to zero at the outer cone. The result is accumulated with ambient and the two immutable point lights, then clamped as today.

The shader uses the supplied pose without inspecting camera data. This is what allows another object to use the light type. A true directional light was rejected because its infinite, positionless influence cannot represent a flashlight cone.

### 6. Remove shooting behavior while retaining inert plate scenery

Remove the rifle, shooting-range coordinator, mutable target states, recoil camera offset, static-ray physics boundary, and target presentation masks. The three plate solids remain ordinary immutable textured/collidable level geometry. Their gameplay descriptions and starting-health metadata are removed; validation and tests count or inspect their authored solids directly where the scene contract still requires three plates.

Keeping the plates avoids expanding this gameplay change into scene layout and texture-packaging work. Retaining dormant target gameplay or a now-unused ray API was rejected because it would preserve unsupported behavior and maintenance cost.

## Risks / Trade-offs

- [No shadow map allows spot-light illumination to ignore occluding geometry] -> Document this limitation, keep influence bounded by range and cone, and require a separate measured shadow-mapping change if leakage harms the intended scene.
- [The push constant consumes the full 128-byte Vulkan guaranteed minimum] -> Add CPU layout assertions, shader-contract tests, and a device-limit startup check already compatible with the current Vulkan baseline; do not add more push data without redesigning the frame-data path.
- [A malformed direction or cone can produce unstable shader math] -> Validate the generic spot-light frame on the CPU and cover invalid/non-finite/incorrectly ordered inputs with deterministic tests.
- [One dynamic slot cannot represent flashlight plus another object light concurrently] -> Keep the frame type source-independent, but defer multi-light storage until concurrent lights are an actual game requirement.
- [Immediate input-batch toggling differs from fixed-step shooting timing] -> Test press, hold, release, minimized waiting, cursor release, and cursor-recapture sequences independently of frame rate.
- [Removing the static ray query may require reintroduction for later interaction mechanics] -> Reintroduce a concrete query only when a new gameplay requirement identifies its semantics; do not retain the shooting-specific API speculatively.

## Migration Plan

1. Add and test the generic spot-light frame contract and concrete player-flashlight toggle state while the old shooting code still compiles.
2. Expose a shared interpolated player view pose, supply the generic spot light in frame requests, and update the push-constant/shader contract and packaged SPIR-V.
3. Replace the fixed-step shooting coordinator with direct player stepping, then remove rifle, target-state, recoil, target-mask, and static-ray code and build entries.
4. Update deterministic boundary/layout/shader/world tests, Vulkan lifecycle and rendering smoke coverage, and project documentation.
5. Configure, build, run the deterministic suite, run the Vulkan smoke preset with validation, inspect the flashlight cone interactively, and review the final diff.

Rollback is a source-level revert of this change. No persisted saves, external assets, or data migrations are introduced.
