# level-object-placement Specification

## Purpose

Defines bounded selection and placement behavior for the game objects already represented by the version-2 persisted level document, without introducing a general scene hierarchy or component model.

## Requirements

### Requirement: Flat supported object set
The editor SHALL present one flat selectable set containing every axis-aligned solid and named entry, exactly two point lights, the single packaged static-prop placement with its box proxy, and the optional singleton switch. The editor SHALL allow solids and entries to be added, duplicated, and removed within their bounds and the switch to be added when absent and removed when present. It SHALL NOT create or remove the fixed-count lights or prop placement or introduce unsupported object, asset, component, or hierarchy types. The default entry SHALL be visibly identified.

#### Scenario: Level objects are listed
- **WHEN** a valid level document is active
- **THEN** the object list contains each supported object exactly once with its concrete game-specific type, entry identifier where applicable, and no parent-child hierarchy

#### Scenario: User requests a new object
- **WHEN** the user adds a solid
- **THEN** the editor creates one axis-aligned solid using a supported solid kind and one of the fixed floor, boundary, or obstacle surface roles without offering arbitrary components or assets

#### Scenario: Switch is added to a level
- **WHEN** the user adds a light switch while the document has none
- **THEN** one switch is created, selected, previewed, and recorded as one undoable edit; another switch cannot be added while it is present

#### Scenario: Entry is added
- **WHEN** the user adds an entry below the entry limit
- **THEN** the editor creates and selects an entry with a unique durable identifier, refreshes validation, and records one undoable edit

### Requirement: Consistent list and viewport selection
Selecting an object in the list or selecting its visible representation in the viewport SHALL establish the same single active selection. Viewport selection SHALL choose the nearest selectable object intersected by the pointer ray, selection shall persist across camera movement, and selecting empty space SHALL clear it.

#### Scenario: Overlapping objects are picked
- **WHEN** the pointer ray intersects multiple selectable objects
- **THEN** the nearest intersection becomes the sole active selection and is identified in both viewport and object list

#### Scenario: Empty space is selected
- **WHEN** the user performs selection where the pointer ray intersects no selectable object
- **THEN** the active selection is cleared in both viewport and object list

### Requirement: Bounded object property editing
The editor SHALL expose finite numeric controls appropriate to the selected concrete object. Solids SHALL support center, positive half extents, tint, kind, and fixed surface role; entries SHALL support identifier, foot position, yaw, and making that entry the default; point lights SHALL support position, non-negative color, positive intensity, and positive radius; and the prop SHALL support translation, yaw, positive uniform scale, fixed obstacle surface, finite signed box-proxy center coordinates, and positive box-proxy half extents. The switch SHALL support finite position and yaw, selection of one of the two authored point lights, and an initial on/off value; its dimensions and appearance SHALL remain fixed. Structural solids SHALL remain axis-aligned. Renaming an entry SHALL update any default reference to it in the same edit, preserve its pose and editor selection, and reject invalid or duplicate identifiers without committing them.

#### Scenario: Solid is edited
- **WHEN** the user commits valid solid center, extent, kind, tint, or surface values
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

### Requirement: Direct object placement
The editor SHALL allow a selected solid, light, entry, prop placement, or switch to be positioned from an explicitly selected scene-surface or terrain-only placement mode. Scene-surface placement SHALL intersect actual terrain triangles when present and structural-solid faces, exclude the object being moved, and show the candidate target, hit height, and face orientation before committing. It SHALL consider the nearest remaining surface and SHALL NOT skip an unsuitable nearer surface to place through it. On an upward structural face or terrain, solids SHALL rest their bottom at the hit, entries SHALL place their feet at the hit, props SHALL place their translation anchor at the hit, and lights and switches SHALL use an explicit height offset. On a vertical structural face, solids SHALL rest their contacting face against the hit, lights SHALL use an explicit outward offset, and switches SHALL mount outside the face with their front oriented outward. Wall placement SHALL be unavailable for entries and the prop. Placement SHALL preserve unrelated authored properties. Terrain-only light and switch placement SHALL initialize its offset from their previous height above terrain when available. Repeated placement using the same target and settings SHALL be deterministic.

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

