## MODIFIED Requirements

### Requirement: Explicit runtime configuration
The application SHALL supply the runtime with an explicit resource root, all required runtime assets SHALL be resolved from that root independently of the process working directory, and the project launcher SHALL derive its default resource root from the actual executable location rather than from the textual form of the process invocation.

#### Scenario: Runtime starts from another working directory
- **WHEN** the executable starts with a valid resource root while the process working directory is elsewhere
- **THEN** the renderer finds the required shader assets and starts normally

#### Scenario: Launcher is invoked through an indirect path
- **WHEN** the project launcher is started through a search path, alias, or invocation string that does not contain the executable directory
- **THEN** it derives the resource root from the actual executable location and finds the copied runtime assets

#### Scenario: Configured resource is missing
- **WHEN** a required shader is absent beneath the configured resource root
- **THEN** startup fails with an actionable error containing the resolved asset path

### Requirement: Renderer outcome coordination
Each frame request SHALL produce an engine-owned outcome that distinguishes rendered work, temporarily skipped work, and swapchain recovery. The runtime SHALL consume that outcome before deciding how the application loop proceeds and SHALL retain event processing and application lifetime ownership without inspecting Vulkan results.

#### Scenario: Frame is rendered
- **WHEN** image acquisition, submission, and presentation succeed
- **THEN** the runtime consumes a rendered outcome and proceeds to the next runtime-controlled loop iteration

#### Scenario: Rendering is temporarily unavailable
- **WHEN** rendering cannot proceed because the surface extent is zero or swapchain recovery is required
- **THEN** the runtime consumes a non-fatal skipped or recovered outcome and retains control of event processing and application lifetime

