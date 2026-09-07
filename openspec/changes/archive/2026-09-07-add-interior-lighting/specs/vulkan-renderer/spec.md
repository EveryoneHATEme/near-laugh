## MODIFIED Requirements

### Requirement: Camera-transformed prototype scene rendering
The renderer SHALL load scene SPIR-V shaders and required selected model/material resources from the explicit package root, upload correctly described static triangle and sampled material data, apply the runtime camera and current bounded lighting, clear the swapchain image, and render all selected generated geometry and static placements with correct depth and OPAQUE/MASK coverage. World geometry SHALL include authored solids and all authored switches, terrain only when present, and separately supplied P03 changing opaque presentation. Materials SHALL NOT change door motion, targeting or frame ownership. The editor SHALL preview renderable safe document fields without requiring successful gameplay validation. Empty generated or prop streams SHALL omit their allocations/draws safely while retaining other renderable content and UI. Aggregate expanded vertex/image byte counts and draw ranges SHALL be checked for overflow and supported allocation/draw limits before upload.

#### Scenario: First visible scene frame
- **WHEN** renderer initialization succeeds with valid runtime resources and the window has a non-zero framebuffer extent
- **THEN** the application presents the selected material-assigned generated world, static placements and supplied doors from the supplied camera pose over the configured clear color

#### Scenario: Camera frame changes
- **WHEN** the renderer receives a different valid camera frame
- **THEN** the next submitted world and prop draws use the new view/projection transform without recreating the graphics pipeline, immutable mesh buffers, sampled texture resources, or swapchain

#### Scenario: Scene shader asset is unavailable
- **WHEN** a required scene SPIR-V shader cannot be read beneath the configured resource root or cannot be used to create a shader module
- **THEN** startup fails with an error identifying the resolved shader asset path and releases all previously created Vulkan resources

#### Scenario: Scene texture asset is unavailable or invalid
- **WHEN** a required selected material texture cannot be read or decoded beneath the configured resource root
- **THEN** startup fails with an error identifying the resolved texture asset path and releases all previously created Vulkan resources

#### Scenario: Static model asset is unavailable or invalid
- **WHEN** any required selected static GLB cannot be read, validated, converted, or uploaded
- **THEN** startup fails with an error identifying the resolved model asset path and releases all previously created Vulkan resources

#### Scenario: Interior has no terrain
- **WHEN** a valid interior is rendered in the game or editor
- **THEN** its slabs, walls, stairs, authored static placements, switch and doors appear through the existing depth-tested textured lighting path without an invented terrain surface

#### Scenario: Invalid interior has no generated geometry
- **WHEN** an editable terrain-free document has no solids and no switches
- **THEN** the editor still displays any renderable props/doors, markers, validation feedback, and UI without allocating a zero-sized world buffer or submitting an invalid draw

### Requirement: Per-frame authored point-light enable state
Each successfully presented scene frame SHALL apply its requested enabled state independently to every light in the bounded authored set. Disabling a point light SHALL remove its contribution from generated world, switch, and imported-prop surfaces without changing authored lighting resources, immutable geometry, ambient, texture behavior, or the optional spotlight. Changes SHALL remain valid with multiple frames in flight and SHALL NOT require additional device capabilities beyond the current Vulkan baseline.

#### Scenario: Light state alternates across frames
- **WHEN** successive frame requests alternate which authored point light is enabled
- **THEN** each presented frame uses its own requested state without stale values or modifying resources still used by an earlier frame

#### Scenario: Flashlight is disabled
- **WHEN** the frame contains no enabled spotlight and disables one authored point light
- **THEN** all other enabled authored point lights and ambient retain their contributions without depending on spotlight enable state

#### Scenario: Presentation recovers after a toggle
- **WHEN** resize, swapchain recovery, or minimize/restore occurs after a point light is disabled
- **THEN** the next presented frame uses the requested current light state and retained immutable scene resources without validation errors

#### Scenario: Zero lights or the full supported set is presented
- **WHEN** a valid frame presents no authored lights or eight lights with four configured shadow casters
- **THEN** each supplied enable affects only its matching light and no disabled or out-of-profile entry is sampled

### Requirement: Bounded changing opaque presentation
The renderer SHALL present the bounded changing opaque geometry supplied for each scene frame, including this milestone's generated door leaves and temporary feedback geometry, with correct world-space normals, opaque depth visibility, existing textures, and the current point-light, ambient, and optional spotlight state. Supported point-light shadows SHALL use the same changing geometry as the color presentation for that submitted frame, including accepted obstructed door poses. Changing a door pose or feedback SHALL NOT rebuild or reupload the immutable world or prop meshes or recreate textures, immutable lighting resources, pipeline, or swapchain. The renderer SHALL NOT infer movement, locks, targeting, or feedback timing from gameplay. Per-frame changing geometry SHALL remain within the existing Vulkan baseline and frames-in-flight lifetime guarantees.

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

## ADDED Requirements

### Requirement: Owned bounded point-light shadow presentation
The renderer SHALL implement the interior-lighting shadow profile for game and editor using the supported Vulkan baseline. Static geometry and current changing geometry SHALL share their presented material coverage with shadow occlusion, including MASK cutouts independent of collision proxies. Mutable lighting and shadow resources SHALL not be overwritten, transitioned for conflicting use or destroyed before dependent GPU work completes. Shadow writes SHALL become visible before scene sampling. Switching lights SHALL require neither allocation of additional shadow capacity nor recreation of static scene resources. Recovery SHALL preserve usable shadow resources and use the next frame's current state. Device/profile or packaged shader failures SHALL produce actionable errors and release partial resources without silently replacing required shadows with unshadowed light.

#### Scenario: Frame slot containing shadows is reused
- **WHEN** a submitted frame used changing doors and shadow resources and its slot is reused
- **THEN** earlier work has completed before mutation and the new frame's shadows and visible geometry agree without validation errors

#### Scenario: Disabled shadowed light is enabled
- **WHEN** a configured shadow-casting point light changes from disabled to enabled
- **THEN** its next displayed contribution includes current occlusion without allocating shadow capacity during the switch action

#### Scenario: Masked prop casts a shadow
- **WHEN** a shadowed key light illuminates the supported phone cord while its collision boxes are empty
- **THEN** covered material regions occlude light and alpha-discarded regions do not fill the entire mesh footprint with shadow

#### Scenario: Shadow construction fails
- **WHEN** a required shadow shader, image, allocation, view, descriptor or pipeline cannot be created
- **THEN** startup or editor replacement reports the failed operation and releases acquired resources exactly once; failed editor replacement retains the previous coherent preview with a stale diagnostic

#### Scenario: Presentation recovers with moving shadows
- **WHEN** the game or editor resizes, minimizes/restores or recreates presentation while shadowed doors/lights are changing
- **THEN** the next submitted shadow and scene presentation uses one coherent state and validation includes final resource teardown
