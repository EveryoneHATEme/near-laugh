## Why

The runtime boundary refactor established the intended module and ownership structure, but several edge paths still do not fulfill the resulting contracts. Input received while waiting on a minimized window can lose look movement, renderer outcomes are returned but not consumed by the runtime, launcher resource discovery is not reliably executable-relative, and swapchain creation assumes surface capabilities that were not validated.

## What Changes

- Preserve one coherent physical-input event batch across both polling and blocking event waits so cursor movement is sampled exactly once instead of being discarded.
- Make the Engine consume every renderer frame outcome and explicitly retain application-loop ownership for rendered, skipped, and recovered results.
- Resolve the launcher's resource root from the actual executable location rather than relying on the spelling of `argv[0]`, while keeping runtime asset lookup independent of the process working directory.
- Validate the surface usage and composite-alpha capabilities required by swapchain creation and report the specific missing capability before attempting an unsupported configuration.
- Strengthen deterministic and boundary tests so they exercise these runtime interactions rather than only the individual value types or resource resolver.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `runtime-composition`: Clarify observable runtime consumption of frame outcomes and require launcher configuration to use the actual executable layout independently of `argv[0]` and the process working directory.
- `platform-windowing`: Define event-batch behavior for blocking waits so physical state and cursor movement produced by the waited event batch remain available for one runtime sample.
- `vulkan-renderer`: Require explicit validation and actionable rejection of surface capabilities needed for color-attachment swapchain images and composite alpha.

## Impact

Affected areas are the Engine main-thread loop, the GLFW window event-batch boundary, launcher resource-root discovery, Vulkan swapchain capability selection, and focused runtime/platform/renderer tests. Public runtime types and controls remain unchanged, no new third-party dependency or abstraction layer is introduced, and the existing `near_laugh_platform`, `near_laugh_render`, and `near_laugh_runtime` target boundaries remain intact.
