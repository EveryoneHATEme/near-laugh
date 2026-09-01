## Context

See `proposal.md` for motivation and the delta specs for required behavior. The existing runtime owns `Platform -> Window -> Renderer`, maps physical input to one `FpsActionSnapshot`, and sends a backend-neutral `FrameRequest` to a Vulkan 1.3 renderer. The renderer currently owns a host-visible triangle vertex buffer and a Dynamic Rendering pipeline with only a color attachment; shaders accept clip-space position and vertex color.

The next slice crosses runtime input, frame timing, camera mathematics, shader data, swapchain-dependent depth resources, and smoke validation. It must preserve the existing module boundary: the runtime may produce render data, but it must not expose GLFW or Vulkan types, and the renderer must not interpret gameplay input or own application-loop decisions.

## Goals / Non-Goals

**Goals:**

- Establish the smallest complete world-space 3D path: camera update, view/projection transform, opaque static geometry, and depth-correct output.
- Keep camera behavior deterministic under direct unit tests by separating elapsed-time input from clock acquisition.
- Preserve validation-clean Vulkan lifetime and swapchain recovery with depth resources present.
- Leave a narrow camera-frame seam that a later player controller can drive without retaining prototype camera behavior in the renderer.

**Non-Goals:**

- Establishing a `World`, entity hierarchy, ECS, physics subsystem, collision representation, or gameplay `Player` type.
- Loading meshes, textures, materials, or levels from external assets.
- Adding dynamic meshes, lighting, shadows, transparency, post-processing, or a render graph.
- Defining a general camera framework, cinematic modes, editor viewport, or persisted debug-camera settings.
- Optimizing static geometry upload beyond what this small built-in scene requires.

## Decisions

### 1. Keep camera state and input interpretation in the runtime

Add one concrete free-fly camera component beside `FpsInputMapper` in `near_laugh_runtime`. The Engine will manage cursor capture, sample the existing actions, update the camera only on renderable iterations, and include a backend-neutral camera frame in the render request. The renderer will consume only the resulting transform and will not see action names, sensitivity, speed, yaw, or pitch.

The camera frame will use fixed-size standard-library scalar storage with an explicit column-major convention rather than placing math-library types in `FrameRequest`. This keeps the current engine-owned frame header usable by the platform target and prevents a private math dependency from becoming a module-boundary requirement.

Putting a camera inside the renderer was rejected because it would make rendering interpret FPS input and time. Introducing a generic camera interface was rejected because there is one required first-person-style inspection camera and no second implementation.

### 2. Use a narrowly pinned GLM dependency for internal camera mathematics

Use a pinned GLM release privately in the runtime implementation for vector normalization, yaw/pitch basis construction, and view/projection composition. Configure radians and Vulkan's zero-to-one depth convention explicitly, apply the Vulkan projection-axis correction in one tested conversion path, and copy the final matrix into the standard-layout camera frame.

GLM types will not appear in public headers or cross the runtime-render boundary. Hand-written general vector and matrix classes were rejected because they would create a new mathematics surface whose conventions and numerical behavior the project would have to maintain. Directly embedding all matrix arithmetic in the Engine was rejected because it would be difficult to test and obscure camera policy.

### 3. Use variable elapsed time for the non-physical camera, with a hard discontinuity guard

The Engine will acquire time from `std::chrono::steady_clock`; the camera update will receive elapsed seconds as an explicit argument. Translation will scale by elapsed time, while mouse look will consume the event-batch delta directly and will not multiply it by elapsed time. Translation input will be combined and normalized before applying the normal or sprint speed.

Elapsed time will be capped at 100 ms for an ordinary renderable update, and the Engine will reset its previous timestamp after every blocking minimized-window wait. This prevents debugger stalls, resize stalls, and restore events from producing a large jump. A fixed simulation timestep was rejected because this camera has no physical integration and is not the future player simulation.

Pitch will be clamped below vertical, while yaw may accumulate or wrap without changing observable orientation. Camera defaults will be constants local to the prototype: an initial pose looking into the scene, a 75-degree vertical field of view, a 0.1 near plane, a far plane large enough for the room, a normal movement speed, a sprint multiplier, and mouse sensitivity.

### 4. Make cursor capture a small Engine-owned state transition

The Engine will capture the cursor once after the window exists. While captured, the menu action releases it; while released, the primary action recaptures it. Cursor transitions will use the existing window operation, which resets cursor tracking, and camera updates will be skipped while released. If release and recapture inputs overlap, release takes precedence for that iteration.

Adding a menu state machine or input rebinding layer was rejected because neither is required to inspect this scene. Closing remains the window manager's normal close operation; Escape releases the cursor rather than terminating the application.

