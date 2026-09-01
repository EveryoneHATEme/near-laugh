## Why

Once levels are persisted, the single developer needs a visual workspace for inspecting and authoring them without turning the game runtime into a general-purpose editor. A separate editor foundation establishes that workspace and its ownership boundaries before content mutation features are added.

## What Changes

- Add a dedicated `level_editor` executable that loads and saves the bounded FPS level format introduced by `add-bounded-level-persistence`.
- Integrate pinned Dear ImGui sources only into editor targets for menus, panels, property display, dialogs, and validation feedback.
- Add an editor-owned free-fly camera and a Vulkan scene view with an ImGui overlay in the main window.
- Add open, save, save-as, dirty-state, close-confirmation, and validation-reporting workflows while leaving loaded content unchanged.
- Keep the shipping `fps` executable, gameplay input mapping, public runtime facade, and immutable game startup free of ImGui and editor state.

## Capabilities

### New Capabilities

- `level-editor`: Standalone editor lifetime, editor camera, ImGui workspace, level document operations, dirty-state handling, and validation presentation.

### Modified Capabilities


## Impact

- Adds editor-specific build targets and a pinned Dear ImGui dependency with GLFW/Vulkan backends.
- Affects CMake, editor-only platform/input handling, editor-only Vulkan rendering, level persistence APIs, resources, and tests.
- Depends on `add-bounded-level-persistence`; it does not add object mutation, terrain brushes, runtime editing, a plugin system, or a generic scene hierarchy.

