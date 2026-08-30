## ADDED Requirements

### Requirement: Prototype solid-state presentation
The renderer SHALL accept backend-neutral highlighted-solid and dimmed-solid masks with each frame request and SHALL apply those states only to the corresponding authored prototype surfaces. Highlighting SHALL take visible precedence while active, dimming SHALL remain visible after highlighting ends, and unaffected surfaces SHALL retain their authored color and existing directional-plus-ambient lighting. Updating these masks SHALL NOT recreate or rewrite immutable scene geometry, recreate the graphics pipeline or swapchain, add a descriptor binding, or add another scene draw.

#### Scenario: Solid is highlighted
- **WHEN** a renderable frame identifies a prototype solid in the highlighted mask
- **THEN** the opaque scene draw presents that solid with the fixed visible highlight treatment

#### Scenario: Solid is dimmed
- **WHEN** a renderable frame identifies a prototype solid only in the dimmed mask
- **THEN** the opaque scene draw presents that solid with the fixed destroyed treatment

#### Scenario: Solid is highlighted and dimmed
- **WHEN** the same solid is present in both masks during its final-hit feedback interval
- **THEN** the highlight treatment takes precedence for that frame

#### Scenario: Presentation changes between frames
- **WHEN** highlighted or dimmed masks change in a later frame request
- **THEN** the next submitted scene draw uses the new presentation without changing immutable vertex data or renderer-lifetime resources

#### Scenario: Target presentation uses opaque depth and lighting
- **WHEN** highlighted, dimmed, and unaffected target surfaces are rendered
- **THEN** they remain in the existing depth-tested opaque scene draw and preserve orientation-readable lighting

