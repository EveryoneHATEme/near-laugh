## Why

The persisted heightfield is structurally ready for authoring, but editing thousands of samples numerically is not practical. Focused terrain brushes complete the primary level-authoring goal while retaining the existing bounded heightfield and shared render/collision source.

## What Changes

- Add viewport terrain picking and raise, lower, and smooth brushes with bounded radius, strength, and falloff controls.
- Preview heightfield geometry updates in the editor and expose slope, finiteness, spawn-support, and spawn-clearance validation failures before saving.
- Treat each continuous brush stroke as one undoable document operation and integrate it with dirty-state and deterministic persistence.
- Preserve the game's immutable startup level; sculpting occurs only in the standalone editor.
- Keep the version-2 document and fixed floor/boundary/obstacle surface set unchanged by terrain editing.
- Exclude holes, caves, overhangs, texture painting, erosion simulation, procedural generation, runtime deformation, and dynamic collision rebuilding in the game.

## Capabilities

### New Capabilities

- `terrain-authoring`: Purpose-built editor picking, brush operations, preview rebuilding, validation visualization, and undo/redo for the bounded game heightfield.

### Modified Capabilities

- `level-editor`: Replace the previous terrain read-only milestone constraint
  with height-sample authoring while keeping terrain layout read-only.

## Impact

- Affects editor document state, viewport picking and overlays, terrain mesh preview rebuilding, undo storage, level validation, and editor tests.
- Depends on `remove-legacy-fps-assumptions` completing first, plus `add-bounded-level-persistence`, `add-level-editor-foundation`, and the document history established by `add-level-object-placement`.
- Does not change runtime terrain mutability or introduce voxel terrain, general geometry editing, or a material editor.
