## MODIFIED Requirements

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

### Requirement: Incremental terrain preview
The editor SHALL regenerate the visible terrain surface from the current in-memory height samples while a stroke is active and SHALL preserve unchanged solids, lights, named entries, and prop presentation. Preview rebuilding SHALL not save the document, mutate the game runtime, or create game physics bodies.

#### Scenario: Brush stamp changes samples
- **WHEN** a terrain stamp changes at least one height value
- **THEN** the next available editor frame displays geometry and normals derived from the changed samples

#### Scenario: Editor document is closed
- **WHEN** the user closes a sculpted document after resolving any unsaved-change decision
- **THEN** its preview resources are released without altering a running game level or the packaged game level implicitly

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
