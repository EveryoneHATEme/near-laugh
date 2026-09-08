## 1. Prepared character asset

- [x] 1.1 Verify the pinned local GLB/hash and archive provenance from asset-inspection.md; retain a versioned preparation manifest and included license with an explicit source/derivative distinction.
- [x] 1.2 Implement deterministic mannequin preparation with the three selected clips and normalized diffuse materials; verify two runs are byte-identical, raw sources are unchanged and excluded features are absent.
- [x] 1.3 Calibrate feet/forward/proxy/preview bounds, walk cycle distance, contact phases and interaction phase from sampled poses; retain values and visual evidence, including the supported speed/contact-spacing check needed by P07b.
- [x] 1.4 Package the derivative and catalog metadata independently of build/p07-assets; verify a resource-layout run from another working directory succeeds without local sources.

## 2. Bounded CPU animation

- [x] 2.1 Add the shared CPU animation target with project-owned interfaces and one compiled cgltf implementation shared with static loading; verify game/viewer/editor linkage and existing dependency-boundary checks.
- [x] 2.2 Implement bounded animated decoding and preflight validation; verify valid mannequin loading, allowed absent channels using rest values, and contextual failures for malformed hierarchy, indices, weights, inverse binds, key times, missing required attributes and size/overflow violations.
- [x] 2.3 Implement local TR sampling, global pose evaluation and bind-preserving skin deformation; verify a known tiny two-joint fixture, actual bind geometry, normals and nonzero world placement/yaw.
- [x] 2.4 Implement loop/clamp, one-shot completion, pause/seek and 0.15 s interrupted transitions; verify pose equivalence across time batching, loop edges and interrupted blends without duplicate completion.

## 3. Rendering and ownership

- [x] 3.1 Add selected render instances and the bounded borrowed pose input; verify zero/four instances and rejection of missing, duplicate, non-finite or skeleton-mismatched poses before submission.
- [x] 3.2 Add per-slot character deformation/upload and per-primitive material ranges while preserving static/door resources; verify independent instances and correct colors/normals with the existing pipelines.
- [x] 3.3 Include the same evaluated character geometry in color and every shadow face; verify posed-arm and off-screen shadow readbacks plus unchanged static/door/flashlight control views.
- [x] 3.4 Implement partial-construction cleanup, fence-protected reuse and transactional character resource replacement; verify injected allocation failures, empty character sets and repeated destruction/recreation.
- [x] 3.5 Preserve pose/resources through resize and swapchain/attachment-format recovery; verify animated Vulkan smoke retains coherent color/shadow state and validates final teardown.

## 4. Independent viewer and acceptance

- [x] 4.1 Add the explicit character_animation_viewer entry with clip, time, restart, pause and one/four-instance controls; verify ordinary filenames trigger no viewer sequence and controls do not generate gameplay/audio events.
- [x] 4.2 Inspect idle/walk/interact, loop/blend boundaries, scale, normals and shadow silhouettes; retain comparison captures and actual observations in validation.md.
- [x] 4.3 Measure light-only, one-character and four-character Release setups using the design's warm-up/sample conditions; retain raw CPU/deformation/upload/GPU/frame timing data and results against the stated targets.
- [x] 4.4 Follow docs/DEVELOPMENT.md: configure debug, build affected game/editor/viewer targets, run affected deterministic tests and Vulkan smoke, and regenerate/validate any changed shaders; record commands/results and unavailable checks.
- [x] 4.5 Update implemented architecture/rendering/development and asset workflow documentation, review git diff and run strict OpenSpec validation; verify docs describe the delivered profile and do not claim full T2.
- [x] 4.6 Prepare the accepted P07a handoff record for the subsequent archive workflow; verify P07b's dependencies match the delivered character-animation and shadow contracts and any measurement limits are explicit.

## 5. Authorized indexed presentation follow-up

- [x] 5.1 Preserve one deformed vertex per source vertex and shared immutable per-asset indices with checked per-instance draw offsets; verify triangle attributes/material equivalence, reordered requests, four shared instances, distinct assets and invalid/empty ranges.
- [x] 5.2 Own and initialize the immutable Vulkan index buffer and use indexed character draws in color and every shadow face; verify partial index allocation/initialization failures, editor retention, fenced slots, empty selection, recovery and final teardown.
- [x] 5.3 Reconfigure/build affected Debug and Release targets, run deterministic and full Vulkan checks, and retain indexed readbacks with comparison to the expanded baseline and actual visual observations.
- [x] 5.4 Repeat all nine Release 0/1/4-character samples with unchanged scene, camera, timing path and gates; retain raw data, hashes and before/after results alongside the original failures, and update documentation/diff/strict OpenSpec review before deciding task 4.6.
