#include "editor/editor_application.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "core/world/light_switch.hpp"
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
    require(preview_point_light_enabled_ ==
                initialPointLightEnabled(document_.document()->light_switch),
            "Editor preview lost authored point-light state");
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

  document_.select(editor_light_switch);
  require(document_.removeSelected(), "Editor smoke switch removal failed");
  preview();
  require(document_.addLightSwitch(), "Editor smoke switch creation failed");
  require(!document_.addLightSwitch() && !document_.duplicateSelected(),
          "Editor smoke allowed another switch");
  preview();
  require(document_.undo() && document_.undo(),
          "Editor smoke switch restoration failed");
  preview();
  auto light_switch = *document_.document()->light_switch;
  light_switch.yaw_degrees += 25;
  for (const auto slot : {0U, 1U}) {
    for (const bool on : {false, true}) {
      light_switch.point_light_index = slot;
      light_switch.initially_on = on;
      require(document_.replaceObject(editor_light_switch, light_switch),
              "Editor smoke switch properties failed");
      preview();
    }
  }
  light_switch.initially_on = false;
  require(document_.replaceObject(editor_light_switch, light_switch),
          "Editor smoke initially-off switch failed");
  preview();
  require(document_.save(), "Editor smoke switch save failed");
  // Exercise every point/spot combination. Normal frames restore the
  // document's initial-state preview without mutating authored values.
  for (int mask = 0; mask < 8; ++mask) {
    preview();
    FrameRequest frame;
    frame.framebuffer = window_.framebufferExtent();
    frame.camera = camera_.frame(16.0F / 9.0F);
    frame.point_light_enabled = {(mask & 1) != 0, (mask & 2) != 0};
    if (mask & 4)
      frame.spot_light = {
          {0, 2, 4, 10}, {0, 0, -1, 0.95F}, {1, 1, 1, 1}, {0.85F, 1, 0, 0}};
    static_cast<void>(renderer_.renderFrame(frame));
  }
  const auto before_terrain = *document_.document();

  EditorTerrainBrush brush;
  brush.radius = 3;
  brush.strength = 0.01F;
  require(document_.setTerrainBrush(brush),
          "Editor smoke brush settings failed");
  const auto uploads = renderer_.terrainReplacementCount();
  document_.beginTerrainStroke({{10, 0, 10}});
  for (int i = 1; i <= 20; ++i)
    document_.extendTerrainStroke({{10 + i * 0.25F, 0, 10}});
  preview();
  require(
      renderer_.terrainReplacementCount() == uploads + 1,
      "Same-frame terrain stamps did not coalesce into one buffer replacement");
  preview();
  require(renderer_.terrainReplacementCount() == uploads + 1,
          "Unchanged frame unnecessarily rebuilt the terrain");
  document_.extendTerrainStroke({{16, 0, 10}});
  window_.setSize(1280, 720);
  renderer_.requestSwapchainRecreation();
  preview();
  require(document_.terrainStrokeActive(),
          "Resize unexpectedly lost the stroke");
  require(document_.finishTerrainStroke(), "Editor smoke stroke commit failed");
  preview();
  const auto sculpted = *document_.document();
  require(document_.undo(), "Editor smoke terrain undo failed");
  preview();
  require(*document_.document() == before_terrain && !document_.dirty(),
          "Terrain undo did not restore the saved document");
  require(document_.redo(), "Editor smoke terrain redo failed");
  preview();
  require(*document_.document() == sculpted, "Terrain redo changed samples");

  brush.mode = EditorBrushMode::Smooth;
  require(document_.setTerrainBrush(brush),
          "Editor smoke smooth settings failed");
  document_.beginTerrainStroke({{12, 0, 10}});
  preview();
  document_.extendTerrainStroke({{15, 0, 10}});
  require(document_.finishTerrainStroke(),
          "Editor smoke smoothing had no effect");
  preview();
  require(document_.save(), "Editor smoke sculpted save failed");
  const auto saved_terrain = *document_.document();
  require(document_.open(temporary.path / "working.level.json"),
          "Editor smoke sculpted reload failed");
  require(*document_.document() == saved_terrain,
          "Editor smoke sculpted round trip changed content");
  preview();

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
  brush.mode = EditorBrushMode::Lower;
  require(document_.setTerrainBrush(brush),
          "Editor smoke lower settings failed");
  document_.beginTerrainStroke({{22, 0, 22}});
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
  require(document_.pendingAction().kind == EditorPendingActionKind::Exit,
          "Sculpted exit did not request an unsaved decision");
  require(document_.resolvePending(EditorPendingDecision::Discard),
          "Editor smoke sculpted discard failed");
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
      static_cast<void>(document_.finishTerrainStroke());
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
  renderer_.drawOverlays(buildEditorOverlay(
      document_, camera, placement_hit,
      ui_.sculpting() ? &document_.terrainBrush() : nullptr));
  ui_.finishFrame();
  synchronizeDocumentResources();

  FrameRequest frame;
  frame.framebuffer = framebuffer;
  frame.framebuffer_resized = window_.consumeFramebufferResize();
  frame.camera = camera;
  frame.point_light_enabled = preview_point_light_enabled_;
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
      if (scene_resources_installed_ &&
          rendered_object_revision_ == document_.objectRevision())
        renderer_.replaceTerrain(*document_.document());
      else
        renderer_.replaceDocument(*document_.document());
    } else {
      renderer_.clearDocument();
    }
    scene_resources_installed_ = document_.document().has_value();
    preview_point_light_enabled_ = initialPointLightEnabled(
        document_.document() ? document_.document()->light_switch
                             : std::nullopt);
    rendered_document_revision_ = document_.revision();
    rendered_object_revision_ = document_.objectRevision();
  } catch (const std::exception& error) {
    // Replacement is transactional: retain the last usable preview on failure.
    rendered_document_revision_ = document_.revision();
    document_.reportResourceError(
        std::string("Scene resource replacement failed: ") + error.what());
  }
}
