#include "editor/editor_application.hpp"

#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "core/world/prototype_level.hpp"
#include "editor/editor_loop.hpp"

namespace {
std::filesystem::path requireEditorFile(const std::filesystem::path& path) {
  const std::filesystem::path resolved =
      std::filesystem::absolute(path).lexically_normal();
  if (!std::filesystem::is_regular_file(resolved)) {
    throw std::runtime_error("Required editor resource is missing: " +
                             resolved.string());
  }
  return resolved;
}

EditorRendererResources resolveEditorRendererResources(
    const std::filesystem::path& resource_root) {
  const std::filesystem::path root =
      std::filesystem::absolute(resource_root).lexically_normal();
  return {
      requireEditorFile(root / "shaders" / "prototype_scene_vertex.spv"),
      requireEditorFile(root / "shaders" / "prototype_scene_fragment.spv"),
      {requireEditorFile(root / "textures" / "prototype_floor.png"),
       requireEditorFile(root / "textures" / "prototype_boundary.png"),
       requireEditorFile(root / "textures" / "prototype_obstacle.png")},
      requireEditorFile(root / "models" / "prototype_chair.glb")};
}
}  // namespace

EditorApplication::EditorApplication(
    std::filesystem::path resource_root,
    std::optional<std::filesystem::path> initial_level)
    : window_(platform_, 1600, 900, "near-laugh level editor"),
      glfw_imgui_bridge_(window_),
      renderer_(window_, window_.framebufferExtent(),
                resolveEditorRendererResources(resource_root),
                validation_diagnostics_) {
  if (initial_level) {
    static_cast<void>(document_.open(*initial_level));
    synchronizeDocumentResources();
  }
}

void EditorApplication::run() {
  while (tick()) {
  }
}

void EditorApplication::runSmoke(const std::filesystem::path& valid_level) {
  if (!document_.document()) {
    if (!document_.open(valid_level)) {
      throw std::runtime_error(
          "Editor smoke could not open the packaged level");
    }
    synchronizeDocumentResources();
  }
  if (!scene_resources_installed_) {
    throw std::runtime_error(
        "Editor smoke did not install scene resources for the packaged level");
  }
  for (int frame = 0; frame < 2 && tick(); ++frame) {
  }

  document_.requestOpen(valid_level);
  static_cast<void>(tick());
  if (!scene_resources_installed_) {
    throw std::runtime_error(
        "Editor smoke did not replace scene resources for a valid level");
  }
  document_.requestOpen(valid_level.parent_path() / "missing.level.json");
  static_cast<void>(tick());

  window_.setSize(1280, 720);
  renderer_.requestSwapchainRecreation();
  static_cast<void>(tick());

  window_.minimize();
  std::thread wake_wait([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EditorGlfwBridge::postEmptyEvent();
  });
  static_cast<void>(tick());
  wake_wait.join();
  window_.restore();
  EditorGlfwBridge::postEmptyEvent();
  static_cast<void>(tick());

  document_.requestExit();
  static_cast<void>(tick());
}

bool EditorApplication::tick() {
  window_.pollEvents();
  if (window_.shouldClose()) {
    window_.cancelCloseRequest();
    document_.requestExit();
  }

  const FramebufferExtent framebuffer = window_.framebufferExtent();
  switch (decideEditorLoopAction(document_.exitRequested(), framebuffer)) {
    case EditorLoopAction::Exit:
      return false;
    case EditorLoopAction::WaitForEvents:
      window_.waitEvents();
      frame_clock_.reset();
      return !document_.exitRequested();
    case EditorLoopAction::Render:
      break;
  }

  renderer_.beginUiFrame();
  glfw_imgui_bridge_.beginFrame();
  updateNavigation(glfw_imgui_bridge_.captureIntent());
  ui_.draw(document_);
  ui_.finishFrame();
  synchronizeDocumentResources();

  FrameRequest frame;
  frame.framebuffer = framebuffer;
  frame.framebuffer_resized = window_.consumeFramebufferResize();
  frame.camera = camera_.frame(static_cast<float>(framebuffer.width) /
                               static_cast<float>(framebuffer.height));
  const FrameOutcome outcome = renderer_.renderFrame(frame);
  return editorContinuesAfter(outcome) && !document_.exitRequested();
}

std::size_t EditorApplication::validationErrorCount() const noexcept {
  return validation_diagnostics_.errorCount();
}

void EditorApplication::updateNavigation(EditorUiCaptureIntent capture) {
  const PhysicalInputSnapshot& physical = window_.input();
  if (window_.cursorCaptured() && physical.isKeyDown(PhysicalKey::Escape)) {
    window_.setCursorCaptured(false);
    frame_clock_.reset();
  } else if (!window_.cursorCaptured() && !capture.pointer &&
             physical.isMouseButtonDown(PhysicalMouseButton::Right)) {
    window_.setCursorCaptured(true);
    frame_clock_.reset();
  }

  const bool navigation_active = window_.cursorCaptured();
  camera_.update(editorNavigationInput(physical, navigation_active, capture),
                 frame_clock_.sample(EditorFrameClock::Clock::now()));
}

void EditorApplication::synchronizeDocumentResources() {
  if (rendered_document_revision_ == document_.revision()) {
    return;
  }
  try {
    if (document_.document()) {
      renderer_.replaceDocument(makePrototypeLevel(*document_.document()));
    } else {
      renderer_.clearDocument();
    }
    scene_resources_installed_ = document_.document().has_value();
    rendered_document_revision_ = document_.revision();
  } catch (const std::exception& error) {
    renderer_.clearDocument();
    scene_resources_installed_ = false;
    rendered_document_revision_ = document_.revision();
    document_.reportResourceError(
        std::string("Scene resource replacement failed: ") + error.what());
  }
}
