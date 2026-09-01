## Context

The SDL-to-GLFW/Vulkan migration established the required Vulkan 1.3 rendering foundation, but the current runtime is still assembled as one broad `engine_lib` target. Public engine headers include platform and renderer implementation headers, Vulkan is a public link dependency, `Renderer` participates in window-event decisions, platform callbacks produce gameplay actions directly, and shader lookup depends on the process working directory.

This change tightens those boundaries without turning the project into a general-purpose engine. The target remains one Windows desktop, single-player FPS with one GLFW window and one Vulkan renderer. The design deliberately uses concrete modules and data structures rather than service registries, backend interfaces, or extension systems.

## Goals / Non-Goals

**Goals:**

- Expose a small runtime API whose headers and transitive usage requirements do not expose GLFW or Vulkan.
- Make the engine runtime the single owner of event polling, close/minimize handling, input translation, and the decision to request a rendered frame.
- Make `Platform -> Window -> Renderer` construction and destruction order explicit and safe during partial initialization failures.
- Separate physical keyboard/mouse state from the fixed FPS action state consumed by gameplay.
- Resolve runtime resources from an explicit root rather than the current working directory.
- Turn Vulkan validation errors into observable smoke-test failures, including errors emitted during teardown.
- Keep failure injection and other test seams out of the public runtime API.

**Non-Goals:**

- Supporting another window system, rendering API, platform, game genre, or input-remapping framework.
- Introducing an RHI, render graph, ECS, job system, plugin system, scripting, custom allocator, or dependency-injection container.
- Separating pipeline and mesh responsibilities; that remains a future renderer-focused change.
- Adding a fixed-timestep gameplay loop, new rendering features, or asynchronous rendering.

## Decisions

### 1. Use one backend-neutral runtime facade with PImpl

The executable will consume a small public runtime surface containing `RuntimeConfig` and the application/runtime entry object. The entry object will own a `std::unique_ptr<Impl>` and define its destructor out of line. Its public headers will use standard-library types only and will not include platform or renderer headers.

`RuntimeConfig` will contain the initial window configuration and an explicit `std::filesystem::path resource_root`. This is configuration for this concrete FPS runtime, not a generic property bag or service locator.

The alternative of publishing abstract `IWindow` and `IRenderer` interfaces was rejected because the project has one supported implementation of each and such interfaces would create an unsupported backend abstraction.

### 2. Split the build into three concrete internal modules

CMake will define three meaningful implementation targets:

- `near_laugh_platform` owns GLFW lifetime, the window, and physical input collection.
- `near_laugh_render` owns Vulkan initialization and frame rendering and privately uses the internal platform/Vulkan surface bridge.
- `near_laugh_runtime` owns application composition, the main loop, and FPS input mapping, and privately depends on the platform and renderer targets.

The executable will link the runtime API. GLFW and Vulkan can remain necessary final link dependencies, but they must not appear in the runtime target's public headers or transitive public usage requirements. No generic `core` catch-all target will be introduced.

Keeping one library target was rejected because it cannot express or automatically check the intended dependency direction. Splitting every source directory into a target was also rejected because it would add build machinery without adding a meaningful boundary.

### 3. Keep the normal window API Vulkan-free and use an internal surface bridge

`Window` will keep GLFW details private and will no longer expose Vulkan types or Vulkan-specific operations in its normal API. A narrowly scoped internal bridge used by the platform and renderer modules will provide the required instance extensions and surface creation for the concrete GLFW/Vulkan pairing.

The bridge is intentionally not an RHI or general native-handle API. It exists solely to confine the unavoidable connection between the two selected backends.

Allowing `Window` to continue exposing `VkSurfaceKHR` was rejected because that makes all window consumers depend on Vulkan even when they only need events, extent, or close state.

### 4. Express `Platform -> Window` lifetime through construction

`Window` will require a live `Platform&` in its constructor and retain it as a non-owning lifetime dependency. Runtime member declaration order will be `Platform`, `Window`, then `Renderer`, so normal destruction occurs in reverse order. Constructors will continue to use RAII so a failure at any later stage destroys every already-created subsystem.

The current mutable process-global “platform active” guard will be removed if it is only compensating for implicit ordering. This runtime has one owner and does not need a general multi-platform lifetime manager.

A heap-allocated platform factory was rejected because a constructor dependency expresses the same invariant more directly and with less machinery.

### 5. Give the runtime sole control of the event/frame loop

For each iteration, the runtime will:

1. poll the platform event batch;
2. observe close state and translate physical input to an FPS action snapshot;
3. wait for more platform events when the framebuffer extent is zero;
4. otherwise send the renderer an explicit frame request containing the current extent and resize state;
5. consume a small renderer outcome such as rendered, skipped, or swapchain recreated.

