# platform-windowing Specification

## Purpose

Defines the desktop window, event, keyboard, mouse, and Vulkan-surface behavior required by the single-player FPS runtime without exposing platform-library details to gameplay code.

## Requirements

### Requirement: Desktop window lifecycle
The runtime SHALL initialize its desktop window system exactly once, create one Vulkan-compatible window without an OpenGL context, and release the window system after all dependent objects have been destroyed.

#### Scenario: Successful startup and shutdown
- **WHEN** the application starts on a supported desktop system and required window services are available
- **THEN** it creates one Vulkan-compatible window and destroys it cleanly during shutdown

#### Scenario: Window initialization failure
- **WHEN** the desktop window system or window cannot be initialized
- **THEN** startup terminates with an actionable error and does not continue into renderer initialization

### Requirement: Vulkan surface integration
The platform integration SHALL provide the Vulkan instance extensions required by the active desktop environment and SHALL create a presentation surface compatible with the application window.

#### Scenario: Required extensions are requested
- **WHEN** the renderer prepares Vulkan instance creation
- **THEN** every instance extension required for desktop surface creation is included in the request

#### Scenario: Surface creation fails
- **WHEN** a Vulkan presentation surface cannot be created for the window
- **THEN** startup terminates with an actionable error and all already-created platform resources are released

### Requirement: Event and close handling
The runtime SHALL process desktop events during each application-loop iteration and SHALL terminate the loop after the window receives a close request.

#### Scenario: Close request
- **WHEN** the user requests that the application window close
- **THEN** the application stops submitting new frames and begins orderly shutdown

### Requirement: Framebuffer extent reporting
The platform integration SHALL report framebuffer dimensions independently of logical window dimensions and SHALL identify zero-sized framebuffers caused by minimization.

#### Scenario: Display scaling changes framebuffer size
- **WHEN** the framebuffer size differs from the logical window size
- **THEN** the renderer receives the framebuffer dimensions for swapchain extent selection

#### Scenario: Window is minimized
- **WHEN** the framebuffer width or height becomes zero
- **THEN** the runtime waits for a non-zero framebuffer extent without continuously submitting or recreating render work

### Requirement: Keyboard and mouse state
The platform integration SHALL expose keyboard keys, mouse buttons, cursor movement, and cursor-capture control needed by one local first-person player.

#### Scenario: Input is sampled
- **WHEN** the application samples input after processing the current event batch
- **THEN** it receives the current keyboard and mouse-button states plus cursor movement accumulated for that batch

#### Scenario: First-person cursor capture
- **WHEN** the application enables first-person mouse input
- **THEN** the cursor is captured and relative movement remains available without exposing a native window handle to gameplay code

### Requirement: Platform dependency boundary
Gameplay-facing code SHALL NOT include or exchange SDL, GLFW, native-window, or platform-specific types. SDL libraries and SDL headers SHALL NOT be required by any runtime or test target after migration.

#### Scenario: Dependency boundary is inspected
- **WHEN** project headers, runtime targets, and test targets are inspected after migration
- **THEN** no SDL dependency remains and platform-library types are confined to the platform implementation boundary
