## MODIFIED Requirements

### Requirement: Flat supported object set
The editor SHALL present one flat selectable set containing every axis-aligned solid and named entry, exactly two point lights, every static-prop placement with its authored boxes, the optional singleton switch, and every authored hinged door. The editor SHALL allow solids and entries to be added, duplicated, and removed within their bounds and the switch to be added when absent and removed when present. Doors SHALL support add, duplicate, and remove within their thirty-two-door bound. Props SHALL support add, duplicate and remove within the 128-placement bound using known catalog models. It SHALL NOT create or remove the fixed-count lights, edit the packaged catalog, or introduce unsupported component or hierarchy types. The default entry SHALL be visibly identified.

#### Scenario: Level objects are listed
- **WHEN** a valid level document is active
- **THEN** the object list contains each supported object exactly once with its concrete game-specific type, entry, prop or door identifier where applicable, and no parent-child hierarchy

#### Scenario: User requests a new object
- **WHEN** the user adds a solid
- **THEN** the editor creates one axis-aligned solid using a supported solid kind and a known structural material independent of kind without offering arbitrary components or filesystem paths

#### Scenario: Switch is added to a level
- **WHEN** the user adds a light switch while the document has none
- **THEN** one switch is created, selected, previewed, and recorded as one undoable edit; another switch cannot be added while it is present

#### Scenario: Entry is added
- **WHEN** the user adds an entry below the entry limit
- **THEN** the editor creates and selects an entry with a unique durable identifier, refreshes validation, and records one undoable edit

#### Scenario: Model placement is added
- **WHEN** the author chooses a catalog model below the placement bound
- **THEN** one selected placement is created with a unique prop ID and copied editable model-default boxes in one history entry

### Requirement: Bounded object property editing
The editor SHALL expose finite numeric controls appropriate to the selected concrete object. Solids SHALL support center, positive half extents, tint, kind, and independent structural material; entries SHALL support identifier, foot position, yaw, and making that entry the default; point lights SHALL support position, non-negative color, positive intensity, and positive radius; and each prop SHALL support identifier, catalog model, translation, yaw, positive uniform scale, and zero through eight boxes with finite local centers and positive half extents. Present terrain SHALL expose its one structural material separately from height-brush operations. Prop renaming SHALL reject invalid or duplicate prop IDs and preserve selection. Changing a model SHALL preserve the prop ID, transform and existing boxes; explicit reset-to-model-default boxes SHALL be a separate undoable operation. The switch SHALL support finite position and yaw, selection of one of the two authored point lights, and an initial on/off value; its dimensions and appearance SHALL remain fixed. Doors SHALL expose identifier, hinge position, closed yaw, leaf width/height/thickness, signed opening angle, angular speed, lock side, and initial open/locked states within interactive-doors bounds. Structural solids SHALL remain axis-aligned. Renaming an entry SHALL update any default reference to it in the same edit, preserve its pose and editor selection, and reject invalid or duplicate identifiers without committing them. Renaming a door SHALL likewise reject malformed or duplicate door identifiers while preserving its editor selection and other fields.

#### Scenario: Solid is edited
- **WHEN** the user commits valid solid center, extent, kind, tint, or material values
- **THEN** the document and scene preview reflect the committed values and the document becomes dirty

#### Scenario: Non-finite value is entered
- **WHEN** a property edit cannot produce a finite value valid for that field
- **THEN** the editor rejects the commit, retains the previous property value, and reports the field error

#### Scenario: Switch properties are edited
- **WHEN** the user commits a switch position, yaw, linked point light, or initial state
- **THEN** the document and preview reflect the value in one undoable edit, preserve unrelated authored values, and refresh validation and dirty state

#### Scenario: Default entry is renamed
- **WHEN** the user commits a valid unique identifier for the default entry
- **THEN** its identifier and the default reference change together, its pose and selection are preserved, and one undo restores both prior values

#### Scenario: Prop model or boxes are edited
- **WHEN** the author changes a selected prop model or commits an authored box-list edit
- **THEN** the prop identity and unrelated fields remain unchanged and preview, proxy overlay, validation and undo/redo reflect the operation

#### Scenario: Structural kind changes
- **WHEN** the author changes a solid kind while its material remains a valid structural material
- **THEN** the authored material is preserved independently of the new collision kind

### Requirement: Direct object placement
The editor SHALL allow a selected solid, light, entry, prop placement, switch, or door to be positioned from an explicitly selected scene-surface or terrain-only placement mode. Scene-surface placement SHALL intersect actual terrain triangles when present and structural-solid faces, exclude the object being moved, and show the candidate target, hit height, and face orientation before committing. It SHALL consider the nearest remaining surface and SHALL NOT skip an unsuitable nearer surface to place through it. On an upward structural face or terrain, solids SHALL rest their bottom at the hit, entries SHALL place their feet at the hit, props SHALL place their translation anchor at the hit, lights and switches SHALL use an explicit height offset, and doors SHALL place their bottom hinge anchor at the hit plus a visible floor-clearance offset. On a vertical structural face, solids SHALL rest their contacting face against the hit, lights SHALL use an explicit outward offset, and switches SHALL mount outside the face with their front oriented outward. Wall placement SHALL be unavailable for entries, props, and doors. Door surface placement SHALL retain authored yaw and swing configuration; it SHALL NOT search through an unsuitable nearer face or infer a doorway from nearby geometry. Placement SHALL preserve unrelated authored properties. Terrain-only light and switch placement SHALL initialize its offset from their previous height above terrain when available. Repeated placement using the same target and settings SHALL be deterministic.

