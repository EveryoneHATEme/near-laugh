#include "editor/editor_application.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "core/world/prototype_level.hpp"
#include "editor/editor_loop.hpp"
#include "editor/editor_overlay.hpp"

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
  return {requireEditorFile(root / "shaders" / "prototype_scene_vertex.spv"),
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

  struct SmokeDirectory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("near_laugh_editor_smoke_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    SmokeDirectory() { std::filesystem::create_directory(path); }
    ~SmokeDirectory() {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  } temporary;
  const auto require = [](bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
  };
  const auto preview = [&] {
    require(tick(), "Editor stopped during editing smoke");
    require(scene_resources_installed_ &&
                rendered_document_revision_ == document_.revision(),
            "Edited scene resources were not installed");
    require(std::none_of(
                document_.diagnostics().begin(), document_.diagnostics().end(),
                [](const auto& d) {
                  return d.category == LevelDiagnosticCategory::Filesystem;
                }),
            "Editor reported a preview resource failure");
  };
  require(document_.saveAs(temporary.path / "working.level.json"),
          "Editor smoke Save As failed");
  const LevelDocument original = *document_.document();
  document_.select(document_.solidIds().back());
  require(document_.duplicateSelected(), "Editor smoke duplicate failed");
  preview();
  require(document_.removeSelected(), "Editor smoke remove failed");
  preview();
  require(document_.undo(), "Editor smoke undo failed");
  preview();
  require(document_.undo(), "Editor smoke duplicate undo failed");
  preview();
  require(*document_.document() == original && !document_.dirty(),
          "Editor smoke undo did not restore saved content");

  auto spawn = original.player_spawn;
  spawn.foot_position.y += 2;
  require(document_.replaceObject(editor_spawn, spawn),
          "Editor smoke invalid spawn edit failed");
  document_.select(editor_spawn);
  preview();
  require(!document_.valid() && !document_.save(),
          "Editor smoke saved an invalid level");
  document_.requestClose();
  preview();
  require(document_.resolvePending(EditorPendingDecision::Cancel),
          "Editor smoke cancel close failed");
  require(document_.undo(), "Editor smoke invalid edit undo failed");
  preview();

  auto light = original.environment_light.point_lights[0];
  light.position.x += 0.5F;
  light.intensity += 0.25F;
  document_.select(editor_first_light);
  require(document_.replaceObject(editor_first_light, light),
          "Editor smoke light edit failed");
  preview();
  auto prop = original.static_prop;
  prop.yaw_degrees += 20;
  prop.uniform_scale *= 0.9F;
  document_.select(editor_prop);
  require(document_.replaceObject(editor_prop, prop),
          "Editor smoke prop edit failed");
  preview();
  require(document_.save(), "Editor smoke edited save failed");
  const auto edited = *document_.document();
  require(document_.open(temporary.path / "working.level.json"),
          "Editor smoke edited reload failed");
  require(*document_.document() == edited,
          "Editor smoke save/reload changed content");
  document_.select(editor_prop);
  preview();

  document_.requestOpen(valid_level);
  static_cast<void>(tick());
  if (!scene_resources_installed_) {
    throw std::runtime_error(
        "Editor smoke did not replace scene resources for a valid level");
  }
  document_.requestOpen(valid_level.parent_path() / "missing.level.json");
  static_cast<void>(tick());

  document_.select(editor_prop);
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
  require(document_.selection() == editor_prop && scene_resources_installed_,
          "Editor recovery lost the selection or scene resources");

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
  const CameraFrame camera =
      camera_.frame(static_cast<float>(framebuffer.width) /
                    static_cast<float>(framebuffer.height));
  ui_.draw(document_);
  const auto placement_hit =
      ui_.updateViewport(document_, camera, window_.cursorCaptured());
  renderer_.drawOverlays(buildEditorOverlay(document_, camera, placement_hit));
  ui_.finishFrame();
  synchronizeDocumentResources();

  FrameRequest frame;
  frame.framebuffer = framebuffer;
  frame.framebuffer_resized = window_.consumeFramebufferResize();
  frame.camera = camera;
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
      renderer_.replaceDocument(*document_.document());
    } else {
      renderer_.clearDocument();
    }
    scene_resources_installed_ = document_.document().has_value();
    rendered_document_revision_ = document_.revision();
  } catch (const std::exception& error) {
    // Replacement is transactional: retain the last usable preview on failure.
    rendered_document_revision_ = document_.revision();
    document_.reportResourceError(
        std::string("Scene resource replacement failed: ") + error.what());
  }
}
