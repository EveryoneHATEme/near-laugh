# platform-windowing Specification

## Purpose

Defines the desktop window, event, keyboard, mouse, and Vulkan-surface behavior required by the single-player game runtime without exposing platform-library details to player code.

## Requirements

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
The platform integration SHALL expose project-owned physical keyboard-key state, physical mouse-button state, cursor movement, and cursor-capture control. Every polling or blocking event-processing operation that dispatches input SHALL form a distinct event batch whose cursor movement remains available until the runtime samples that batch exactly once. Starting the next event batch SHALL reset look movement without clearing held physical state. The platform integration SHALL NOT assign gameplay-action meaning to those physical inputs.

#### Scenario: Input is sampled
- **WHEN** the application samples input after processing a polling event batch
- **THEN** it receives current project-owned physical key and mouse-button states plus cursor movement accumulated for that batch

#### Scenario: Blocking wait dispatches input
- **WHEN** a blocking platform wait returns after dispatching keyboard, mouse-button, or cursor events
- **THEN** the runtime can sample the physical state and cursor movement from that waited event batch before the next batch resets its look movement

#### Scenario: Next event batch begins
- **WHEN** the runtime begins processing the event batch after a previously sampled batch
- **THEN** cursor movement starts at zero while held keyboard and mouse-button state remains active

#### Scenario: First-person cursor capture
- **WHEN** the application enables first-person mouse input
- **THEN** the cursor is captured and relative movement remains available without exposing a native window handle to runtime consumers

### Requirement: Platform dependency boundary
Gameplay-facing and runtime-facing code SHALL NOT include or exchange SDL, GLFW, native-window, platform-specific, or Vulkan types. Platform-library types SHALL remain confined to the platform implementation, Vulkan surface integration SHALL be visible only to the internal platform-renderer boundary, and SDL libraries and headers SHALL NOT be required by runtime or test targets.

#### Scenario: Dependency boundary is inspected
- **WHEN** public runtime headers, project targets, and platform implementation files are inspected
- **THEN** runtime consumers expose no backend types, GLFW remains confined to the platform implementation, and Vulkan surface types do not propagate into gameplay-facing code
