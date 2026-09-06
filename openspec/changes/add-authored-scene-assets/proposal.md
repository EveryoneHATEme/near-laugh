## Why

The story needs a recognizable furnished apartment, including a telephone,
radio, kitchen props, doors, and later visible people. The current loader
accepts one packaged chair primitive and ignores its materials, so it cannot
support that content-production workflow.

## What Changes

- Replace the single fixed prop with explicit placements of a bounded set of
  packaged model and material assets, referenced by stable logical identities.
  Keep resource locations under the explicit package root.
- Establish a documented, controlled static export profile using representative
  apartment assets before designing import details. Support the primitives,
  transforms, texture assignments, and opaque material properties those
  exports actually need; report unsupported content with asset context.
- Author each placement's transform and simple collision proxies separately
  from render geometry. Multiple placements can reference one asset without
  forcing physics to parse render models.
- Add asset/placement selection, duplication, deletion, property editing,
  validation, undo/redo, and faithful preview to the editor. Define reference
  behavior on deletion and preserve identities through save/reopen.
- Replace fixed floor/boundary/obstacle texture assignment where the actual
  scene requires authored appearance. Preserve Vulkan ownership, resource
  recovery, and startup error cleanup.
- **BREAKING:** Replace the singleton prop and fixed texture assumptions in the
  then-current content profile; specify explicit compatibility during design.
- Animated characters belong to P07, lighting/shadows to P10. No general asset
  discovery service, arbitrary glTF support, streaming, or material graph.

## Capabilities

### New Capabilities

- `authored-scene-assets`: Packaged asset identities, supported export profile,
  repeated model placements, authored materials, and collision proxies.

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

## Impact

Affects asset packaging, private model/texture loading, world validation,
static collision construction, renderer resources, and editor preview and
commands. Document export instructions and the supported material profile.
Do not choose a larger rendering architecture merely to support more props.

## Dependencies and Boundaries

P02; requires [P01](../archive/2026-09-06-add-interior-level-authoring/proposal.md).
Static assets may remain immutable after load. P03 can use generated door
geometry and does not wait for final models.

## Acceptance Criteria

- Export and load representative furniture plus a telephone/radio; author
  repeated furniture placements with distinct transforms and correct materials.
- Rendering and authored proxies agree sufficiently for the intended traversal
  and targeting; a render asset is never loaded by physics.
- Save/reopen and undo/redo preserve references. Missing assets and unsupported
  exports produce actionable errors without destroying the active editor scene.
- Run codec/import/proxy tests and game/editor Vulkan smoke; visually inspect
  the representative exports and recovery behavior.
