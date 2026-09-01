## 1. Prototype World and Presentation Contracts

- [x] 1.1 Add three visually separated target-plate solids and stable target descriptions to the immutable prototype level, validate their unique maskable solid indices and positive shared health, and verify `test_world` covers valid geometry, mappings, spawn clearance, and invalid target descriptions.
- [x] 1.2 Add the backend-neutral highlighted-solid and dimmed-solid presentation masks to frame requests without gameplay types, and verify frame/loop unit tests preserve masks through render decisions while public-header boundary tests remain clean.

## 2. Static Physics Ray Queries

- [x] 2.1 Add engine-owned static-ray input and optional closest-hit result types to the physics boundary, retain each created body's prototype-solid index privately in Jolt, and verify source/header boundary tests find no leaked Jolt query or body types.
- [x] 2.2 Implement validated closest-static-hit queries over the non-moving collision layer without advancing simulation, and verify headless physics tests cover invalid input, misses, nearest-of-multiple hits, environment hits, all three target indices, and unchanged character state.

## 3. Concrete Shooting Gameplay

- [x] 3.1 Implement the single fixed-step prototype rifle with inactive-control clearing, no-step trigger latching, held automatic fire, cooldown preservation, unlimited ammunition, bounded recoil, and recovery; verify focused unit tests cover press/hold/release, recapture suppression, cadence, shot-before-kick direction, accumulation, clamping, and recovery.
- [x] 3.2 Implement exactly three gameplay-owned target states with fixed health, solid-index hit mapping, non-negative damage, destroyed-state behavior, fixed-step highlight timers, and presentation-mask conversion; verify focused unit tests cover environment hits, independent targets, repeated/final hits, destroyed-target hits, refreshed/expired highlights, and highlight-over-dim precedence.
- [x] 3.3 Expose an engine-owned current player eye/aim calculation that combines the non-interpolated simulated stance pose, base look, and rifle recoil while preserving pitch limits, and verify player-controller tests cover standing/crouched origins and zero, partial, and maximum recoil directions and camera frames.

## 4. Runtime Coordination

- [x] 4.1 Add the rifle and shooting targets to explicit Engine composition and feed both from the existing cursor/control-active decision, verifying ownership/source tests preserve the required `Platform -> Window -> PrototypeLevel -> PhysicsWorld -> PlayerController -> gameplay -> Renderer` lifetime boundaries.
- [x] 4.2 Coordinate recoil recovery, shot emission, physics hit resolution, target damage/feedback, and frame-presentation conversion in each complete fixed step, and verify deterministic runtime tests cover zero-step input retention, one-shot steps, multiple ordered steps, misses/environment hits, inactive controls, and one final presentation per render iteration.

## 5. Single-Draw Vulkan Presentation

- [x] 5.1 Add each generated vertex's one-bit source-solid mask and the aligned presentation-mask lane to the scene push constant, keeping the total at 112 bytes or less and the immutable one-buffer contract; verify vertex-generation and CPU/GPU layout tests cover every solid, attribute format/offsets, mask bounds, standard layout, and push-constant range.
- [x] 5.2 Update the prototype vertex/fragment shaders so flat solid masks select highlight before dimming while unaffected surfaces retain directional/ambient lighting, rebuild the committed SPIR-V resources, and verify shader/resource tests and the debug build accept the matching GLSL, SPIR-V, vertex, and push layouts.
- [x] 5.3 Pass frame presentation through renderer recording and the existing opaque draw without descriptors, buffer rewrites, extra draws, or lifetime-resource recreation, and verify renderer source checks plus the Vulkan smoke test exercise normal, highlighted, dimmed, and highlight-plus-dim frame requests across forced swapchain recreation.

## 6. Documentation and Full Validation

- [x] 6.1 Update architecture, gameplay, rendering, development, and README descriptions for shooting controls, fixed target behavior, ray-query ownership, recoil, and visual feedback, and verify the documentation consistently states the excluded weapon/enemy/rendering features.
- [x] 6.2 Configure and build the debug preset and run `ctest --preset debug --output-on-failure`; verify compiler selection remains Clang and every deterministic and boundary test passes.
- [x] 6.3 Run `ctest --preset vulkan-smoke --output-on-failure` and the FPS executable where a presentation-capable desktop is available; verify no error-severity Vulkan validation messages, target highlight/destroyed appearance is clear, recoil is bounded and recovers, recapture does not fire, and held fire respects its cadence, reporting any unavailable manual validation.
- [x] 6.4 Review `git diff` for scope, ownership, CPU/GPU layout agreement, regenerated binary assets, and unrelated edits, then verify `openspec validate add-hitscan-shooting-range --strict` succeeds before reporting implementation complete.
