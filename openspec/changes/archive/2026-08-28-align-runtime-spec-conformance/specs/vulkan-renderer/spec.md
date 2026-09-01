## MODIFIED Requirements

### Requirement: Swapchain lifecycle
The renderer SHALL validate the current surface capabilities required for swapchain image usage and composite alpha, SHALL create a swapchain only with supported values, and SHALL recreate swapchain-dependent resources after resize, out-of-date, or suboptimal presentation without recreating instance- or device-lifetime resources.

#### Scenario: Required surface configuration is supported
- **WHEN** the surface supports color-attachment swapchain images and at least one usable composite-alpha mode
- **THEN** the renderer selects supported values and creates the swapchain from the current surface capabilities

#### Scenario: Required image usage is unavailable
- **WHEN** the surface does not support color-attachment usage for swapchain images
- **THEN** renderer startup fails before swapchain creation with an actionable message identifying the missing color-attachment capability

#### Scenario: Composite alpha selection is required
- **WHEN** the preferred opaque composite-alpha mode is unavailable but another supported mode exists
- **THEN** the renderer selects a supported fallback mode without requesting an unsupported value

#### Scenario: Window framebuffer is resized
- **WHEN** the framebuffer receives a new non-zero extent
- **THEN** the renderer waits for affected GPU work and recreates the swapchain and dependent image views for that extent

#### Scenario: Swapchain becomes out of date
- **WHEN** image acquisition or presentation reports an out-of-date swapchain
- **THEN** the current frame is abandoned safely and swapchain recreation occurs before the next rendered frame

#### Scenario: Swapchain becomes suboptimal
- **WHEN** image acquisition or presentation reports a suboptimal swapchain
- **THEN** the renderer completes safe work and schedules swapchain recreation without terminating the application
