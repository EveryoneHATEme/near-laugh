## 1. Backend-Neutral Spot-Light Contract

- [x] 1.1 Add the standard-layout source-independent spot-light frame, disabled representation, validity checks, and `FrameRequest` field; verify unit tests cover valid active data, disabled data, non-finite values, zero direction, invalid range/intensity/color, and incorrectly ordered cone limits.
- [x] 1.2 Replace target presentation masks in the scene push constant with the four aligned spot-light records and add size/offset assertions; verify the CPU layout test proves the camera-plus-spot payload is exactly 128 bytes and matches the shader contract.
- [x] 1.3 Update frame-decision and renderer-boundary tests to preserve arbitrary supplied spot-light data without player or flashlight types; verify a non-player-authored world-space pose passes unchanged through the backend-neutral frame path.

## 2. Player Flashlight and View Coordination

- [x] 2.1 Add the concrete player-flashlight state with fixed valid prototype parameters and armed press-edge handling; verify deterministic tests cover initial-off, press toggles, hold stability, release/re-press, inactive controls, recapture suppression, and release-after-recapture behavior.
- [x] 2.2 Expose one interpolated player view pose shared by camera construction and flashlight presentation, and remove recoil parameters from camera/aim APIs; verify player tests cover standing/crouched interpolation, movement interpolation, look direction, pitch limits, and camera/spot alignment.
- [x] 2.3 Replace Engine rifle sampling and fixed-step shooting coordination with immediate flashlight sampling, direct player fixed stepping, and generic spot-light frame composition; verify runtime/source-boundary tests cover normal polling, waited input batches, cursor transitions, iterations with zero or multiple fixed steps, and a flashlight toggle visible in the next frame request.

## 3. Vulkan Spot Lighting

- [x] 3.1 Update graphics-pipeline draw recording to push the source-independent spot-light payload while retaining the immutable point-light descriptor sets and one draw command; verify layout, descriptor-count, push-command, and single-draw boundary tests pass without per-frame descriptor updates.
- [x] 3.2 Add bounded distance, inner/outer cone, enabled-state, and Lambert spot-light evaluation to the fragment shader, remove target-mask presentation branches, and regenerate packaged SPIR-V; verify shader-source contract tests and shader module loading tests pass.
- [x] 3.3 Extend deterministic lighting tests with full-strength inner-cone, smooth transition, range/cone exclusion, back-facing surface, disabled light, and bounded accumulation cases; verify all lighting math cases pass independently of a player or camera source.
- [x] 3.4 Update Vulkan smoke coverage for changing and disabling spot-light push data across frames and forced swapchain recreation; verify immutable texture/point-light descriptors are still updated only at startup and Vulkan validation reports no errors.

## 4. Remove Shooting-Only Systems

- [x] 4.1 Remove prototype rifle, shooting-range coordinator, shooting-target state, their runtime ownership, focused tests, and build entries; verify no rifle, recoil, health, damage, highlight, or dimming symbols remain in runtime/render code and the project still configures.
- [x] 4.2 Remove the backend-neutral static-ray request/result and physics query implementation now that it has no consumer; verify physics tests retain static player collision coverage and no shooting ray API remains in headers or boundary checks.
- [x] 4.3 Remove target descriptions and starting-health metadata from the prototype level while retaining exactly three inert textured/collidable plate solids; verify world, scene, texturing, and physics tests confirm their stable geometry, texture role, and matching collision.

## 5. Documentation and Validation

- [x] 5.1 Update `docs/ARCHITECTURE.md`, `docs/GAMEPLAY.md`, `docs/RENDERING.md`, and `docs/DEVELOPMENT.md` for flashlight controls, generic one-slot spot-light presentation, shooting removal, inert plates, push-constant layout, and shadow limitation; verify documentation contains no claim that rifle/target gameplay or presentation masks remain implemented.
- [x] 5.2 Configure and build the debug preset, then run `ctest --preset debug --output-on-failure`; verify configuration, compilation, boundary checks, and all deterministic affected tests succeed.
- [x] 5.3 Run `ctest --preset vulkan-smoke --output-on-failure` and interactively inspect toggle edges, recapture suppression, camera alignment, cone falloff, retained point lights, inert plate appearance, swapchain recovery, and validation output; verify there are no error-severity Vulkan messages and report any unavailable interactive check.
- [x] 5.4 Review `git diff` for scope, ownership, generated shader assets, and accidental unrelated changes; verify the final diff implements every task without a light registry, multiple dynamic lights, shadow system, or unrelated refactor.
