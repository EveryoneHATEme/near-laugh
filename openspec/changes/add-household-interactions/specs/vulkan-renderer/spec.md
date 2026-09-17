## MODIFIED Requirements

### Requirement: Bounded changing opaque presentation
The renderer SHALL present the bounded changing opaque geometry supplied for each scene frame, including generated door leaves, physical household boxes, readable panels and temporary feedback geometry, with finite full three-axis orientation and correctly transformed world-space normals, opaque depth visibility, existing textures, and the current point-light, ambient, and optional spotlight state. Supported point-light shadows SHALL use the same changing geometry as the color presentation for that submitted frame, including accepted obstructed door poses. Changing a door/household pose or feedback SHALL NOT rebuild or reupload the immutable world or prop meshes or recreate textures, immutable lighting resources, pipeline, or swapchain. The renderer SHALL NOT infer movement, locks, targeting, or feedback timing from gameplay. Per-frame changing geometry SHALL remain within the existing Vulkan baseline and frames-in-flight lifetime guarantees.

#### Scenario: Doors move repeatedly
- **WHEN** consecutive frames contain changing accepted leaf poses and feedback
- **THEN** each submitted frame uses its own supplied geometry with correct lighting and depth while static resources remain valid

#### Scenario: No doors are authored
- **WHEN** a scene frame has no changing opaque geometry
- **THEN** no empty allocation or invalid door draw is required and the static scene remains available

#### Scenario: Frame slot is reused
- **WHEN** a frame slot carrying earlier door vertices is reused
- **THEN** the earlier GPU work completes before those vertices or their allocations can be overwritten or destroyed

#### Scenario: Presentation recovers during door motion
- **WHEN** repeated motion spans resize, minimize/restore, out-of-date acquisition, or suboptimal presentation
- **THEN** the next submitted frame uses current supplied poses without stale vertices, resetting runtime state, or validation errors

#### Scenario: Changing geometry allocation fails
- **WHEN** allocation, mapping, upload, or scene replacement for door presentation fails
- **THEN** the failure is actionable, partial owners are released exactly once, and failed editor replacement retains the preceding usable resources

#### Scenario: Box tumbles under a shadowed light
- **WHEN** a physical box changes pitch, roll and yaw across submitted frames
- **THEN** its corners, normals, depth and shadow silhouette use the same supplied full orientation as its physical/targeting pose

#### Scenario: Household-only scene starts
- **WHEN** a valid scene has household geometry but no doors
- **THEN** all required changing-geometry material resources are prepared and box/document/indicator draws are valid without a door dependency

#### Scenario: Maximum doors and household objects coexist
- **WHEN** the scene contains the maximum supported doors, boxes, documents and radio indicators
- **THEN** the complete combined geometry fits checked frame capacity without silently removing existing door feedback or household objects


### Requirement: Game text overlay presentation
The runtime renderer SHALL draw supplied bounded resolved caption, document-page and household-feedback text over the scene with the layout and glyph support required by game-text-presentation. Text SHALL remain readable independently of scene lighting and depth, and SHALL NOT change scene lighting, depth coverage or existing geometry. The renderer SHALL NOT resolve cue identities, start playback or advance caption time, navigate pages or interpret household state. Empty text SHALL require no text draw. Unsupported text or invalid font resources SHALL produce actionable errors instead of unsafe indexing or unbounded allocation.

#### Scenario: Text overlays a dark or occluded scene
- **WHEN** a frame supplies valid caption text while the camera faces dark geometry
- **THEN** its glyphs and backing are visible over the scene without being occluded by world depth or darkened by scene lights

#### Scenario: No caption is active
- **WHEN** a frame supplies no text
- **THEN** the scene renders normally without an empty or invalid glyph draw

#### Scenario: Reading and captions share a dark frame
- **WHEN** document text, control hints and foreground/ambience captions are supplied together
- **THEN** all occupy their specified independent screen regions with checked geometry capacity and readable contrast, without changing world depth or lighting
