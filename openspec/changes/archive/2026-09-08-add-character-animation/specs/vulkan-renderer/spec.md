## ADDED Requirements

### Requirement: Bounded animated scene presentation
The renderer SHALL present zero through four selected animated instances using their supplied pose and world placement, correct finite transformed normals and per-primitive base colors under the existing depth-tested diffuse lighting. Immutable geometry/skin/material data SHALL be shared by instances of one model and loaded only during scene preparation. Changing pose-derived geometry SHALL have separate bounded ownership from static geometry and door presentation. Every submitted color and shadow draw SHALL consume the same evaluated pose; skinning SHALL NOT run separately against different time samples in different passes. Empty character sets SHALL require no character allocations or draws. Aggregate byte and draw ranges SHALL be checked before use.

#### Scenario: Two instances use different clips
- **WHEN** two instances share the mannequin asset but supply different clip poses and placements
- **THEN** each deforms independently with its own material ranges and coherent lighting without reloading the model

#### Scenario: Static-only scene is shown
- **WHEN** the scene has no selected animated instances
- **THEN** existing static and changing door rendering works with no required character file or empty character draw

### Requirement: Animated resource lifetime and recovery
Any resource mutated for character presentation SHALL be modified only after its dependent GPU work completes. Immutable character resources SHALL survive swapchain recovery; format-dependent presentation resources SHALL recreate safely. Partial startup SHALL release every acquired resource once. Editor candidate replacement SHALL install animated and static/material/lighting data coherently or retain the previous complete preview with an actionable stale diagnostic. Validation SHALL remain active through final character GPU teardown.

#### Scenario: Frame slot is reused during a transition
- **WHEN** a character pose changes while prior frames remain in flight
- **THEN** no storage used by unfinished draws is overwritten and subsequent shadow/color passes agree on the new pose

#### Scenario: Recovery or replacement fails
- **WHEN** resize recovery or a candidate character allocation fails
- **THEN** failure follows the existing recoverable-preview or orderly-runtime-cleanup path without leaked resources or mismatched old/new character data
