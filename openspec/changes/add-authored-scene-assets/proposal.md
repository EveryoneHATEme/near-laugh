## Why

The story needs a recognizable furnished apartment, including a telephone,
radio, kitchen props, doors, and later visible people. The current loader
accepts one packaged chair primitive and ignores its materials, so it cannot
support that content-production workflow.

## What Changes

- Replace the single fixed prop with explicit placements of a bounded set of
  packaged model and material assets, referenced by stable logical identities.
  Keep resource locations under the explicit package root.
- Establish a controlled static export profile from the supplied
  `house_interior_pack` chair, table, phone, and radio. Prepare selected game
  derivatives with embedded base-color textures, explicit opaque or alpha-cutout
  materials, and the source assets' nearest texture sampling. The phone's
  textured cord needs cutout coverage; blended transparency is not required.
- Author each placement's transform and simple collision proxies separately
  from render geometry. Multiple placements can reference one asset without
  forcing physics to parse render models.
- Add asset/placement selection, duplication, deletion, property editing,
  validation, undo/redo, and faithful preview to the editor. Define reference
  behavior on deletion and preserve identities through save/reopen.
- Assign packaged structural materials independently of collision kind,
  using selected wood-floor and wallpaper textures as concrete examples.
  Preserve Vulkan ownership, resource recovery, and startup error cleanup.
- **BREAKING:** Write level version 6 after the selected P03/version-5
  integration baseline. Replace the singleton prop with zero through 128
  identified placements and fixed surface roles with material identities.
  Read exact versions 2 through 5, preserve their appearance and collision,
  and retain P03 doors without rewriting source files on load.
- Animated characters belong to P07, lighting/shadows to P10. No general asset
  discovery service, arbitrary glTF support, streaming, material graph, PBR,
  or blending/sorting pipeline. Only selected derivatives are packaged; the
  supplied raw pack remains source material.

## Capabilities

### New Capabilities

- `authored-scene-assets`: Packaged asset identities, supported export profile,
  repeated model placements, opaque/cutout materials, and collision proxies.

### Modified Capabilities

- `static-model-loading`: Replace the single-chair primitive profile with the
  controlled static game-asset profile and actionable failures.
- `prototype-scene`: Allow multiple explicitly placed static props.
- `scene-texturing`: Replace fixed prototype-only texture/surface appearance
  with supported authored material assignments.
- `scene-lighting`: Preserve defined lighting behavior on the supported
  authored geometry/materials without requiring fixed prototype resources.
- `level-persistence`: Persist placements, asset references, and proxies.
- `level-object-placement`: Author/select independent prop placements.
- `level-editor`: Present asset-reference and import failures.
- `physics-simulation`: Build the authored static placement proxies.
- `runtime-composition`: Resolve required assets from the selected scene.
- `vulkan-renderer`: Own and render the supported scene assets and materials.
- `interior-level-authoring`: Create interiors without a mandatory chair and
  furnish the existing acceptance route while preserving P03 doors.
- `terrain-authoring`: Preserve authored material assignments during sculpting
  instead of requiring the former three-role palette.

## Impact

Affects asset packaging, private model/texture loading, world validation,
static collision construction, renderer resources, and editor preview and
commands. Document export instructions and the supported material profile.
Do not choose a larger rendering architecture merely to support more props.

## Dependencies and Boundaries

P02; its feature prerequisite is
[P01](../archive/2026-09-06-add-interior-level-authoring/proposal.md).
Planning and isolated importer/material work can run alongside P03. The chosen
integration order is P03 then P02, so the shared codec, editor, physics, and
renderer deltas compose against P03's final version-5 contracts before apply.
This ordering does not make doors an intrinsic asset dependency. Static assets
remain immutable; P03 keeps generated door geometry and its independent moving
collision/presentation path.

## Acceptance Criteria

- Prepare and load the selected chair, table, phone, and radio; author repeated
  furniture placements with distinct transforms, correct base colors and
  sampling, and the phone cord's cutout silhouette. Inspect wood-floor and
  wallpaper assignments on structural solids.
- Rendering and authored proxies agree sufficiently for the intended traversal
  and targeting; a render asset is never loaded by physics.
- Save/reopen and undo/redo preserve references. Missing assets and unsupported
  exports produce actionable errors without destroying the active editor scene.
- Run codec/import/proxy tests and game/editor Vulkan smoke; visually inspect
  the representative exports and recovery behavior.
