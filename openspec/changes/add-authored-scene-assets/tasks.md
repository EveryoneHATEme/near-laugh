## 1. Compose the P03 baseline and selected content

- [x] 1.1 Re-read the final applied P03 design/specs and compose this change against its version-5 codec, door definitions, editor history, collision and changing-presentation contracts; verify the combined delta review retains every P03 requirement/scenario and has one version-6 schema before editing shared implementation files.
- [x] 1.2 Prepare selected game derivatives of `props/chair.glb`, `table.glb`, `phone.glb` and `radio.glb` from `house_interior_pack`, with base-color-only normalized materials and phone MASK cutoff 0.5; verify mesh/UV preservation, supported metadata, embedded PNGs, nearest sampling and unchanged raw-source hashes.
- [x] 1.3 Extract the selected `geometry/floorWood.glb` and `wallWallpaper.glb` base colors into structural materials and record source paths/hashes, provenance and repeatable preparation instructions; verify only selected resources are staged and the raw pack is not a normal-build dependency.

## 2. World data, catalog and version-6 persistence

- [x] 2.1 Add the explicit logical model and structural-material catalog and ordered 0РІР‚вЂњ128 prop placements with 0РІР‚вЂњ8 local boxes each; verify ID syntax/uniqueness, known references, finite transforms, positive extents and empty placement/box behavior with world tests.
- [x] 2.2 Replace current solid surface fields with independent structural material IDs and add one material to present terrain; verify changing kind preserves material, cutout/model-only materials cannot be assigned to structural collision, and sculpting preserves assignments.
- [x] 2.3 Implement strict version-6 decoding and exact v2/3/4/5 compatibility, mapping the old chair/box and three surfaces to explicit legacy identities while preserving P03 doors; verify fixtures for every accepted version, later-field rejection, missing fields, removed values, unsupported v1 and unknown references.
- [x] 2.4 Update deterministic writing to version 6 and immutable runtime handoff; verify byte-identical repeated saves across locales, stable prop/box/door order, legacy load without writes, and preservation of all entry/default/switch/door values.
- [x] 2.5 Extend shared entry and initial-door validation over all transformed placement boxes; verify multi-floor entry support, clearance failures from repeated props, initial-door overlaps, finite derived bounds and zero-proxy non-blocking behavior without reading GLBs in world validation.

## 3. Selected asset resolution and controlled loading

- [x] 3.1 Resolve only the selected level's model/material dependencies under the explicit resource root, retaining legacy switch/door material aliases; verify another working directory, absent unselected model, missing selected resource and no raw-pack dependency with resource tests.
- [x] 3.2 Extend the existing private GLB loader with its one-root/one-primitive base-color material profile, explicit normalized shading inputs, supported samplers, constant-white case and legacy chair assignment; verify all four staged props plus legacy chair load and unsupported hierarchy, BLEND, texture references, extensions and material inputs fail with asset context.
- [x] 3.3 Decode embedded PNG base color with bounded dimensions/byte ranges and material factors/cutoffs; verify malformed images, buffer-view bounds, allocation-size overflow, non-finite values, OPAQUE alpha semantics and MASK alpha/factor data in importer tests.
- [x] 3.4 Reuse each decoded model/material within a scene load while expanding independent world placements; verify distinct transforms/normals, unchanged UVs, deterministic output, aggregate byte/draw-range checks and cleanup on partial failure.

## 4. Static collision and existing door integration

- [x] 4.1 Build the authored oriented static boxes from placement-local centers/extents and placement transforms; verify repeated chairs/tables, scaled/yawed proxies, empty boxes and partial construction cleanup in physics tests while the module never loads model geometry.
- [x] 4.2 Include all placement boxes in existing visibility queries and P03 swing obstruction without altering door ownership or action semantics; verify a prop blocks a switch/door target, a moving door stops against a prop, and decorative zero-proxy props do not become blockers.

## 5. Material rendering and resource lifetime