### Requirement: Object duplication and removal
Duplicating a solid SHALL create an independently selectable copy with the same authored values at a deterministic visible offset. Removing a selected solid SHALL remove only that solid. Solid operations SHALL respect the level's maximum solid count. Duplicating an entry SHALL preserve its pose, allocate a unique identifier, select the copy, and leave the default unchanged. Entry operations SHALL respect the sixteen-entry maximum. Removing the last entry or the current default entry SHALL be unavailable; the author SHALL be able to select another default before removing the old one. The optional switch SHALL support removal and undo/redo of that removal but SHALL NOT support duplication. The fixed lights and prop placement SHALL NOT support duplication or removal.

#### Scenario: Solid is duplicated
- **WHEN** the user duplicates a selected solid below the maximum count
- **THEN** one offset copy is inserted, selected, previewed, and marks the document dirty

#### Scenario: Fixed-count object removal is requested
- **WHEN** the user requests deletion of either point light or the static prop placement
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

### Requirement: Bounded undo and redo
The editor SHALL retain at most 128 committed object-edit operations for the active document. One committed property edit, direct placement, add, duplicate, or remove action SHALL form one history entry; undo and redo SHALL restore the corresponding document content and selection when that object still exists. Opening or replacing a document SHALL clear both history stacks, and committing a new edit after undo SHALL discard the redo branch.

#### Scenario: Committed edit is undone and redone
- **WHEN** the user undoes and then redoes the most recent committed object edit
- **THEN** the affected properties, selection, preview, validation result, and dirty state follow the restored document states

#### Scenario: History reaches its bound
- **WHEN** a new edit is committed while 128 undo entries are retained
- **THEN** the oldest entry is discarded and the newest edit remains undoable

### Requirement: Validation-gated object saving
Every committed edit SHALL refresh level validation and present all current failures without discarding the editable document. The editor SHALL refuse to save while the document is invalid and SHALL allow the user to correct or undo the responsible edits.

#### Scenario: Edit creates an invalid level
- **WHEN** an otherwise valid property commit makes the spawn overlap a solid or violates another cross-object level constraint
- **THEN** the preview and editable value remain, the document is marked invalid and dirty, and save is unavailable with an actionable diagnostic

#### Scenario: Invalid edit is corrected
- **WHEN** later editing or undo restores a valid level
- **THEN** validation clears the resolved failure and deterministic saving becomes available again

### Requirement: Switch initial-state preview
The editor SHALL preview the switch's authored initial light state without running gameplay interaction. A light unlinked by editing or switch removal SHALL return to enabled in preview. Terrain sculpting, undo/redo, and presentation recovery SHALL preserve the switch geometry and initial-state preview for the current document. Invalid switch fields SHALL remain diagnosable without unsafe rendering or light indexing.

#### Scenario: Switch starts off in the preview
- **WHEN** the user commits Initially on as false for a valid switch
- **THEN** the linked light contributes no preview illumination and the other light remains enabled

#### Scenario: Switch link changes
- **WHEN** an initially-off switch is changed from one point-light slot to the other
- **THEN** the formerly linked light is enabled and the newly linked light is disabled in preview

#### Scenario: Terrain or presentation changes
- **WHEN** a document containing a switch is sculpted, undone, redone, resized, or restored from minimization
- **THEN** subsequent editor frames retain the switch at its current authored placement with the current authored initial light state

#### Scenario: Invalid switch is previewed
- **WHEN** a switch has invalid fields that cannot be rendered or used to select a light safely
- **THEN** the editor reports validation failures and retains the editable document without invalid light access or unsafe geometry submission
