## Context

See `proposal.md` for motivation. The existing target split and ownership order are correct and must remain unchanged. The gaps are confined to coordination at four concrete boundaries: GLFW event dispatch versus input sampling, renderer outcomes versus the Engine loop, process invocation versus runtime layout discovery, and queried surface capabilities versus swapchain create values.

The runtime is intentionally single-threaded and purpose-built for one local FPS player. The correction must not introduce a generic renderer interface, platform framework, event bus, or additional backend abstraction solely to make these paths testable.

## Goals / Non-Goals

**Goals:**

- Preserve cursor movement dispatched by either polling or blocking event processing until the runtime samples that batch once.
- Make frame-outcome handling explicit and exhaustive in runtime-owned code.
- Derive the copied resource directory from the actual process executable on supported desktop hosts.
- Select only swapchain usage and composite-alpha values supported by the current surface.
- Add focused regression tests and source-boundary checks that would fail if the original gaps returned.

**Non-Goals:**

- Changing public runtime controls, input mappings, frame outcome values, or target dependencies.
- Adding gameplay updates while minimized or changing the renderer's ownership of swapchain recovery.
- Generalizing process layout discovery for arbitrary applications or adding unsupported platform targets.
- Expanding renderer scope beyond the current Vulkan triangle foundation.

## Decisions

### 1. Treat each blocking wait as an input event batch

The window layer will begin a fresh input batch immediately before either polling or blocking event dispatch. After a blocking wait returns, the Engine will sample and map that waited batch before any later poll can begin another batch. Held key and button state will remain in the accumulator, while only per-batch cursor delta is reset at the next batch boundary.

This keeps batch ownership beside the GLFW dispatch operation and keeps FPS semantics in `FpsInputMapper`. Resetting cursor delta at the start of the next unconditional `pollEvents` without sampling the waited batch was rejected because it recreates the current data loss. Accumulating indefinitely until a later renderable frame was rejected because it merges distinct event batches and can produce a large synthetic look jump after restore.

### 2. Consume frame outcomes through an exhaustive runtime policy

The Engine will store each returned `FrameOutcome` and pass it through a small runtime-owned exhaustive decision function or switch before completing the iteration. The current policy continues after `Rendered`, `Skipped`, and `Recovered`; recovery remains renderer-owned, and every next iteration still starts with runtime-owned event processing.

The outcome policy will be directly unit tested for every enumerator, while a source-boundary check will ensure the Engine no longer discards `renderFrame` results. Introducing an `IRenderer` solely to mock one implementation was rejected because it would add a hypothetical rendering abstraction contrary to project scope. Requesting zero-extent frames merely to make `Skipped` occur in the Engine was also rejected because minimized-window waiting already avoids unnecessary renderer calls.

### 3. Resolve runtime layout with a launcher-local native helper

The launcher will obtain its own executable path from the native process facility available on each desktop host currently supported by the build, normalize the result, and derive the adjacent `resources` directory from it. Windows will use the wide-character module-path API; other supported desktop implementations will use their existing native process-path facility. The helper will remain private to launcher/runtime startup code and expose only `std::filesystem::path`.

`argv[0]` may remain useful in diagnostics but will not be the source of truth for resource layout. Searching the process `PATH` was rejected because aliases and caller-controlled argument text still do not identify the running module reliably. A compile-time absolute build resource path was rejected because it would break copied or installed runtime layouts.

A focused process-level test executable will exercise the same helper from a different working directory and verify executable-adjacent resource discovery without initializing a window or Vulkan.

### 4. Select swapchain values from queried capabilities

Swapchain preparation will first verify that `VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT` is present in the surface's supported usage flags. Composite alpha will be selected by a small deterministic helper that prefers opaque and otherwise chooses a supported fallback from the Vulkan-defined modes used by the current desktop renderer. If no required usage or usable mode is available, startup will fail before `vkCreateSwapchainKHR` with the missing capability named in the error.

The selection helpers will operate on Vulkan flag values and receive focused unit coverage. Adding a generalized swapchain policy object was rejected because there is one Vulkan renderer and one concrete presentation requirement.

### 5. Keep regression seams narrow and internal

Deterministic tests will cover input-batch reset and sampling order, exhaustive frame-outcome policy, launcher path discovery, and surface capability selection. Existing boundary checks will be extended where a static invariant is more direct, such as prohibiting discarded renderer outcomes. Vulkan smoke coverage will continue to verify real swapchain recreation and validation-clean teardown; it will not replace deterministic tests for pure selection rules.

## Risks / Trade-offs

- **[A blocking wait can return for a non-input event]** -> The resulting batch legitimately has zero cursor delta and unchanged held state; sampling it remains harmless and preserves one consistent rule.
- **[Native executable discovery needs host-specific code]** -> Keep the code launcher-local, return only a standard path, and implement only hosts already supported by the project build.
- **[All current frame outcomes have the same continue policy]** -> Retain an exhaustive switch and regression check so future outcome additions cannot silently bypass runtime coordination.
- **[A non-opaque composite-alpha fallback can compose differently]** -> Use opaque whenever available and otherwise prefer a deterministic supported mode rather than submitting an invalid swapchain request.
- **[Process-level path tests add a small test target]** -> Keep the target limited to the launcher helper and resource-layout assertion without GLFW or Vulkan startup.

## Migration Plan

1. Add focused tests for the desired batch, outcome, executable-layout, and surface-selection behavior.
2. Correct event-batch sampling and explicit Engine outcome handling without changing public runtime APIs.
3. Replace `argv[0]` resource discovery with the launcher-local native helper and update launcher-focused tests and documentation.
4. Validate required swapchain usage and select supported composite alpha before creation.
5. Run deterministic boundary/unit tests, then the Vulkan smoke preset and strict OpenSpec validation.

There is no persisted data or deployment migration. Rollback is a source-level reversal of these localized coordination changes.
