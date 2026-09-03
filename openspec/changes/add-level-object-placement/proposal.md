## Why

The editor foundation can inspect a level but cannot yet change its layout. Purpose-built object placement is the smallest useful authoring step for arranging game structures, props, lights, and the player spawn without introducing arbitrary components or a general scene editor.

## What Changes

- Add selection through both viewport picking and a flat object list for the bounded level object types.
- Add, duplicate, remove, and edit supported axis-aligned solids; reposition and edit the existing single static prop and its box proxy, the two point lights, and the single player spawn.
- Provide numeric property editing plus direct ground-plane placement and simple project-specific viewport manipulation where useful.
- Add bounded undo/redo, document dirty-state integration, deterministic save ordering, and validation feedback for every edit.
- Keep the version-2 document, floor/boundary/obstacle surface roles, single packaged prop, fixed two-light environment, transforms, and collision representations constrained to current game needs.

## Capabilities

### New Capabilities

- `level-object-placement`: Selection, placement, property editing, deletion, duplication, undo/redo, and validation for supported game level objects.

### Modified Capabilities


## Impact

- Affects editor document state, picking, editor rendering overlays, ImGui panels, level serialization, validation, and editor tests.
- Depends on `remove-legacy-fps-assumptions` completing first, in addition to the archived `add-bounded-level-persistence` and `add-level-editor-foundation` changes.
- Does not add arbitrary asset discovery, multiple prop assets or placements, variable light counts, hierarchy, prefabs, generic components, arbitrary rotation for structural boxes, runtime mutation, or play-in-editor.
