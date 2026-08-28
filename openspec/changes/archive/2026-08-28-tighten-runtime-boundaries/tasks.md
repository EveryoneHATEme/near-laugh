## 1. Runtime Facade and Boundary Tests

- [x] 1.1 Add backend-neutral public `RuntimeConfig` and application/runtime facade headers using PImpl, and verify a minimal consumer translation unit compiles without including Vulkan, GLFW, platform, or renderer headers.
- [x] 1.2 Move existing application composition behind the facade without changing visible startup behavior, and verify the current deterministic test preset still passes with `ctest --preset debug --output-on-failure`.
- [x] 1.3 Add an automated public-boundary check for forbidden backend includes/types and transitive public usage requirements, and verify the check fails against a deliberately backend-dependent fixture or equivalent negative test.

## 2. Physical and FPS Input Separation

- [x] 2.1 Replace platform FPS-semantic input values with engine-owned physical key/button state and per-batch cursor movement, and verify platform input tests cover press, release, held state, and cursor accumulation without GLFW constants in consumer code.
- [x] 2.2 Implement the concrete one-player `FpsInputMapper` and action snapshot for W/A/S/D, Space, Left Shift, Left Control, Escape, and left/right mouse buttons, and verify focused unit tests cover every required default mapping.
- [x] 2.3 Reset look delta between processed event batches while preserving held keys and buttons, and verify a multi-batch unit test observes zero new look movement with actions still held.

## 3. Platform Lifetime and Vulkan Surface Boundary

- [x] 3.1 Make `Window` construction require a live `Platform&`, enforce `Platform -> Window -> Renderer` member order, and verify lifecycle tests record reverse destruction order on normal shutdown and renderer-construction failure.
- [x] 3.2 Remove the compensating mutable platform-active global state once constructor ordering enforces the invariant, and verify attempts to construct a window without the required owner are rejected at the API/compile boundary before native creation.
- [x] 3.3 Move required-extension lookup and Vulkan surface creation into a narrow internal GLFW/Vulkan bridge, and verify the normal window and public runtime headers contain no Vulkan or GLFW types while renderer initialization still creates a presentation surface.

## 4. Engine-Owned Loop and Renderer Frame Contract

- [x] 4.1 Introduce engine-owned frame request/outcome types carrying framebuffer extent, resize state, and rendered/skipped/recovered results, and verify focused tests cover every outcome without exposing Vulkan results.
- [x] 4.2 Refactor the renderer to consume explicit frame requests and remove its event polling, event waiting, close-state queries, and persistent loop-policy decisions, and verify source-boundary checks plus renderer tests show zero GPU submission for a zero extent.
- [x] 4.3 Move poll, close, input-sample, minimized wait, and render-request sequencing into the Engine-owned loop, and verify runtime tests cover normal, close-requested, zero-extent, and resize/recovery iterations with at most one frame request per iteration.
- [x] 4.4 Preserve swapchain out-of-date/suboptimal handling inside the renderer while returning a backend-neutral recovery outcome, and verify the Vulkan smoke path still forces and survives one swapchain recreation.

## 5. Explicit Runtime Resources

- [x] 5.1 Derive and normalize the runtime resource root in the launcher, validate it during startup, and pass resolved shader paths into renderer construction; verify a missing shader error contains its resolved absolute path.
- [x] 5.2 Remove renderer-relative `resources/...` path construction and add a test launched from a different working directory, verifying the triangle shaders are still found through `RuntimeConfig::resource_root`.

## 6. Validation Diagnostics and Internal Test Seams

- [x] 6.1 Add a runtime-owned, thread-safe validation diagnostics sink that decodes severity/category and counts error-severity messages, and verify unit tests cover decoding plus concurrent-safe error counting.
- [x] 6.2 Connect the Vulkan debug callback to the diagnostics sink without throwing or aborting in callback context, and verify development startup still warns and continues when validation layers are unavailable but records errors when the callback reports them.
- [x] 6.3 Keep diagnostics alive through Vulkan teardown and make the smoke process return failure after orderly cleanup when any validation error was counted, and verify an injected validation-error test fails while a clean smoke run succeeds.
- [x] 6.4 Move failure injection and forced swapchain controls to internal/test-only headers or compilation units, and verify the public runtime facade exposes none of those controls while their existing tests remain executable.

## 7. Concrete CMake Module Boundaries

- [x] 7.1 Create the concrete `near_laugh_platform`, `near_laugh_render`, and `near_laugh_runtime` targets with dependency direction matching the design, and verify a clean `cmake --preset debug` configure and `cmake --build --preset debug` build succeed.
- [x] 7.2 Link the FPS executable through the runtime target, keep GLFW/Vulkan requirements private to implementation boundaries, and remove the obsolete catch-all target; verify the automated target-interface and minimal-consumer checks pass.
- [x] 7.3 Review installed/public include paths and compile commands for backend leakage, and verify runtime/gameplay translation units neither include backend headers nor receive GLFW/Vulkan include directories as public usage requirements.

## 8. Integrated Validation and Documentation

- [x] 8.1 Update `README.md` and relevant architecture/rendering/development documents to describe the runtime ownership, module boundaries, input split, resource root, and validation failure behavior, and verify documented target names and commands match the final CMake configuration.
- [x] 8.2 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`, and verify all deterministic checks complete successfully without ignored compiler warnings.
- [x] 8.3 Run `ctest --preset vulkan-smoke --output-on-failure` in a desktop Vulkan 1.3 environment, and verify triangle rendering, forced swapchain recreation, orderly teardown, and zero validation errors.
- [x] 8.4 Review `git diff` for unrelated refactors or unsupported abstractions and run `openspec validate tighten-runtime-boundaries --strict`, verifying the implementation remains within this change and the OpenSpec change is valid before completion.
