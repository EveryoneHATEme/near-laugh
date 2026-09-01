## MODIFIED Requirements

### Requirement: Explicit runtime configuration
The application SHALL supply the runtime with an explicit resource root, all required runtime shader and fixed prototype-texture assets SHALL be resolved from that root independently of the process working directory, and the project launcher SHALL derive its default resource root from the actual executable location rather than from the textual form of the process invocation.

#### Scenario: Runtime starts from another working directory
- **WHEN** the executable starts with a valid resource root while the process working directory is elsewhere
- **THEN** the renderer finds the required shader and fixed prototype-texture assets and starts normally

#### Scenario: Launcher is invoked through an indirect path
- **WHEN** the project launcher is started through a search path, alias, or invocation string that does not contain the executable directory
- **THEN** it derives the resource root from the actual executable location and finds the copied runtime assets

#### Scenario: Configured resource is missing
- **WHEN** a required shader or fixed prototype texture is absent beneath the configured resource root
- **THEN** startup fails with an actionable error containing the resolved missing asset path
