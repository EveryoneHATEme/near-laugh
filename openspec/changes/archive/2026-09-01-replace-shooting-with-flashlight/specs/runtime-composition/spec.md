## ADDED Requirements

### Requirement: Gameplay-independent spot-light render request
The runtime SHALL supply at most one optional dynamic spot-light description containing only backend-neutral position, direction, range, cone, color, intensity, and enabled state. Frame requests and renderer interfaces SHALL NOT expose player, flashlight, weapon, target, health, damage, physics-hit, or other gameplay implementation types, and the renderer SHALL NOT infer a spot-light pose from the camera matrix.

#### Scenario: Active spot light is prepared for rendering
- **WHEN** runtime state selects an enabled spot light for a renderable frame
- **THEN** the frame request contains its current validated world-space lighting description without identifying its gameplay source

#### Scenario: No spot light is active
- **WHEN** runtime state selects no enabled dynamic spot light
- **THEN** the frame request represents no dynamic spot-light contribution without exposing gameplay state

#### Scenario: Runtime-render boundary is inspected
- **WHEN** frame and renderer-facing declarations are inspected
- **THEN** their spot-light contract contains only engine-owned scalar lighting data and no gameplay, physics-library, platform, or graphics-backend types

## REMOVED Requirements

### Requirement: Gameplay-free render request
**Reason**: Its target highlight/dim presentation payload is removed with shooting-target gameplay.

**Migration**: Use the gameplay-independent spot-light render request, which carries only source-agnostic lighting data.

### Requirement: Fixed-step shooting-range coordination
**Reason**: Rifle, hitscan, target damage, target feedback, and recoil simulation are removed from the runtime.

**Migration**: Advance the player directly during each complete fixed step, and coordinate flashlight press edges during input sampling rather than delaying them to a fixed simulation step.
