# terrain-authoring Specification

## Purpose

Defines deterministic, undoable heightfield-brush behavior for authored terrain while preserving material assignments, unrelated level objects and immutable runtime terrain.

## Requirements

### Requirement: Terrain brush targeting
When the active document contains terrain, the editor SHALL target the nearest terrain surface intersection beneath the scene pointer and SHALL display the active brush radius and falloff footprint before modifying samples. Brush input SHALL be suppressed while UI controls own the pointer, and a stroke that never intersects terrain SHALL not change the document or history. When terrain is absent, terrain brushes and terrain-only placement SHALL be unavailable with an explanation, and the editor SHALL NOT retain a brush gesture, hit, or footprint from a previously open terrain document.

#### Scenario: Pointer is over terrain
- **WHEN** a terrain brush is active and the scene pointer intersects the terrain
- **THEN** the viewport displays the world-space brush footprint centered at the nearest terrain intersection

#### Scenario: UI owns the pointer
- **WHEN** the user drags an editor UI control while a terrain brush is selected
- **THEN** no terrain stroke begins and no height sample changes

#### Scenario: Terrain document is replaced by an interior
- **WHEN** a terrain-bearing document is replaced by one without terrain after resolving pending edits and dirty state
- **THEN** terrain tools become unavailable and any previous stroke, target, and footprint are cleared without adding terrain or history to the replacement

### Requirement: Bounded brush controls
The editor SHALL provide raise, lower, and smooth brush modes with finite positive radius, bounded strength, and bounded falloff controls. Radius SHALL range from one terrain sample spacing through 8 metres; raise and lower strength SHALL range from 0.01 through 1 metre per stamp; and smooth strength and falloff SHALL each range from 0 through 1. Values outside these ranges SHALL not be committed.

#### Scenario: Valid controls are changed
- **WHEN** the user commits brush values within their defined bounds
- **THEN** subsequent stamps use those values and existing terrain samples remain unchanged until a stroke occurs

#### Scenario: Brush control is outside its range
- **WHEN** the user attempts to commit a non-finite or out-of-range radius, strength, or falloff
- **THEN** the editor retains the previous valid control value and reports the invalid field

### Requirement: Deterministic raise and lower strokes
Each raise or lower stamp SHALL modify only height samples within the brush radius, applying the selected signed strength weighted by distance and falloff while keeping every result finite. The same initial terrain, brush settings, and ordered world-space stamp positions SHALL produce the same height samples independently of render frame rate.

#### Scenario: Raise stroke crosses terrain
- **WHEN** an ordered raise stroke supplies stamps over valid terrain
- **THEN** affected samples rise by their deterministic weighted amounts, samples outside every footprint remain unchanged, and the preview follows the resulting surface

#### Scenario: Lower stroke is repeated deterministically
- **WHEN** the same lower-stroke input is applied to identical terrain documents at different rendering rates
- **THEN** both documents contain identical final height samples

### Requirement: Deterministic smoothing strokes
Each smooth stamp SHALL move affected samples toward a deterministic local weighted average derived from the pre-stamp neighborhood, with influence controlled by distance, falloff, and smooth strength. A smooth stamp SHALL not change samples outside its radius or alter terrain origin, spacing, or dimensions.

#### Scenario: Rough samples are smoothed
- **WHEN** a smooth stamp covers height samples with differing values
- **THEN** covered samples move toward their deterministic local averages without changing the heightfield's extent or sampling layout

#### Scenario: Smooth strength is zero
- **WHEN** a valid smooth stroke uses zero smooth strength
- **THEN** no terrain sample, dirty state, or history entry changes

### Requirement: Incremental terrain preview
The editor SHALL regenerate the visible terrain surface from the current in-memory height samples while a stroke is active and SHALL preserve unchanged solids, lights, named entries, and prop presentation. Preview rebuilding SHALL not save the document, mutate the game runtime, or create game physics bodies.

#### Scenario: Brush stamp changes samples
- **WHEN** a terrain stamp changes at least one height value
- **THEN** the next available editor frame displays geometry and normals derived from the changed samples

#### Scenario: Editor document is closed
- **WHEN** the user closes a sculpted document after resolving any unsaved-change decision
- **THEN** its preview resources are released without altering a running game level or the packaged game level implicitly

### Requirement: One history operation per stroke
A continuous pointer press that changes terrain SHALL create one undoable history entry containing the affected samples' before and after values, regardless of the number of stamps. A press that changes no sample SHALL create no entry. Undo and redo SHALL restore the complete affected sample set and its corresponding preview, validation, and dirty state.

#### Scenario: Multi-stamp stroke is undone
- **WHEN** the user completes a stroke containing multiple stamps and invokes undo once
- **THEN** every sample changed by that stroke returns to its pre-stroke value

#### Scenario: Stroke is canceled before changing terrain
- **WHEN** a brush press ends without changing a sample
- **THEN** the document history and dirty state remain unchanged

### Requirement: Validation-gated terrain saving
Terrain edits SHALL refresh the shared level validation result after each completed stroke. Invalid slopes, non-finite samples, unsupported entry heights, or entry overlap caused by terrain SHALL be presented in the viewport or validation panel, SHALL keep the document editable, and SHALL prevent saving until corrected or undone. Every entry SHALL be revalidated; a clear entry supported by a structural floor SHALL NOT become invalid merely because terrain below that floor changed height.

#### Scenario: Stroke creates an unsupported slope
- **WHEN** a completed stroke makes any playable terrain triangle exceed the player-supported slope limit
- **THEN** the editor identifies the invalid terrain region, marks the document invalid and dirty, and refuses to save it

#### Scenario: Terrain is repaired
- **WHEN** a later stroke or undo restores every terrain and entry constraint
- **THEN** the editor clears the resolved diagnostics and permits deterministic saving

#### Scenario: Terrain changes below an upper entry
- **WHEN** a stroke preserves valid slopes and does not intrude into an entry's standing clearance or remove its structural support
- **THEN** the upper entry remains valid at its unchanged authored pose

### Requirement: Heightfield-only authoring boundary
Terrain brushes SHALL modify only height samples in the single bounded heightfield and SHALL preserve all authored terrain/solid material assignments and unrelated props and doors. The separate terrain material property SHALL choose one packaged opaque structural material for the whole terrain and SHALL NOT become a texture-paint brush. It SHALL NOT create holes, caves, overhangs, additional terrain surfaces, voxel data, texture paint, procedural or erosion output, or runtime-deformable terrain.

#### Scenario: Terrain tools are inspected
- **WHEN** the terrain-authoring controls and saved level data are inspected
- **THEN** brushes expose only heightfield operations, any separate material choice applies uniformly, and the saved heightfield layout remains bounded
