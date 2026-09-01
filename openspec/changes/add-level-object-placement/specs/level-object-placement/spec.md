## Purpose

Defines bounded selection and placement behavior for the FPS level objects already represented by the persisted level document, without introducing a general scene hierarchy or component model.

## ADDED Requirements

### Requirement: Flat supported object set
The editor SHALL present one flat selectable set containing every axis-aligned solid plus the single player spawn, exactly two point lights, and the single packaged static-prop placement with its box proxy. The editor SHALL allow solids to be added, duplicated, and removed, but SHALL NOT create or remove the fixed-count spawn, lights, or prop placement or introduce unsupported object, asset, component, or hierarchy types.

#### Scenario: Level objects are listed
- **WHEN** a valid level document is active
- **THEN** the object list contains each supported object exactly once with its concrete FPS-oriented type and no parent-child hierarchy

#### Scenario: User requests a new object
- **WHEN** the user adds an object
- **THEN** the editor creates one axis-aligned solid using a supported solid kind and surface role without offering arbitrary components or assets

### Requirement: Consistent list and viewport selection
Selecting an object in the list or selecting its visible representation in the viewport SHALL establish the same single active selection. Viewport selection SHALL choose the nearest selectable object intersected by the pointer ray, selection shall persist across camera movement, and selecting empty space SHALL clear it.

#### Scenario: Overlapping objects are picked
- **WHEN** the pointer ray intersects multiple selectable objects
- **THEN** the nearest intersection becomes the sole active selection and is identified in both viewport and object list

#### Scenario: Empty space is selected
- **WHEN** the user performs selection where the pointer ray intersects no selectable object
- **THEN** the active selection is cleared in both viewport and object list

### Requirement: Bounded object property editing
The editor SHALL expose finite numeric controls appropriate to the selected concrete object. Solids SHALL support center, positive half extents, tint, kind, and fixed surface role; the spawn SHALL support foot position and yaw; point lights SHALL support position, non-negative color, positive intensity, and positive radius; and the prop SHALL support translation, yaw, positive uniform scale, fixed obstacle surface, and positive box-proxy center and half extents. Structural solids SHALL remain axis-aligned.

#### Scenario: Solid is edited
- **WHEN** the user commits valid solid center, extent, kind, tint, or surface values
- **THEN** the document and scene preview reflect the committed values and the document becomes dirty

#### Scenario: Non-finite value is entered
- **WHEN** a property edit cannot produce a finite value valid for that field
- **THEN** the editor rejects the commit, retains the previous property value, and reports the field error

### Requirement: Direct object placement
The editor SHALL allow a selected solid, light, spawn, or prop placement to be positioned from a viewport intersection with the terrain. Direct placement SHALL preserve properties unrelated to position and SHALL use a deterministic object-specific vertical anchor so repeated placement at the same terrain point produces the same result.

#### Scenario: Solid is placed on terrain
- **WHEN** the user directly places a selected solid at a terrain intersection
- **THEN** its horizontal center moves to the intersection and its bottom rests on the terrain according to its current half extent

#### Scenario: Placement ray misses terrain
- **WHEN** the user requests direct placement but the pointer ray has no terrain intersection
- **THEN** the selected object's properties and document dirty state remain unchanged

### Requirement: Object duplication and removal
Duplicating a solid SHALL create an independently selectable copy with the same authored values at a deterministic visible offset. Removing a selected solid SHALL remove only that solid. Duplicate and remove operations SHALL respect the level's maximum solid count and SHALL not apply to the fixed spawn, lights, or prop placement.

#### Scenario: Solid is duplicated
- **WHEN** the user duplicates a selected solid below the maximum count
- **THEN** one offset copy is inserted, selected, previewed, and marks the document dirty

#### Scenario: Fixed-count object removal is requested
- **WHEN** the user requests deletion of the spawn, either point light, or the static prop placement
- **THEN** the operation is unavailable and the document remains unchanged

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