- [x] 5.1 Add immutable per-material sampled resources and factor/alpha uniforms while retaining the existing lighting/frame push-constant capacity; verify white fallback, nearest versus legacy linear sampling, full mip chains and cleanup/reuse through material/resource tests.
- [x] 5.2 Render generated structural materials and repeated static placements using simple material-bound draw ranges; verify independent floor/wall appearance, correct repeated transforms, finite normals and empty static streams without introducing per-frame prop mutation.
- [x] 5.3 Add MASK discard using sampled alpha times factor alpha before color/depth writes, preserving the current bounded diffuse lights; verify contrasting overlapping geometry, OPAQUE radio alpha, cutoff boundaries and point/spot enable combinations with image/smoke assertions appropriate to the actual rendering behavior.
- [x] 5.4 Keep P03 generated door/feedback presentation on its explicit opaque material and independent frame-slot resources; verify repeated door motion and lighting changes do not rebuild static geometry/materials or modify in-flight resources.
- [x] 5.5 Regenerate both packaged SPIR-V stages with the Vulkan 1.3 commands in `docs/DEVELOPMENT.md` and run `spirv-val` for both; verify shader interfaces and the existing device baseline remain valid.

## 6. Editor placements, materials and failure recovery

- [x] 6.1 Add catalog-model selection and independent prop add/rename/duplicate/remove operations to the flat editor set; verify deterministic unique IDs, count bounds, shared asset references, last-prop deletion and exact identity/selection restoration through undo/redo.
- [x] 6.2 Expose prop transforms, local box list and explicit reset-to-model-default boxes, preserving boxes on model change; verify finite-field refusal, cross-object invalid edits retained for repair, direct placement anchors, and one history entry per committed edit.
- [x] 6.3 Add independent structural material controls and a whole-terrain material property separate from height brushes; verify kind changes, terrain sculpt/undo, dirty-state restoration and P03 door fields are preserved by real UI/editor tests.
- [x] 6.4 Pick props by visible model bounds and show distinct render/proxy overlays, retaining selectable markers for unknown references; verify decorative zero-proxy selection, nearest object selection, malformed references and deletion/undo behavior.
- [x] 6.5 Share runtime import/material behavior in editor preview and transactional replacement; verify missing/corrupt asset and failed upload retain usable prior resources with explicit stale-preview feedback, while correction/undo restores coherent preview and safe invalid documents remain editable.
- [x] 6.6 Extend saved-file Play preflight to required selected assets before native process creation; verify missing/unsupported assets launch nothing, unselected missing models do not block Play, and save/reopen continues preserving prop/material/door identities.

## 7. Furnished scene and combined acceptance

- [x] 7.1 Update New Interior to an empty-prop/empty-door starter and migrate packaged prototype/acceptance levels to v6; verify the legacy prototype appearance/collision remains intact and starter Save/Play needs no unused model.
- [x] 7.2 Furnish the existing apartment/stairs acceptance scene with repeated chairs, table, phone and radio, wood floor and wallpaper while retaining P03's Lena door and blocked-switch exercise; verify both entry clearances, ordinary walking routes and useful chair/table proxies.
- [x] 7.3 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`; verify the affected world, codec, resource/import, physics, editor and runtime checks including the composed P03 regressions pass.
- [x] 7.4 Run `ctest --preset vulkan-smoke --output-on-failure` for game and editor with repeated placements, cutout, doors, resize/minimize/recovery, partial startup and failed replacement; verify no error-severity Vulkan validation messages and retained correct scene/door/light state after recovery.
- [ ] 7.5 Visually inspect the selected materials and phone cord at close and receding distances, verify depth holes against a contrasting surface, and walk the furnished route from both entries while exercising the door; record the observed result and any environmental validation limit.
- [ ] 7.6 Update `docs/ARCHITECTURE.md`, `docs/RENDERING.md`, `docs/GAMEPLAY.md`, `docs/DEVELOPMENT.md`, selected-asset preparation/provenance documentation and stale specification Purpose text during spec sync; verify they describe the final v6 profile and bounded materials without claiming broad glTF/PBR support.
- [x] 7.7 Review `git diff`, run `openspec validate add-authored-scene-assets --strict`, and review the composed P03/P02 requirements before syncing/archiving; verify no unrelated code, raw source pack, or unrequested asset-catalog/editor framework entered the change and report any unperformed acceptance check.
