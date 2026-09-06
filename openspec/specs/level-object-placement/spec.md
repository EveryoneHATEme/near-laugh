# level-object-placement Specification

## Purpose

Defines bounded selection and placement behavior for the game objects already represented by the version-2 persisted level document, without introducing a general scene hierarchy or component model.

## Requirements

### Requirement: Flat supported object set
The editor SHALL present one flat selectable set containing every axis-aligned solid plus the single player spawn, exactly two point lights, the single packaged static-prop placement with its box proxy, and the optional singleton switch. The editor SHALL allow solids to be added, duplicated, and removed, SHALL allow the switch to be added when absent and removed when present, but SHALL NOT create or remove the fixed-count spawn, lights, or prop placement or introduce unsupported object, asset, component, or hierarchy types.

#### Scenario: Level objects are listed
- **WHEN** a valid level document is active
- **THEN** the object list contains each supported object exactly once with its concrete game-specific type and no parent-child hierarchy

#### Scenario: User requests a new object
- **WHEN** the user adds a solid
- **THEN** the editor creates one axis-aligned solid using a supported solid kind and one of the fixed floor, boundary, or obstacle surface roles without offering arbitrary components or assets

#### Scenario: Switch is added to a level
- **WHEN** the user adds a light switch while the document has none
- **THEN** one switch is created, selected, previewed, and recorded as one undoable edit; another switch cannot be added while it is present

### Requirement: Consistent list and viewport selection
Selecting an object in the list or selecting its visible representation in the viewport SHALL establish the same single active selection. Viewport selection SHALL choose the nearest selectable object intersected by the pointer ray, selection shall persist across camera movement, and selecting empty space SHALL clear it.

#### Scenario: Overlapping objects are picked
- **WHEN** the pointer ray intersects multiple selectable objects
- **THEN** the nearest intersection becomes the sole active selection and is identified in both viewport and object list

#### Scenario: Empty space is selected
- **WHEN** the user performs selection where the pointer ray intersects no selectable object
- **THEN** the active selection is cleared in both viewport and object list

### Requirement: Bounded object property editing
The editor SHALL expose finite numeric controls appropriate to the selected concrete object. Solids SHALL support center, positive half extents, tint, kind, and fixed surface role; the spawn SHALL support foot position and yaw; point lights SHALL support position, non-negative color, positive intensity, and positive radius; and the prop SHALL support translation, yaw, positive uniform scale, fixed obstacle surface, finite signed box-proxy center coordinates, and positive box-proxy half extents. The switch SHALL support finite position and yaw, selection of one of the two authored point lights, and an initial on/off value; its dimensions and appearance SHALL remain fixed. Structural solids SHALL remain axis-aligned.

#### Scenario: Solid is edited
- **WHEN** the user commits valid solid center, extent, kind, tint, or surface values
- **THEN** the document and scene preview reflect the committed values and the document becomes dirty

#### Scenario: Non-finite value is entered
- **WHEN** a property edit cannot produce a finite value valid for that field
- **THEN** the editor rejects the commit, retains the previous property value, and reports the field error

#### Scenario: Switch properties are edited
- **WHEN** the user commits a switch position, yaw, linked point light, or initial state
- **THEN** the document and preview reflect the value in one undoable edit, preserve unrelated authored values, and refresh validation and dirty state

### Requirement: Direct object placement
The editor SHALL allow a selected solid, light, spawn, prop placement, or switch to be positioned from a viewport intersection with the terrain. Direct placement SHALL preserve properties unrelated to position and SHALL use a deterministic object-specific vertical anchor so repeated placement at the same terrain point produces the same result. The switch's terrain placement SHALL preserve its previous height above its terrain anchor, yaw, light link, and initial state.

#### Scenario: Solid is placed on terrain
- **WHEN** the user directly places a selected solid at a terrain intersection
- **THEN** its horizontal center moves to the intersection and its bottom rests on the terrain according to its current half extent

#### Scenario: Placement ray misses terrain
- **WHEN** the user requests direct placement but the pointer ray has no terrain intersection
- **THEN** the selected object's properties and document dirty state remain unchanged

#### Scenario: Switch is placed on terrain
- **WHEN** the user places a selected switch at a valid terrain intersection
- **THEN** its horizontal position moves to the hit and its height above terrain is retained without changing its light link, yaw, or initial state

### Requirement: Object duplication and removal
Duplicating a solid SHALL create an independently selectable copy with the same authored values at a deterministic visible offset. Removing a selected solid SHALL remove only that solid. Solid duplicate and remove operations SHALL respect the level's maximum solid count. The optional switch SHALL support removal and undo/redo of that removal, but SHALL NOT support duplication. The fixed spawn, lights, and prop placement SHALL NOT support duplication or removal.

#### Scenario: Solid is duplicated
- **WHEN** the user duplicates a selected solid below the maximum count
- **THEN** one offset copy is inserted, selected, previewed, and marks the document dirty

#### Scenario: Fixed-count object removal is requested
- **WHEN** the user requests deletion of the spawn, either point light, or the static prop placement
- **THEN** the operation is unavailable and the document remains unchanged

#### Scenario: Switch is removed and restored
- **WHEN** the user removes the selected switch and then undoes the removal
- **THEN** the switch disappears and returns with its original authored fields and selection, and preview, validation, and dirty state follow the restored document

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
