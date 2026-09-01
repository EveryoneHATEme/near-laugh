## 1. Event-Batch and Outcome Coordination

- [x] 1.1 Add a focused regression test and source-boundary assertion for a blocking wait event batch, verifying cursor delta cannot be reset by a later poll before the waited batch is sampled while held actions persist.
- [x] 1.2 Begin an input batch before blocking event dispatch and sample/map it in the Engine immediately after the wait returns, verifying the focused input and runtime-loop tests pass for poll, wait, close, and restore paths.
- [x] 1.3 Replace the discarded renderer return value with an exhaustive runtime-owned `FrameOutcome` policy, and verify unit tests cover rendered, skipped, and recovered outcomes while the source-boundary check rejects an ignored `renderFrame` result.

## 2. Executable-Relative Runtime Layout

- [x] 2.1 Add a launcher-local helper that returns the actual executable path through native facilities on each currently supported desktop host, and verify focused tests exercise buffer/path normalization and actionable discovery failures without exposing native types.
- [x] 2.2 Add a small process-level resource-layout test target that runs from a different working directory with misleading invocation text, and verify it still resolves executable-adjacent copied shader resources without starting GLFW or Vulkan.
- [x] 2.3 Update the FPS launcher to derive `RuntimeConfig::resource_root` from the native executable path instead of `argv[0]`, and verify normal startup and missing-resource diagnostics use the resolved executable-relative path.

## 3. Swapchain Capability Selection

- [x] 3.1 Add small Vulkan helpers for required image-usage validation and deterministic composite-alpha selection, and verify unit tests cover opaque preference, supported fallback modes, and missing color-attachment usage.
- [x] 3.2 Use the validated usage and selected composite-alpha mode in swapchain creation, and verify unsupported capabilities fail before `vkCreateSwapchainKHR` with the missing requirement named in the error.
- [x] 3.3 Preserve resize, out-of-date, and suboptimal recovery behavior after capability selection changes, and verify the existing forced-recreation Vulkan smoke path remains validation-clean.

## 4. Integrated Validation and Documentation

- [x] 4.1 Update `README.md` and the relevant architecture, rendering, and development documentation to describe waited input batches, runtime outcome consumption, native executable-relative resource discovery, and supported swapchain selection; verify documented behavior matches the final code.
- [x] 4.2 Run `cmake --preset debug`, `cmake --build --preset debug`, and `ctest --preset debug --output-on-failure`, and verify all deterministic unit, process, public-header, source, and target-boundary checks pass.
- [x] 4.3 Run `ctest --preset vulkan-smoke --output-on-failure`, and verify rendering, forced swapchain recreation, lifecycle cleanup, injected-validation failure handling, and zero unexpected validation errors.
- [x] 4.4 Review `git diff` for unrelated changes or new unsupported abstractions and run `openspec validate align-runtime-spec-conformance --strict`, verifying the implementation remains within the approved corrective scope.
