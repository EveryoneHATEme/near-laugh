## 1. Prototype Lighting Model

- [x] 1.1 Replace the directional environment-light value with exactly two point-light records plus a near-black ambient scalar, add finite/range validation, and verify focused world tests accept valid lights and reject non-finite, negative, zero-intensity, and zero-radius cases.
- [x] 1.2 Author the cool spawn light and warm destination light with non-overlapping dark-route coverage, and verify deterministic level tests establish the fixed count, valid values, camera-independent positions, and at least one intended route sample outside both radii.

## 2. Immutable GPU Lighting Resources

- [x] 2.1 Add the renderer-private two-light `std140` upload layout and RAII uniform-buffer/descriptor owner, register its source with the build, and verify compile-time plus deterministic tests cover alignment, offsets, descriptor configuration, mapped upload values, and partial-construction cleanup.
- [x] 2.2 Integrate the lighting owner into renderer construction and destruction beside the sampled texture so it survives swapchain recreation, and verify lifecycle tests prove pipelines are destroyed before lighting resources, lighting resources before the Vulkan device, and injected failures release each created resource exactly once.

## 3. Pipeline and Shader Integration

- [x] 3.1 Add the lighting descriptor as set 1, bind it with the existing set-0 texture descriptor, shrink the push constant to camera plus presentation masks, and verify pipeline/layout tests cover descriptor-set order, the 80-byte push-constant contract, and removal of per-frame environment-light arguments.
- [x] 3.2 Pass the existing world-space vertex position to the fragment stage and implement the two-light radius-bounded Lambert accumulation over the near-black ambient floor, then rebuild committed SPIR-V and verify shader-interface checks cover matching locations, the fixed two-light loop, finite normalization, exact zero-radius-edge falloff, bounded RGB output, and unchanged opaque alpha.
- [x] 3.3 Preserve texture/tint composition, highlight-before-dim precedence, depth testing, and one scene draw under local illumination, and verify deterministic renderer checks plus the Vulkan smoke fixture detect extra draws, per-frame lighting descriptor updates, or lighting-resource recreation during forced swapchain recovery.

## 4. Documentation and Validation

- [x] 4.1 Update README and the architecture, rendering, gameplay, and development documentation to describe the two immutable local lights, intentional darkness, lighting ownership, controls, and milestone exclusions, and verify repository searches no longer describe the implemented prototype as using directional-plus-ambient lighting.
- [x] 4.2 Configure the standard debug preset with Clang, build it, and run `ctest --preset debug --output-on-failure`; verify configuration, compilation, shader generation, and every deterministic test succeed.
- [x] 4.3 Run `ctest --preset vulkan-smoke --output-on-failure` and visually inspect the packaged FPS route for a dim cool spawn pool, warmer destination pool, intervening darkness, bounded leakage, textured surface response, and readable target highlight/dim states; verify Vulkan validation reports no errors or document any unavailable GPU/display validation.

  Validation note: all three Vulkan smoke fixtures passed on the available presentation-capable device with no validation errors. Subjective visual inspection of the transient native window is unavailable through this API session; the smoke rendered the packaged route and exercised live, highlighted, dimmed, and highlight-over-dim target states.
- [x] 4.4 Review `git diff` for scope, ownership, CPU/GPU layout consistency, generated SPIR-V, and accidental general lighting machinery, then verify `openspec validate add-local-horror-lighting --strict` succeeds before reporting implementation complete.
