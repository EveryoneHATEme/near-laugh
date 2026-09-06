## ADDED Requirements

### Requirement: Bounded changing opaque presentation
The renderer SHALL present the bounded changing opaque geometry supplied for each scene frame, including this milestone's generated door leaves and temporary feedback geometry, with correct world-space normals, opaque depth visibility, existing textures, and the current point-light, ambient, and optional spotlight state. Changing a door pose or feedback SHALL NOT rebuild or reupload the immutable world or prop meshes or recreate textures, immutable lighting resources, pipeline, or swapchain. The renderer SHALL NOT infer movement, locks, targeting, or feedback timing from gameplay. Per-frame changing geometry SHALL remain within the existing Vulkan baseline and frames-in-flight lifetime guarantees.

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
