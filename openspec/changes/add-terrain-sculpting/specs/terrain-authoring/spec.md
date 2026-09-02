## Purpose

Defines deterministic, undoable terrain-brush behavior for editing the bounded FPS heightfield while keeping game runtime terrain immutable and non-voxel.

## ADDED Requirements

### Requirement: Terrain brush targeting
The editor SHALL target the nearest terrain surface intersection beneath the scene pointer and SHALL display the active brush radius and falloff footprint before modifying samples. Brush input SHALL be suppressed while UI controls own the pointer, and a stroke that never intersects terrain SHALL not change the document or history.

#### Scenario: Pointer is over terrain
- **WHEN** a terrain brush is active and the scene pointer intersects the terrain
- **THEN** the viewport displays the world-space brush footprint centered at the nearest terrain intersection

#### Scenario: UI owns the pointer
- **WHEN** the user drags an editor UI control while a terrain brush is selected
- **THEN** no terrain stroke begins and no height sample changes

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
The editor SHALL regenerate the visible terrain surface from the current in-memory height samples while a stroke is active and SHALL preserve unchanged solids, lights, spawn, and prop presentation. Preview rebuilding SHALL not save the document, mutate the game runtime, or create game physics bodies.

#### Scenario: Brush stamp changes samples
- **WHEN** a terrain stamp changes at least one height value
- **THEN** the next available editor frame displays geometry and normals derived from the changed samples

#### Scenario: Editor document is closed
- **WHEN** the user closes a sculpted document after resolving any unsaved-change decision
- **THEN** its preview resources are released without altering any running or packaged FPS level implicitly

### Requirement: One history operation per stroke
A continuous pointer press that changes terrain SHALL create one undoable history entry containing the affected samples' before and after values, regardless of the number of stamps. A press that changes no sample SHALL create no entry. Undo and redo SHALL restore the complete affected sample set and its corresponding preview, validation, and dirty state.

#### Scenario: Multi-stamp stroke is undone
- **WHEN** the user completes a stroke containing multiple stamps and invokes undo once
- **THEN** every sample changed by that stroke returns to its pre-stroke value

#### Scenario: Stroke is canceled before changing terrain
- **WHEN** a brush press ends without changing a sample
- **THEN** the document history and dirty state remain unchanged

### Requirement: Validation-gated terrain saving
Terrain edits SHALL refresh the shared level validation result after each completed stroke. Invalid slopes, non-finite samples, unsupported spawn height, or spawn overlap caused by the terrain SHALL be presented in the viewport or validation panel, SHALL keep the document editable, and SHALL prevent saving until corrected or undone.

#### Scenario: Stroke creates an unsupported slope
- **WHEN** a completed stroke makes any playable terrain triangle exceed the player-supported slope limit
- **THEN** the editor identifies the invalid terrain region, marks the document invalid and dirty, and refuses to save it

#### Scenario: Terrain is repaired
- **WHEN** a later stroke or undo restores every terrain and spawn constraint
- **THEN** the editor clears the resolved diagnostics and permits deterministic saving

### Requirement: Heightfield-only authoring boundary
Terrain authoring SHALL modify only height samples in the single bounded heightfield. It SHALL NOT create holes, caves, overhangs, additional terrain surfaces, voxel data, texture paint, procedural or erosion output, or runtime-deformable terrain.

#### Scenario: Terrain tools are inspected
- **WHEN** the terrain-authoring controls and saved level data are inspected
- **THEN** they expose only heightfield brush operations and the existing fixed terrain representation
