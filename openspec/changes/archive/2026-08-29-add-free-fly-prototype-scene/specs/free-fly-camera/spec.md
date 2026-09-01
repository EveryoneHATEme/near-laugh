## Purpose

Defines the runtime-owned perspective camera used to inspect the built-in 3D prototype scene without introducing a physical player controller.

## ADDED Requirements

### Requirement: Perspective camera frame
The runtime SHALL maintain one free-fly camera with position, yaw, pitch, vertical field of view, and near/far clipping state, and SHALL produce backend-neutral view/projection data using the current non-zero framebuffer aspect ratio.

#### Scenario: Initial camera frame
- **WHEN** the prototype starts with a non-zero framebuffer extent
- **THEN** the runtime supplies a perspective camera frame whose initial pose presents the built-in scene in front of the viewer

#### Scenario: Framebuffer aspect changes
- **WHEN** the framebuffer changes to a different non-zero aspect ratio
- **THEN** the next camera frame uses that aspect ratio without stretching the rendered scene

### Requirement: Free-fly navigation
While the cursor is captured, the camera SHALL translate relative to its current orientation from the existing forward, backward, left, and right FPS actions, SHALL use jump and crouch actions for vertical movement, SHALL increase translation speed while sprint is active, and SHALL rotate from look delta. Translation SHALL scale with bounded elapsed time, combined translation axes SHALL NOT increase the configured movement speed, and pitch SHALL remain limited so the camera cannot overturn.

#### Scenario: Camera translates relative to view
- **WHEN** forward input remains active for a renderable interval while the camera is rotated away from its initial yaw
- **THEN** the camera advances in its current horizontal forward direction by the configured speed multiplied by the bounded elapsed time

#### Scenario: Camera moves vertically
- **WHEN** jump or crouch input is active while the cursor is captured
- **THEN** the camera moves vertically without applying gravity, jumping, or ground constraints

#### Scenario: Mouse look updates orientation
- **WHEN** captured cursor movement is sampled
- **THEN** yaw and pitch change according to the configured sensitivity and pitch remains within its configured limits

#### Scenario: Diagonal movement is normalized
- **WHEN** multiple translation axes are active during the same update
- **THEN** the resulting translation magnitude does not exceed the selected normal or sprint speed for that elapsed time

#### Scenario: Runtime resumes after waiting
- **WHEN** rendering resumes after the window spent time minimized or otherwise blocked in an event wait
- **THEN** the waiting duration is not applied as a large camera translation on the next rendered frame

### Requirement: Prototype cursor capture
The prototype runtime SHALL begin with the cursor captured, SHALL release it while the menu action is active, SHALL recapture it from the primary action, and SHALL reset relative-look tracking across each capture transition. Camera navigation SHALL remain inactive while the cursor is released.

#### Scenario: Cursor is released
- **WHEN** the menu action is active while the cursor is captured
- **THEN** the cursor becomes available for normal desktop interaction and camera navigation stops

#### Scenario: Cursor is recaptured
- **WHEN** the primary action is active while the cursor is released
- **THEN** the cursor is captured again without applying cursor movement accumulated before or during the transition as camera look

