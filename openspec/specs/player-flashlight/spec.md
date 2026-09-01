# player-flashlight Specification

## Purpose

Defines the local player's camera-mounted flashlight state, primary-action toggle behavior, and production of a reusable spot-light frame.

## Requirements

### Requirement: Edge-triggered flashlight toggle
The runtime SHALL maintain exactly one player flashlight that starts disabled and SHALL toggle its enabled state once when a newly pressed primary action is processed while first-person controls are active. Holding the primary action SHALL NOT toggle repeatedly, and a release SHALL be required before a later press can toggle the flashlight again.

#### Scenario: Flashlight is enabled
- **WHEN** the disabled flashlight observes a new primary-action press while first-person controls are active
- **THEN** it becomes enabled for the next frame request

#### Scenario: Primary action remains held
- **WHEN** the primary action remains active across later event batches
- **THEN** the flashlight retains its current enabled state without toggling again

#### Scenario: Flashlight is disabled
- **WHEN** the enabled flashlight observes a new primary-action press after the prior press was released
- **THEN** it becomes disabled for the next frame request

### Requirement: Cursor-recapture suppression
A primary-action press used to recapture the released cursor SHALL capture the cursor without toggling the flashlight. The press SHALL remain suppressed until the primary action is released, so holding the recapture click into a later captured iteration does not produce a delayed toggle.

#### Scenario: Released cursor is recaptured
- **WHEN** the cursor is released and the primary action becomes active
- **THEN** the cursor is captured and the flashlight retains its previous enabled state

#### Scenario: Recapture action remains held
- **WHEN** the primary action remains held after recapturing the cursor
- **THEN** the flashlight remains unchanged until a release followed by a new captured press

### Requirement: Camera-mounted spot-light presentation
While enabled, the player flashlight SHALL produce one valid source-independent spot-light description whose origin uses the interpolated rendered eye position and whose direction matches the current first-person look orientation. Movement, stance, and look changes SHALL update the next frame's spot-light pose consistently with the rendered camera, while the flashlight's range, cone, color, and intensity remain fixed prototype values.

#### Scenario: Enabled player moves or looks
- **WHEN** player movement, stance, interpolation, or look orientation changes while the flashlight is enabled
- **THEN** the next renderable frame receives a spot light aligned with that frame's rendered eye position and look direction

#### Scenario: Flashlight is disabled
- **WHEN** the flashlight is disabled
- **THEN** the player supplies no active spot-light contribution to the next frame request
