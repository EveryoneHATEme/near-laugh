## ADDED Requirements

### Requirement: Game text overlay presentation
The runtime renderer SHALL draw supplied bounded resolved caption text over the scene with the layout and glyph support required by game-text-presentation. Text SHALL remain readable independently of scene lighting and depth, and SHALL NOT change scene lighting, depth coverage or existing geometry. The renderer SHALL NOT resolve cue identities, start playback or advance caption time. Empty text SHALL require no text draw. Unsupported text or invalid font resources SHALL produce actionable errors instead of unsafe indexing or unbounded allocation.

#### Scenario: Text overlays a dark or occluded scene
- **WHEN** a frame supplies valid caption text while the camera faces dark geometry
- **THEN** its glyphs and backing are visible over the scene without being occluded by world depth or darkened by scene lights

#### Scenario: No caption is active
- **WHEN** a frame supplies no text
- **THEN** the scene renders normally without an empty or invalid glyph draw

### Requirement: Text GPU resource lifetime
Font atlas and descriptor resources SHALL outlive dependent draws and survive compatible swapchain recovery without repeated font decoding or atlas upload. Mutable glyph data SHALL obey existing frame-slot completion guarantees. Attachment format changes SHALL recreate compatible text pipelines safely. Failed initialization SHALL release partial owners once, and teardown SHALL wait for dependent GPU work before releasing text resources and the device. Game text SHALL NOT require editor UI code or expand mandatory Vulkan device features.

#### Scenario: Captions change with multiple frames in flight
- **WHEN** successive frames display different strings while prior frames remain in use
- **THEN** each submitted frame retains its own valid glyph data without overwriting GPU-owned storage

#### Scenario: Text presentation recovers
- **WHEN** resize, minimize/restore or swapchain recovery occurs while captions are active
- **THEN** the next submitted frame presents current supplied text with retained compatible font resources and no validation errors

#### Scenario: Text resource construction fails
- **WHEN** atlas upload, descriptor allocation or text pipeline creation fails partway
- **THEN** the failed operation is identified and all successfully acquired resources are released once without disturbing other owners' lifetimes