### 5. Build one immutable world-space vertex stream in code

Replace the triangle fixture with a deterministic scene builder that emits colored triangle vertices for a floor, boundary surfaces, and several boxes or pillars. Concrete helper functions may append a box or quad because they directly reduce duplication in this one scene, but they will not form a public mesh API. Object placement will be baked into world-space positions during initialization, producing one immutable vertex buffer and one draw call.

The vertex shader will transform each world-space position by one view-projection matrix supplied through a vertex-stage push constant; vertex color remains the fragment input. A 4x4 float matrix fits within Vulkan's minimum guaranteed push-constant capacity and avoids introducing descriptor sets, uniform-buffer lifetime, or per-frame mapped resources for one small value.

Separate model transforms and draw objects were rejected for this slice because the scene is immutable and CPU-baked placement is sufficient. Indexed meshes and device-local staging uploads were also rejected until scene size or measured upload/runtime cost justifies them.

### 6. Own one depth attachment per swapchain image

Choose the first supported format from a short renderer-local list suitable for depth attachment use, then create a device-local image, allocation, and view for every swapchain image. One depth attachment per concurrently usable presentation image avoids cross-frame depth hazards without serializing the existing frames-in-flight policy. Each record operation will transition and use the corresponding depth image with Synchronization 2, clear depth to the convention's far value, and provide it to Dynamic Rendering.

The graphics pipeline will declare the selected depth format and enable depth testing/writes with the comparison operation matching the chosen conventional projection. Swapchain-dependent cleanup and recreation will include depth images, allocations, and views after affected device work is complete. Small deterministic helpers will cover depth-format selection; the Vulkan smoke path will cover real allocation, transitions, recreation, and teardown.

A single shared depth image was rejected because frames in flight can overlap. A render pass was rejected because the renderer is explicitly based on Dynamic Rendering.

### 7. Replace triangle assets and tests atomically

Rename the shader/resource vocabulary from triangle to prototype scene, compile and commit the corresponding SPIR-V files using the existing resource layout, and update the runtime resource resolver and copied-resource checks in the same implementation step. Triangle-specific vertex-layout tests will become scene vertex-layout and scene-construction tests rather than leaving two rendering paths.

Deterministic tests will cover camera basis/motion, pitch limiting, diagonal normalization, pause clamping/reset policy, camera matrix aspect changes, scene composition invariants, push-constant layout, and depth-format selection. The Vulkan smoke preset will render the scene, force swapchain recreation, exercise partial construction cleanup and the injected validation-error path, and require zero unexpected validation errors.

Keeping the triangle as an alternate path was rejected because it would add dead compatibility behavior and duplicate shaders, pipeline expectations, and test fixtures.

## Risks / Trade-offs

- **[Incorrect matrix storage or Vulkan clip conventions can make the scene mirrored, inverted, or clipped]** -> Define one documented camera-frame layout, configure GLM explicitly, and test known points plus aspect-ratio changes before relying on the smoke image.
- **[Depth image transitions can conflict across frames in flight]** -> Bind depth ownership to swapchain-image ownership and synchronize each depth image alongside the existing per-image fence tracking.
- **[A hard-coded scene can grow into an accidental asset or scene framework]** -> Keep construction private, immutable, vertex-colored, and limited to the geometry required by the prototype spec.
- **[Free-fly controls reuse jump and crouch actions with non-gameplay meaning]** -> Confine that interpretation to the prototype camera; a later player change replaces the consumer without changing physical input mapping.
- **[Adding GLM creates another fetched dependency]** -> Pin one version, link it privately, include only needed headers, and prevent its types from entering public or cross-module contracts.
- **[Visual correctness is difficult to prove in headless deterministic tests]** -> Test geometry and transforms numerically, then use the presentation-capable Vulkan smoke run for integration and validation correctness; record any manual visual check separately.

## Migration Plan

1. Add the private math dependency, backend-neutral camera-frame data, deterministic camera/scene helpers, and their unit tests without changing the public application facade.
2. Replace triangle shaders/resources and vertex upload with the built-in world-space scene and push-constant camera transform.
3. Add per-swapchain-image depth ownership, Dynamic Rendering depth attachment use, and depth-enabled pipeline state.
4. Integrate Engine timing, cursor state, camera updates, and render requests; then update boundary and resource-layout checks.
5. Configure and build the debug preset, run deterministic tests, run the Vulkan smoke preset, manually inspect the navigable scene when practical, and validate the OpenSpec change strictly.

There is no persisted-data or deployment migration. Rollback is a source-level reversal to the triangle shaders, triangle resource resolver, and color-only pipeline; it must also remove camera/depth tests and the private GLM dependency together so no unused prototype path remains.
