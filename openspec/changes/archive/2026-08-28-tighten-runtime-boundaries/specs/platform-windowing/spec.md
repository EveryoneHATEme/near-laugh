## MODIFIED Requirements

### Requirement: Desktop window lifecycle
The runtime SHALL initialize its desktop window system exactly once, SHALL allow a window to be created only while that platform owner is active, SHALL create one Vulkan-compatible window without an OpenGL context, and SHALL destroy the window before releasing the window system.

#### Scenario: Successful startup and shutdown
- **WHEN** the application starts on a supported desktop system and required window services are available
- **THEN** it creates one Vulkan-compatible window under the active platform owner and destroys the window before platform shutdown

#### Scenario: Window is requested without platform lifetime
- **WHEN** window creation is attempted without an active platform owner
- **THEN** construction is rejected with an actionable lifetime error before a native window is requested

#### Scenario: Window initialization failure
- **WHEN** the desktop window system or window cannot be initialized
- **THEN** startup terminates with an actionable error and does not continue into renderer initialization

### Requirement: Keyboard and mouse state
The platform integration SHALL expose engine-owned physical keyboard-key state, physical mouse-button state, cursor movement, and cursor-capture control. It SHALL NOT assign FPS gameplay meaning to those physical inputs.

#### Scenario: Input is sampled
- **WHEN** the application samples input after processing the current event batch
- **THEN** it receives current engine-owned physical key and mouse-button states plus cursor movement accumulated for that batch

#### Scenario: First-person cursor capture
- **WHEN** the application enables first-person mouse input
- **THEN** the cursor is captured and relative movement remains available without exposing a native window handle to runtime consumers

### Requirement: Platform dependency boundary
Gameplay-facing and runtime-facing code SHALL NOT include or exchange SDL, GLFW, native-window, platform-specific, or Vulkan types. Platform-library types SHALL remain confined to the platform implementation, Vulkan surface integration SHALL be visible only to the internal platform-renderer boundary, and SDL libraries and headers SHALL NOT be required by runtime or test targets.

#### Scenario: Dependency boundary is inspected
- **WHEN** public runtime headers, project targets, and platform implementation files are inspected
- **THEN** runtime consumers expose no backend types, GLFW remains confined to the platform implementation, and Vulkan surface types do not propagate into gameplay-facing code