#### Scenario: Solid is placed on terrain
- **WHEN** the user directly places a selected solid at a terrain intersection
- **THEN** its horizontal center moves to the intersection and its bottom rests on terrain according to its current half extent

#### Scenario: Placement ray misses terrain
- **WHEN** terrain-only placement is active but the pointer ray has no terrain intersection
- **THEN** the selected object's properties and document dirty state remain unchanged

#### Scenario: Switch is placed on terrain
- **WHEN** the user places a selected switch on terrain without changing its initialized height offset
- **THEN** its horizontal position moves to the hit and its previous height above terrain is retained without changing its light link, yaw, or initial state

#### Scenario: Entry is placed on an upper floor
- **WHEN** scene-surface placement targets the upward face of an upper-floor slab while terrain or a lower floor exists below it
- **THEN** the candidate identifies that slab and elevation and the entry's feet are placed there in one undoable edit

#### Scenario: Switch is mounted on a wall
- **WHEN** scene-surface placement targets a vertical wall face for the selected switch
- **THEN** the switch is placed just outside that face, its front faces outward, and its light link and initial state remain unchanged

#### Scenario: Nearer surface is unsuitable
- **WHEN** a nearer wall or slab underside occludes a possible floor hit for an entry
- **THEN** the editor indicates that the nearest target is unsuitable and clicking does not place the entry on a surface behind it

#### Scenario: Placement is canceled or captured
- **WHEN** the user cancels placement, the ray misses, or UI or camera navigation owns the input
- **THEN** no placement edit, history entry, or dirty-state change occurs

#### Scenario: Repeated prop is placed on a floor
- **WHEN** the author places a selected model prop on a suitable upper structural face
- **THEN** its translation anchor moves to that hit while ID, model, yaw, scale and local boxes remain unchanged in one undoable edit

### Requirement: Object duplication and removal
Duplicating a solid SHALL create an independently selectable copy with the same authored values at a deterministic visible offset. Removing a selected solid SHALL remove only that solid. Solid operations SHALL respect the level's maximum solid count. Duplicating an entry SHALL preserve its pose, allocate a unique identifier, select the copy, and leave the default unchanged. Entry operations SHALL respect the sixteen-entry maximum. Removing the last entry or the current default entry SHALL be unavailable; the author SHALL be able to select another default before removing the old one. The optional switch SHALL support removal and undo/redo of that removal but SHALL NOT support duplication. Duplicating a door SHALL allocate a unique durable door identifier, preserve the configuration at a deterministic visible horizontal offset, select the copy, and refresh validation. Undo and redo SHALL restore the same allocated identifier. Removing a door SHALL remove only that definition and its preview; no reference framework is introduced in the absence of authored door references. Duplicating a prop SHALL preserve its model, scale, yaw and local boxes at a deterministic visible horizontal offset, allocate a unique prop identifier and select the copy; undo/redo SHALL restore that same allocated identifier. Removal SHALL delete only that placement and never its catalog asset or another placement sharing it. Only the two fixed lights SHALL prohibit duplication and removal.

#### Scenario: Solid is duplicated
- **WHEN** the user duplicates a selected solid below the maximum count
- **THEN** one offset copy is inserted, selected, previewed, and marks the document dirty

#### Scenario: Fixed-count object removal is requested
- **WHEN** the user requests deletion of either point light
- **THEN** the operation is unavailable and the document remains unchanged

#### Scenario: Switch is removed and restored
- **WHEN** the user removes the selected switch and then undoes the removal
- **THEN** the switch disappears and returns with its original authored fields and selection, and preview, validation, and dirty state follow the restored document

#### Scenario: Entry is duplicated and restored
- **WHEN** the user duplicates an entry and then undoes and redoes that edit
- **THEN** the copy disappears and returns with the same allocated identifier and pose, and the original default remains unchanged

#### Scenario: Default entry removal is requested
- **WHEN** the user requests removal of the default or only entry
- **THEN** removal is unavailable with an explanation and the document remains unchanged

#### Scenario: Door is duplicated and removed
- **WHEN** the user duplicates a door, removes the copy, and undoes and redoes those operations
- **THEN** door configuration, allocated identity, selection, preview, validation, and dirty state follow the corresponding document states without changing the original

#### Scenario: Shared model placement is deleted
- **WHEN** two chairs reference the same model and the author deletes one
- **THEN** only that placement is removed and one undo restores its identity, transform, boxes and selection while the other chair remains unchanged

### Requirement: Consistent list and viewport selection
List and viewport selection SHALL establish the same single active selection, choosing the nearest intersected selectable representation. Prop viewport picking SHALL use transformed visible model bounds independently of collision boxes, so decorative placements remain selectable. A missing/invalid model reference SHALL retain a selectable placement marker and any safe authored box bounds with a diagnostic. Selection SHALL persist across camera movement, and clicking empty space SHALL clear it. Selected render bounds and optional collision-box overlays SHALL be distinguishable.

#### Scenario: Overlapping objects are picked
- **WHEN** the pointer ray intersects multiple selectable objects
- **THEN** the nearest intersection becomes the sole active selection and is identified in both viewport and object list

#### Scenario: Empty space is selected
- **WHEN** the user performs selection where the pointer ray intersects no selectable object
- **THEN** the active selection is cleared in both viewport and object list
