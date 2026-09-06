## MODIFIED Requirements

### Requirement: Explicit runtime configuration
The application SHALL supply the runtime with an explicit resource root and optional explicit level path and entry identifier using backend-neutral configuration. Required runtime shaders, fixed prototype textures, and the static model SHALL resolve from that root independently of the process working directory. With no level override, the packaged prototype level SHALL resolve from that root; with an override, only the selected external level SHALL be required. The launcher SHALL derive its default resource root from the actual executable location rather than the textual form of the process invocation and SHALL resolve a relative level argument once before passing it to runtime composition. Entry resolution SHALL use the explicit identifier or the selected document's default and SHALL finish before level-dependent physics, player, or renderer construction. An invalid explicit selection SHALL NOT fall back to packaged content or another entry.

#### Scenario: Runtime starts from another working directory
- **WHEN** the executable starts with a valid resource root and either no level override or a resolved absolute level override while the process working directory is elsewhere
- **THEN** the runtime finds the selected level, shaders, fixed prototype textures, and packaged static GLB and starts normally at the resolved entry

#### Scenario: Launcher is invoked through an indirect path
- **WHEN** the project launcher is started through a search path, alias, or invocation string that does not contain the executable directory
- **THEN** it derives the resource root from the actual executable location and finds the copied runtime assets

#### Scenario: Configured resource is missing
- **WHEN** the selected level or a required shader, fixed prototype texture, or static GLB is absent
- **THEN** startup fails with an actionable error containing the resolved missing asset path

#### Scenario: Unselected packaged level is absent
- **WHEN** a valid explicit external level is selected and the packaged prototype file is absent but all selected dependencies exist
- **THEN** startup succeeds without requiring or loading the unselected packaged prototype

#### Scenario: Entry selection fails
- **WHEN** the requested entry cannot be resolved in the validated selected document
- **THEN** startup reports the path and identifier, constructs no level-dependent consumer, and releases already-created runtime owners in dependency-safe order
