#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>
#include <optional>
#include <stdexcept>

#include "core/development/frame_capture.hpp"
#include "core/gameplay/door_controller.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/renderer.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/world/door.hpp"
#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
#include "editor/editor_renderer.hpp"
#include "launcher/executable_path.hpp"

namespace {
WorldPosition subtract(WorldPosition a, WorldPosition b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
WorldPosition normalized(WorldPosition v) {
  const float length = std::hypot(v.x, v.y, v.z);
  return {v.x / length, v.y / length, v.z / length};
}
WorldPosition cross(WorldPosition a, WorldPosition b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dot(WorldPosition a, WorldPosition b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
CameraFrame camera(WorldPosition eye, WorldPosition target) {
  const auto f = normalized(subtract(target, eye));
  const auto r = normalized(cross(f, {0, 1, 0}));
  const auto d = cross(f, r);
  const float sy = 1 / std::tan(std::numbers::pi_v<float> / 6),
              sx = sy * 9 / 16;
  constexpr float near = .05F, far = 100, a = far / (far - near), b = -near * a;
  return {{sx * r.x, sy * d.x, a * f.x, f.x, sx * r.y, sy * d.y, a * f.y, f.y,
           sx * r.z, sy * d.z, a * f.z, f.z, -sx * dot(r, eye),
           -sy * dot(d, eye), b - a * dot(f, eye), -dot(f, eye)}};
}
std::vector<OpaqueBoxFrame> doors(const LevelDocument& doc,
                                  std::optional<float> angle) {
  std::vector<OpaqueBoxFrame> boxes;
  for (const auto& door : doc.doors) {
    const auto geometry = doorPresentationBoxes(
        door, angle.value_or(doorInitialAngle(door)), door.initially_locked);
    boxes.insert(boxes.end(), geometry.begin(), geometry.end());
  }
  return boxes;
}
LevelDocument control(bool wall, bool doorway) {
  LevelDocument doc;
  doc.entries = {{"control", {{3, 0, 4}, -90}}};
  doc.default_entry = "control";
  doc.solids = {{{0, -.1F, 0},
                 {6, .1F, 6},
                 {255, 255, 255, 255},
                 PrototypeSolidKind::Floor,
                 "prototype-floor"}};
  doc.environment_light = {
      {{{-2, 3, 0}, {1, 1, 1}, 1.5F, 8, "key", true, true}}, 0};
  const auto block = [&](WorldPosition center, WorldExtent half) {
    doc.solids.push_back({center,
                          half,
                          {255, 255, 255, 255},
                          PrototypeSolidKind::Boundary,
                          "prototype-boundary"});
  };
  if (wall) block({0, 1.5F, 0}, {.08F, 1.5F, 3});
  if (doorway) {
    block({0, 1.5F, -1.85F}, {.08F, 1.5F, 1.15F});
    block({0, 1.5F, 1.85F}, {.08F, 1.5F, 1.15F});
    block({0, 2.6F, 0}, {.08F, .4F, .7F});
    DoorDefinition door;
    door.id = "control-door";
    door.hinge_position = {0, .02F, -.63F};
    door.closed_yaw_degrees = -90;
    door.width = 1.26F;
    door.height = 2.1F;
    door.thickness = .06F;
    door.open_angle_degrees = -90;
    doc.doors = {door};
  }
  return doc;
}
void runtimeCaptures(Window& window, ValidationDiagnostics& diagnostics,
                     const std::filesystem::path& root,
                     const std::filesystem::path& output,
                     const std::string& name, const LevelDocument& doc,
                     WorldPosition eye, WorldPosition target,
                     std::optional<float> angle = {},
                     SpotLightFrame spot = {}) {
  const auto level = makePrototypeLevel(doc);
  FrameCapture capture;
  RendererResources resources{root / "shaders/prototype_scene_vertex.spv",
                              root / "shaders/prototype_scene_fragment.spv",
                              root};
  resources.capture = &capture;
  Renderer renderer(window, window.framebufferExtent(), level, resources,
                    diagnostics);
  auto enabled = initialPointLightEnabled(doc.environment_light);
  const auto boxes = doors(doc, angle);
  FrameRequest frame{window.framebufferExtent(), false, camera(eye, target),
                     spot, enabled};
  frame.opaque_boxes = boxes;
  for (bool on : {true, false}) {
    if (!enabled.empty()) enabled[0] = on;
    capture.requested = true;
    for (int i = 0; i < 4 && capture.requested; ++i)
      (void)renderer.renderFrame(frame);
    capture.writePpm(output / (name + (on ? "-on.ppm" : "-off.ppm")));
  }
  std::ofstream metadata(output / (name + ".txt"));
  metadata << "eye " << eye.x << ' ' << eye.y << ' ' << eye.z << "\ntarget "
           << target.x << ' ' << target.y << ' ' << target.z
           << "\nvertical-fov 60\n"
           << "framebuffer " << capture.width << ' ' << capture.height << '\n';
  for (const auto& door : doc.doors)
    metadata << "door " << door.id << " angle "
             << angle.value_or(doorInitialAngle(door)) << '\n';
}
SpotLightFrame flashlight(WorldPosition eye, WorldPosition target) {
  const auto direction = normalized(subtract(target, eye));
  return {{eye.x, eye.y, eye.z, 12},
          {direction.x, direction.y, direction.z, .965926F},
          {1, 1, 1, 1},
          {.906308F, 1, 0, 0}};
}
void acceptedDoorCaptures(Window& window, ValidationDiagnostics& diagnostics,
                          const std::filesystem::path& root,
                          const std::filesystem::path& output,
                          WorldPosition eye, WorldPosition target) {
  auto doc = control(false, true);
  doc.doors[0].initially_open = true;
  doc.entries[0].pose.foot_position = {0, 0, 0};
  const auto level = makePrototypeLevel(doc);
  PhysicsWorld physics(level);
  DoorController controller(level.doors());
  (void)controller.act(0, DoorAction::Interact, eye);
  for (int i = 0; i < 90; ++i) {
    (void)physics.stepCharacter({{}, {0, -18, 0}}, 1.F / 60);
    controller.fixedStep(1.F / 60, physics);
  }
  const auto stopped = controller.state(0);
  if (stopped.moving || stopped.feedback != DoorResultKind::Obstructed ||
      !(stopped.angle > -90 && stopped.angle < 0))
    throw std::runtime_error(
        "Capture door did not stop against the actual player capsule");
  runtimeCaptures(window, diagnostics, root, output, "door-player-obstructed",
                  doc, eye, target, stopped.angle);
  for (int i = 0; i < 45; ++i)
    (void)physics.stepCharacter({{2, 0, 0}, {0, -18, 0}}, 1.F / 60);
  (void)controller.act(0, DoorAction::Interact, eye);
  for (int i = 0; i < 10; ++i) {
    (void)physics.stepCharacter({{}, {0, -18, 0}}, 1.F / 60);
    controller.fixedStep(1.F / 60, physics);
  }
  const auto reversed = controller.state(0).angle;
  if (!(reversed < stopped.angle - 10))
    throw std::runtime_error("Capture door did not reverse after obstruction");
  runtimeCaptures(window, diagnostics, root, output, "door-reversed", doc, eye,
                  target, reversed);
}
void editorCapture(Window& window, ValidationDiagnostics& diagnostics,
                   const std::filesystem::path& root,
                   const std::filesystem::path& output,
                   const LevelDocument& doc, WorldPosition eye,
                   WorldPosition target) {
  ImGui::CreateContext();
  try {
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::GetIO().DisplaySize = {1920, 1080};
    ImGui::GetIO().DeltaTime = 1.F / 60;
    FrameCapture capture;
    EditorRenderer renderer(
        window, window.framebufferExtent(),
        {root / "shaders/prototype_scene_vertex.spv",
         root / "shaders/prototype_scene_fragment.spv", root, &capture},
        diagnostics);
    renderer.replaceDocument(doc);
    const auto enabled = initialPointLightEnabled(doc.environment_light);
    FrameRequest frame{
        window.framebufferExtent(), false, camera(eye, target), {}, enabled};
    capture.requested = true;
    for (int i = 0; i < 4 && capture.requested; ++i) {
      renderer.beginUiFrame();
      ImGui::NewFrame();
      ImGui::Render();
      (void)renderer.renderFrame(frame);
    }
    capture.writePpm(output / "furnished-editor.ppm");
  } catch (...) {
    ImGui::DestroyContext();
    throw;
  }
  ImGui::DestroyContext();
}
int run(const std::filesystem::path& executable,
        const std::filesystem::path& output,
        const std::filesystem::path& material_root = {}) {
  if (std::filesystem::exists(output))
    throw std::runtime_error("Choose a new capture directory");
  std::filesystem::create_directories(output);
  const auto root = executable.parent_path() / "resources";
  ValidationDiagnostics diagnostics;
  {
    Platform platform;
    Window window(platform, 1920, 1080,
                  "Interior lighting fixed-view validation");
    window.useFullscreenMode(1920, 1080, 60);
    window.pollEvents();
    const WorldPosition eye{3, 2.5F, 4}, target{1, 0, 0};
    if (!material_root.empty()) {
      auto doc = control(false, false);
      doc.environment_light.point_lights[0].position = {0, 3, 0};
      for (const auto& [name, model] :
           {std::pair{"material-mask", "apartment-phone"},
            std::pair{"material-backface-mask", "apartment-table"},
            std::pair{"material-opaque", "apartment-radio"},
            std::pair{"material-factor-discard", "apartment-chair"}}) {
        doc.props = {{"coverage-control", model, {0, 0, 0}, 0, 1, {}}};
        runtimeCaptures(window, diagnostics, material_root, output, name, doc,
                        eye, {0, 0, 0});
      }
      doc.props.clear();
      runtimeCaptures(window, diagnostics, material_root, output,
                      "material-unoccluded", doc, eye, {0, 0, 0});
    }
    runtimeCaptures(window, diagnostics, root, output, "lit",
                    control(false, false), eye, target);
    auto unshadowed = control(false, false);
    unshadowed.environment_light.point_lights[0].casts_shadows = false;
    runtimeCaptures(window, diagnostics, root, output, "lit-unshadowed",
                    unshadowed, eye, target);
    runtimeCaptures(window, diagnostics, root, output, "wall",
                    control(true, false), eye, target);
    runtimeCaptures(window, diagnostics, root, output, "door-closed",
                    control(false, true), eye, target);
    runtimeCaptures(window, diagnostics, root, output, "door-open",
                    control(false, true), eye, target, -90);
    runtimeCaptures(window, diagnostics, root, output, "door-partial",
                    control(false, true), eye, target, -45);
    acceptedDoorCaptures(window, diagnostics, root, output, eye, target);
    runtimeCaptures(window, diagnostics, root, output, "wall-flashlight",
                    control(true, false), eye, target, {},
                    flashlight(eye, target));
    auto only_flashlight = control(true, false);
    only_flashlight.environment_light.point_lights.clear();
    runtimeCaptures(window, diagnostics, root, output, "zero-lights-flashlight",
                    only_flashlight, eye, target, {}, flashlight(eye, target));
    runtimeCaptures(window, diagnostics, root, output, "offscreen-wall",
                    control(true, false), {3, 1.65F, 0}, {2, 0, 0});
    auto seam = control(true, false);
    seam.environment_light.point_lights[0].position = {0, 3, 0};
    seam.solids[1].center.x = 1.5F;
    runtimeCaptures(window, diagnostics, root, output, "face-seam", seam,
                    {5, 2.5F, 4}, {3, 0, 0});
    const auto loaded =
        loadLevelDocument(root / "levels/interior-lighting.level.json");
    if (!loaded)
      throw std::runtime_error(formatLevelDiagnostics(loaded.diagnostics));
    auto furnished = *loaded.document;
    const WorldPosition room_eye{-3, 4.65F, 3.7F}, room_target{-1, 4, 3.7F};
    runtimeCaptures(window, diagnostics, root, output, "furnished", furnished,
                    room_eye, room_target);
    editorCapture(window, diagnostics, root, output, furnished, room_eye,
                  room_target);
    runtimeCaptures(window, diagnostics, root, output, "corridor", furnished,
                    {0, 4.65F, 3.5F}, {0, 4, -5});
    runtimeCaptures(window, diagnostics, root, output, "kitchen", furnished,
                    {3, 4.65F, -1.2F}, {3, 3.6F, -4});
    runtimeCaptures(window, diagnostics, root, output, "stairs", furnished,
                    {0, 4.65F, -6}, {0, 1.5F, -16});
    runtimeCaptures(window, diagnostics, root, output, "landing", furnished,
                    {0, 1.65F, -16.2F}, {0, 2, -9});
    for (auto& light : furnished.environment_light.point_lights)
      light.initially_on = false;
    runtimeCaptures(window, diagnostics, root, output, "furnished-all-off",
                    furnished, room_eye, room_target);
    furnished.environment_light.ambient_intensity = 0;
    runtimeCaptures(window, diagnostics, root, output, "furnished-zero-ambient",
                    furnished, room_eye, room_target);
  }
  if (diagnostics.errorCount())
    throw std::runtime_error(
        "Fixed-view readback recorded Vulkan validation errors");
  std::cout << "Fixed-view captures written to " << output.string() << '\n';
  return 0;
}
}  // namespace
#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
  try {
    if (argc < 2 || argc > 3)
      throw std::runtime_error(
          "Usage: interior_lighting_visual <new-output-directory> "
          "[material-fixture-root]");
    return run(launcher::currentExecutablePath(),
               std::filesystem::absolute(argv[1]),
               argc == 3 ? std::filesystem::absolute(argv[2])
                         : std::filesystem::path{});
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
#else
int main(int argc, char** argv) {
  try {
    if (argc < 2 || argc > 3)
      throw std::runtime_error(
          "Usage: interior_lighting_visual <new-output-directory> "
          "[material-fixture-root]");
    return run(launcher::currentExecutablePath(),
               std::filesystem::absolute(argv[1]),
               argc == 3 ? std::filesystem::absolute(argv[2])
                         : std::filesystem::path{});
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
#endif
