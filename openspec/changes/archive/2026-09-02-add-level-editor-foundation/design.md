## Context

See `proposal.md` for motivation. This change assumes `add-bounded-level-persistence` has been implemented and archived. The game currently has one GLFW window abstraction with FPS-specific input accumulation, one Vulkan renderer that owns presentation and immutable game-scene resources, and no tool executable or UI dependency.

The editor must render the same level data but has different lifetime, input, and presentation needs. Treating it as a mode of the `fps` executable would couple tool state to gameplay and make ImGui a shipping dependency.

## Goals / Non-Goals

**Goals:**

- Establish a separate editor executable with explicit RAII ownership and editor-only dependencies.
- Reuse project-owned level data, validation, scene conversion, and low-level Vulkan policy without adding an RHI or plugin hook.
- Provide a stable document shell and camera/input policy ready for later placement and sculpting changes.
- Keep the first viewport simple: scene rendering directly to the main swapchain with UI overlaid in the same window.

**Non-Goals:**

- Offscreen viewport textures, multiple OS viewports, play-in-editor, physics simulation, or runtime hot reload.
- Object selection or mutation, gizmos, terrain brushes, asset browsing, or a scene hierarchy.
- Generalizing the game renderer into a tool-extensible rendering framework.

## Decisions

### Build a separate concrete editor stack

Add a `level_editor` executable composed from editor-owned application, document, camera, UI, platform bridge, and renderer code. It links `near_laugh_world` and reuses internal platform/render utilities only through narrow project-owned interfaces. It does not link `near_laugh_runtime`, construct `Application`, or add editor APIs to the public runtime facade.

Use explicit editor modules rather than an `EditorMode` branch in `Engine`. This duplicates a small amount of loop coordination but keeps gameplay lifetime and input behavior unchanged.

### Keep Dear ImGui editor-only and pinned

Fetch an exact Dear ImGui release or commit and compile core sources plus only the GLFW and Vulkan backends into an editor UI target. Enable docking inside the main OS window, but disable platform multi-viewports. The game renderer, `fps`, public headers, and runtime target must have no transitive ImGui dependency.

ImGui supplies panels and input-capture intent; it does not own the level document, camera, renderer, or application lifetime. File open/save-as uses an ImGui path-entry modal in this first change instead of introducing a native-dialog dependency.

### Bridge GLFW callbacks without broadening FPS input

Retain the existing `Window` and `PhysicalInputSnapshot` contract for gameplay. Add one internal editor bridge, analogous to `GlfwVulkanBridge`, that initializes and shuts down the ImGui GLFW backend against the hidden native handle. Initialize the backend with callback chaining after `Window` installs its callbacks so ImGui receives text, scroll, key, and pointer input while the existing snapshot still supports W/A/S/D, Space, Left Control, and Left Shift camera movement.

The editor consults ImGui capture flags plus explicit scene-navigation state before consuming the physical snapshot. This avoids turning the FPS input mapper into a generic UI event system.

### Use an editor-specific Vulkan renderer

Add a concrete editor renderer that owns one Vulkan context, swapchain-dependent scene pipeline, immutable textures and lights for the active document, generated scene meshes, and ImGui Vulkan backend lifetime. It records scene draws first and ImGui draw data last in one dynamic-rendering pass to the swapchain image. A pass-through central dock region leaves the scene visible behind docked panels, avoiding an offscreen scene framebuffer.

Share existing vertex generation, texture/resource code, Vulkan selection utilities, and destruction-order conventions where they already form useful concrete units. Do not add a generic render callback, render graph, renderer interface, or backend abstraction. Editor renderer code may live in an editor-specific target while using Vulkan directly.

When a different level is opened, wait only for the editor renderer's in-flight frames as required, then replace document-dependent scene, lighting, and model-placement resources. Swapchain recreation retains the active document and UI state.

### Make document replacement transactional

`EditorDocument` owns a `LevelDocument`, resolved optional path, validation diagnostics, dirty state, and later extension points for selection/history. Opening parses and validates into a temporary candidate; only success replaces the active document. Save serializes the active valid document through the shared codec and clears dirty state only after atomic replacement succeeds.

Dirty-document transitions use one pending action state machine:

```text
requested open/close/exit
          |
          v
    document dirty? -- no --> perform action
          |
         yes
          v
   save / discard / cancel
      |       |        |
      v       v        v
   save then perform   retain state
```

This keeps modal UI decisions separate from filesystem and lifetime operations.

### Restore the concrete former free-fly policy inside the editor

Implement an editor-owned camera value using the proven former prototype free-fly behavior: finite position/yaw/pitch/FOV, W/A/S/D horizontal motion, Space/Left Control vertical motion, Left Shift sprint, mouse look, pitch limits, and current framebuffer aspect. It produces the existing backend-neutral camera matrix format but has no collision or gameplay identity.

## Risks / Trade-offs

- **[ImGui callback installation conflicts with current GLFW callbacks]** -> Initialize in a defined order, use backend callback chaining, and add tests or instrumentation for both UI capture and camera input.
- **[Editor renderer duplicates orchestration]** -> Share narrow Vulkan and scene helpers, but accept a concrete second renderer rather than weakening gameplay boundaries with generic hooks.
- **[Direct-to-swapchain scene limits future viewport layouts]** -> Treat offscreen docked viewports as a later change only if placement workflow requires them.
- **[Editor can become a general engine tool]** -> Keep document panels and renderer inputs tied to the bounded FPS level schema and enforce dependency-boundary tests.
- **[Native file dialogs are absent]** -> Use explicit path entry now; a platform dialog can be proposed later based on measured usability need.

## Migration Plan

1. Implement and archive `add-bounded-level-persistence` first.
2. Add pinned ImGui and editor-only target boundaries with compile/dependency tests.
3. Add editor platform/UI lifetime, camera, document state machine, and headless deterministic tests.
4. Add the editor renderer and read-only workspace, then verify resize/minimize recovery and Vulkan validation.
5. Document build/run controls and confirm `fps` behavior and dependencies remain unchanged.

Rollback removes the editor targets and pinned ImGui declaration without changing the persisted level format or game executable.

