## ADDED Requirements

### Requirement: Per-frame spot-light presentation
The renderer SHALL accept at most one valid optional source-independent spot-light description with each frame request and SHALL apply changes in its pose, parameters, or enabled state to the next submitted opaque scene draw. Updating the spot light SHALL NOT recreate the graphics pipeline, swapchain, immutable scene geometry, sampled textures, or immutable point-light resources; SHALL NOT add another scene draw; and SHALL remain safe for the current frames-in-flight model.

#### Scenario: Spot light changes between frames
- **WHEN** a later frame supplies a different valid spot-light pose, parameters, or enabled state
- **THEN** the next submitted scene draw uses the new value without recreating renderer-lifetime or swapchain resources

#### Scenario: Spot light is disabled
- **WHEN** a frame supplies no enabled dynamic spot light
- **THEN** the existing immutable point-light and ambient scene shading is rendered without a dynamic cone contribution

#### Scenario: Spot-light source is not the player
- **WHEN** a valid frame supplies a spot-light pose produced by another runtime object
- **THEN** the renderer shades from that supplied pose without requiring a camera-mounted or flashlight-specific path

## REMOVED Requirements

### Requirement: Prototype solid-state presentation
**Reason**: Per-target highlighting and destroyed dimming belonged to the removed shooting-target gameplay.

**Migration**: Remove highlighted/dimmed solid masks and render all inert plates through the ordinary textured lighting path; use the freed per-frame scene-data capacity for the optional source-independent spot light.

