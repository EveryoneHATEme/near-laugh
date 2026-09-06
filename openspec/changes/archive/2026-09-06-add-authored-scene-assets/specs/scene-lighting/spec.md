## MODIFIED Requirements

### Requirement: Packaged lighting shaders
The lit scene SHALL execute from the explicit runtime resource root using the selected level, scene shaders and referenced model/material resources. It SHALL retain the two authored point lights, their independent enabled state, ambient and optional spotlight behavior for all generated geometry, doors and surviving OPAQUE/MASK prop fragments. It SHALL NOT require a general material framework, a separate lighting data file, or the raw source pack. Base-color materials SHALL NOT implicitly add emission, roughness/specular lighting or extra lights.

#### Scenario: Executable-relative resources are complete
- **WHEN** the launcher supplies a valid executable-relative resource root containing the selected level, lit scene shaders, and all referenced model/material resources
- **THEN** renderer startup can construct the lit textured scene pipeline and its selected generated, prop and door draws without consulting the process working directory or unrelated graphics assets
