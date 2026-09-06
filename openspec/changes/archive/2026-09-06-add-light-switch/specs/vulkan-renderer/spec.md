## ADDED Requirements

### Requirement: Per-frame authored point-light enable state
Each successfully presented scene frame SHALL apply its requested enabled state independently to both authored point lights. Disabling a point light SHALL remove its contribution from generated world, switch, and imported-prop surfaces without changing authored lighting resources, immutable geometry, ambient, texture behavior, or the optional spotlight. Changes SHALL remain valid with multiple frames in flight and SHALL NOT require additional device capabilities beyond the current Vulkan baseline.

#### Scenario: Light state alternates across frames
- **WHEN** successive frame requests alternate which authored point light is enabled
- **THEN** each presented frame uses its own requested state without stale values or modifying resources still used by an earlier frame

#### Scenario: Flashlight is disabled
- **WHEN** the frame contains no enabled spotlight and disables one authored point light
- **THEN** the other authored point light and ambient retain their contributions without depending on spotlight enable state

#### Scenario: Presentation recovers after a toggle
- **WHEN** resize, swapchain recovery, or minimize/restore occurs after a point light is disabled
- **THEN** the next presented frame uses the requested current light state and retained immutable scene resources without validation errors