The renderer will not poll or wait for events, query whether the application should close, or decide how a minimized application sleeps. It will handle Vulkan-local consequences of a frame request, including out-of-date/suboptimal swapchain results, and report the outcome to the runtime.

Moving all swapchain policy into the runtime was rejected because Vulkan result interpretation and recreation remain renderer responsibilities. Adding a fixed-timestep update phase is outside this change.

### 6. Translate physical input into one fixed FPS action snapshot

The platform module will expose backend-neutral physical key/button state and accumulated pointer movement; it will not emit movement, jump, crouch, or fire semantics. An engine-level `FpsInputMapper` will create the concrete one-player action snapshot using the project's fixed default mapping: W/A/S/D, Space, Left Shift, Left Control, Escape, and the left/right mouse buttons.

Held state persists across event batches, while look delta represents only movement accumulated since the previous batch and is then reset. Gameplay consumers will not see GLFW key codes or native constants.

A configurable binding database was rejected because user-remappable controls are not currently required and would broaden the architecture beyond this FPS.

### 7. Resolve resources from explicit runtime configuration

The launcher will construct `RuntimeConfig` with a resource root derived from its executable/runtime layout, and tests may pass a known fixture or built resource directory directly. The runtime will validate the root early and pass resolved shader paths into renderer construction. Renderer code will not construct paths relative to the process current working directory.

An implicit global resource directory and a process-wide current-directory change were rejected because both hide ownership and make tests and launch behavior fragile.

### 8. Store Vulkan validation diagnostics outside the callback

Development Vulkan setup will receive a runtime-owned diagnostics sink through the debug callback user-data pointer. The sink will decode and record severity/category information and maintain a thread-safe validation error count. Logging remains available, but the callback itself will not abort or throw.

Smoke tests will keep the sink alive until after renderer/Vulkan teardown and then fail when its error count is nonzero. This includes errors reported during destruction, which would be missed by checking while the renderer is still alive.

Relying on log-text matching was rejected because it is brittle and cannot provide a direct test result. Crashing inside the callback was rejected because callback context is not a safe exception or control-flow boundary.

### 9. Confine test controls to internal build surfaces

Failure injection, forced swapchain events, and diagnostic inspection used only by tests will move to internal headers or test-only compilation units guarded by the test build. They will not be methods on the public application/runtime facade.

The test suite will verify both behavior and boundaries: a minimal consumer compile, CMake link-interface checks, lifecycle/partial-failure cleanup, input translation, minimized/resize behavior, resource lookup from a different working directory, and validation-error propagation.

## Risks / Trade-offs

- **[More targets and files increase build complexity]** -> Limit the split to the three dependency boundaries that the runtime actually has and avoid one-target-per-folder organization.
- **[PImpl adds one startup allocation and indirection]** -> Keep it only at the public runtime facade; the cost is negligible for a long-lived application object.
- **[The internal Vulkan surface bridge couples platform and renderer details]** -> Keep it private, narrow, and specific to GLFW/Vulkan so the coupling is visible without leaking into general window consumers.
- **[Executable-relative resource discovery can vary by launch environment]** -> Resolve and validate the path once in the launcher, store the normalized path in `RuntimeConfig`, and let tests pass an explicit path.
- **[Validation callbacks may be invoked from different threads]** -> Use an atomic error count and synchronized storage only where detailed messages are retained.
- **[Centralizing minimized/resize behavior can subtly change the smoke loop]** -> Add focused runtime tests and retain the existing resize/recreation smoke coverage.
- **[Changing constructor and target boundaries creates a broad intermediate compile break]** -> Migrate in dependency order and keep each task buildable before removing the old catch-all target and APIs.

## Migration Plan

1. Add the backend-neutral runtime configuration/facade and a consumer boundary test while retaining existing internals behind its implementation.
2. Separate physical input collection from FPS action mapping and add mapping/batch-lifetime tests.
3. Make `Platform` an explicit `Window` dependency and move Vulkan surface operations to the internal bridge.
4. Introduce the renderer frame request/outcome API, then move polling, close, minimize, and resize coordination into the runtime loop.
5. Thread the explicit resource root through startup and renderer construction and add working-directory-independent tests.
6. Add the validation diagnostics sink, make smoke checks occur after teardown, and internalize test-only controls.
7. Split the CMake targets, remove the obsolete catch-all/public backend dependencies, and verify the dependency direction with a minimal consumer.
8. Run the documented configure/build/test and Vulkan smoke workflow, inspect the final diff, and strictly validate the OpenSpec change.

If an intermediate migration step cannot preserve a buildable target, the old facade may coexist temporarily inside the implementation, but it must be removed before the change is considered complete. No persisted data migration or deployment rollback is required; rollback is the source-level reversal of the new facade and target split.
