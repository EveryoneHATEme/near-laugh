#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/animation/character_animation.hpp"
#include "core/development/frame_capture.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/renderer.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/testing/test_controls.hpp"
#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
#include "development/character_fixture.hpp"
#include "editor/editor_renderer.hpp"
#include "launcher/executable_path.hpp"

namespace {
void require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}
class Failure {
 public:
  explicit Failure(const char* stage) {
#ifdef _WIN32
    _putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", stage);
#else
    setenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", stage, 1);
#endif
  }
  ~Failure() {
#ifdef _WIN32
    _putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", "");
#else
    unsetenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE");
#endif
  }
};
std::size_t count(const std::vector<std::string>& events, const char* value) {
  return static_cast<std::size_t>(std::count(events.begin(), events.end(), value));
}
void balanced(const std::vector<std::string>& events) {
  for (const auto& pair : {std::pair{"character.buffer.created", "character.buffer.destroyed"},
                          std::pair{"character.memory.allocated", "character.memory.freed"},
                          std::pair{"character.index.buffer.created", "character.index.buffer.destroyed"},
                          std::pair{"character.index.memory.allocated", "character.index.memory.freed"},
                          std::pair{"character.resources.created", "character.resources.destroyed"},
                          std::pair{"texture.created", "texture.destroyed"},
                          std::pair{"device.created", "device.destroyed"}})
    require(count(events, pair.first) == count(events, pair.second),
            std::string("Unbalanced character lifetime: ") + pair.first);
}
std::array<std::size_t, 5> indexEvents(const std::vector<std::string>& events) {
  return {count(events, "character.index.buffer.created"),
          count(events, "character.index.buffer.destroyed"),
          count(events, "character.index.memory.allocated"),
          count(events, "character.index.memory.freed"),
          count(events, "character.indices.uploaded")};
}
constexpr std::array character_allocation_failures{
    "character_material", "character_buffer", "character_memory", "character_slots",
    "character_index_buffer", "character_index_memory", "character_index_upload"};
std::size_t changed(const std::vector<std::uint8_t>& a,
                    const std::vector<std::uint8_t>& b) {
  require(a.size() == b.size(), "Readback sizes differ");
  std::size_t pixels{};
  for (std::size_t i = 0; i < a.size(); i += 4) {
    int difference{};
    for (std::size_t c = 0; c < 3; ++c)
      difference = std::max(difference, std::abs(int(a[i + c]) - int(b[i + c])));
    pixels += difference > 3;
  }
  return pixels;
}
std::size_t visibleVertices(const CharacterAsset& asset, const CharacterPose& pose,
                            const CameraFrame& camera) {
  const auto vertices = deformCharacter(asset, pose);
  const auto& m = camera.view_projection;
  std::size_t count{};
  for (const auto& v : vertices) {
    std::array<float, 4> clip{};
    for (unsigned r = 0; r < 4; ++r)
      clip[r] = m[r] * v.position[0] + m[4 + r] * v.position[1] +
                m[8 + r] * v.position[2] + m[12 + r];
    count += clip[3] > 0 && std::abs(clip[0]) <= clip[3] &&
             std::abs(clip[1]) <= clip[3] && clip[2] >= 0 && clip[2] <= clip[3];
  }
  return count;
}
struct Evidence {
  std::filesystem::path directory;
  std::ofstream observations;
  explicit Evidence(std::filesystem::path path) : directory(std::move(path)) {
    if (!directory.empty()) {
      require(!std::filesystem::exists(directory), "Choose a fresh capture directory");
      std::filesystem::create_directories(directory);
      observations.open(directory / "readbacks.txt");
    }
  }
  void save(const FrameCapture& capture, const std::string& name) {
    if (!directory.empty()) capture.writePpm(directory / (name + ".ppm"));
  }
  void report(const std::string& text) {
    std::cout << text << '\n';
    if (observations.is_open()) observations << text << '\n';
  }
};
void captureFrame(Renderer& renderer, FrameCapture& capture, const FrameRequest& frame) {
  capture.requested = true;
  auto request = frame;
  for (unsigned attempt = 0; attempt < 8 && capture.requested; ++attempt) {
    require(runtimeContinuesAfter(renderer.renderFrame(request)), "Unexpected runtime outcome");
    request.framebuffer_resized = false;
  }
  require(!capture.requested && !capture.rgba.empty(), "Runtime readback did not complete");
}
void captureFrame(EditorRenderer& renderer, FrameCapture& capture, const FrameRequest& frame) {
  capture.requested = true;
  auto request = frame;
  for (unsigned attempt = 0; attempt < 8 && capture.requested; ++attempt) {
    renderer.beginUiFrame();
    ImGui::NewFrame();
    ImGui::Render();
    require(runtimeContinuesAfter(renderer.renderFrame(request)), "Unexpected editor outcome");
    request.framebuffer_resized = false;
  }
  require(!capture.requested && !capture.rgba.empty(), "Editor readback did not complete");
}
void run(const std::filesystem::path& root, Evidence& evidence) {
  ValidationDiagnostics diagnostics;
  std::vector<std::string> events;
  setLifecycleLog(&events);
  try {
    {
      Platform platform;
      Window window(platform, 960, 540, "Character animation validation");
      window.pollEvents();
      const auto asset = loadCharacterAsset(root / "characters/test-mannequin.glb");
      auto doc = character_fixture::controlScene();
      DoorDefinition control_door;
      control_door.id = "control-door";
      control_door.hinge_position = {3, .02F, -.5F};
      control_door.closed_yaw_degrees = 25;
      doc.doors = {control_door};
      const auto level = makePrototypeLevel(doc);
      auto enabled = initialPointLightEnabled(doc.environment_light);
      FrameCapture capture;
      RendererResources resources{root / "shaders/prototype_scene_vertex.spv",
                                  root / "shaders/prototype_scene_fragment.spv", root};
      resources.capture = &capture;
      resources.characters = {{1, asset}};
      auto pose = evaluateCharacterPose(*asset, characterRestPose(*asset));
      std::array<CharacterPoseFrame, 1> characters{{{1, asset->skeleton_identity, pose, {}, 0}}};
      FrameRequest frame{window.framebufferExtent(), false,
                         character_fixture::camera({0, 2.2F, 5}, {0, .9F, 0}, 16.F / 9), {}, enabled};
      frame.characters = characters;
      {
        const auto uploads_before = count(events, "character.indices.uploaded");
        Renderer renderer(window, frame.framebuffer, level, resources, diagnostics);
        require(renderer.validationEnabled(), "Character smoke requires Vulkan validation");
        captureFrame(renderer, capture, frame);
        require(count(events, "character.indices.uploaded") == uploads_before + 1,
                "Selected character indices were not initialized once");
        const auto retained_indices = indexEvents(events);
        evidence.save(capture, "bind");
        for (const auto& clip : asset->clips) {
          std::vector<std::uint8_t> initial;
          for (const double phase : {0., .25, .5, .75, .999999}) {
            pose = evaluateCharacterPose(*asset, sampleCharacterPose(*asset, clip.id, clip.duration * phase));
            characters[0].joint_globals = pose;
            captureFrame(renderer, capture, frame);
            const auto name = clip.id + "-" + std::to_string(static_cast<int>(phase * 100));
            evidence.save(capture, name);
            if (phase == 0) initial = capture.rgba;
            else evidence.report(name + " changed pixels=" + std::to_string(changed(initial, capture.rgba)));
          }
        }
        CharacterPlayback playback(asset, "walk");
        playback.seek(.4);
        playback.selectClip("interact");
        (void)playback.advance(.06);
        pose = playback.pose();
        characters[0].joint_globals = pose;
        captureFrame(renderer, capture, frame);
        const auto before = capture.rgba;
        evidence.save(capture, "transition-before-interruption");
        playback.selectClip("idle");
        pose = playback.pose();
        characters[0].joint_globals = pose;
        captureFrame(renderer, capture, frame);
        require(capture.rgba == before, "Interrupted transition changed its initial displayed pose");
        (void)playback.advance(.075);
        pose = playback.pose(); characters[0].joint_globals = pose;
        captureFrame(renderer, capture, frame);
        evidence.save(capture, "transition-interrupted-middle");
        const auto paused = capture.rgba;
        const auto resources_before = count(events, "character.resources.created");
        renderer.requestSwapchainRecreation();
        captureFrame(renderer, capture, frame);
        require(capture.rgba == paused, "Recovery altered the supplied pose/lighting");
        require(count(events, "character.resources.created") == resources_before,
                "Recovery recreated immutable character resources");
        require(indexEvents(events) == retained_indices,
                "Animation or recovery replaced or rewrote immutable indices");
        {
          Failure alternate("alternate_surface_format");
          renderer.requestSwapchainRecreation();
          captureFrame(renderer, capture, frame);
          require(count(events, "swapchain.surface_format.alternate.selected") > 0,
                  "Alternate attachment format was not exercised");
        }
        renderer.requestSwapchainRecreation();
        captureFrame(renderer, capture, frame);
        require(capture.rgba == paused, "Attachment format recovery changed the retained pose");
        require(count(events, "character.resources.created") == resources_before,
                "Attachment format recovery rebuilt character resources");
        require(indexEvents(events) == retained_indices,
                "Attachment format recovery replaced or rewrote immutable indices");
        window.setSize(800, 600); window.pollEvents();
        frame.framebuffer = window.framebufferExtent(); frame.framebuffer_resized = true;
        captureFrame(renderer, capture, frame);
        window.minimize(); window.pollEvents();
        auto zero = frame; zero.framebuffer = {};
        require(renderer.renderFrame(zero) == FrameOutcome::Skipped, "Zero extent was not skipped");
        window.restore(); window.setSize(960, 540); window.pollEvents();
        frame.framebuffer = window.framebufferExtent(); frame.framebuffer_resized = true;
        captureFrame(renderer, capture, frame); frame.framebuffer_resized = false;
        require(capture.rgba == paused, "Resize/minimize/restore changed frozen pose");
        require(indexEvents(events) == retained_indices,
                "Resize/minimize/restore replaced or rewrote immutable indices");
        evidence.save(capture, "recovered-frozen");
        const auto draws = count(events, "character.vertices.drawn");
        frame.characters = {};
        bool rejected{};
        try { (void)renderer.renderFrame(frame); } catch (const std::exception&) { rejected = true; }
        require(rejected && count(events, "character.vertices.drawn") == draws,
                "Missing pose was not rejected before drawing");
        frame.characters = characters;
        {
          Failure failure("character_upload");
          rejected = false;
          try { (void)renderer.renderFrame(frame); } catch (const std::exception&) { rejected = true; }
          require(rejected, "Character upload failure was not exercised");
        }
        captureFrame(renderer, capture, frame);
        require(capture.rgba == paused, "Failed upload damaged the next frame");
        require(indexEvents(events) == retained_indices,
                "Vertex upload failure changed immutable indices");
      }
      balanced(events);
      // Independent off-screen receiver readback: no deformed vertex projects
      // into this downward-facing camera, so changing pixels must be shadows.
      frame.camera = character_fixture::camera({1.8F, 2, -1.8F}, {1.8F, 0, -1.79F}, 16.F / 9);
      std::vector<std::uint8_t> light_only, idle_shadow;
      std::array<OpaqueBoxFrame, 1> fixed_door{{{{3, 1, -.5F}, {.04F, 1, .5F}, 25}}};
      frame.opaque_boxes = fixed_door;
      std::vector<std::uint8_t> ambient_control, flashlight_control;
      {
        auto empty = resources; empty.characters.clear();
        const auto absent_indices = indexEvents(events);
        Renderer renderer(window, frame.framebuffer, level, empty, diagnostics);
        frame.characters = {};
        captureFrame(renderer, capture, frame);
        light_only = capture.rgba;
        evidence.save(capture, "offscreen-light-only");
        enabled[0] = 0;
        captureFrame(renderer, capture, frame);
        ambient_control = capture.rgba;
        evidence.save(capture, "static-door-ambient-control");
        frame.spot_light = character_fixture::flashlight({1.8F, 2, -1.8F}, {1.8F, 0, -1.79F});
        captureFrame(renderer, capture, frame);
        flashlight_control = capture.rgba;
        evidence.save(capture, "static-door-flashlight-control");
        frame.spot_light = {}; enabled[0] = 1;
        require(indexEvents(events) == absent_indices,
                "Empty runtime selection allocated character indices");
      }
      {
        Renderer renderer(window, frame.framebuffer, level, resources, diagnostics);
        frame.characters = characters;
        for (const auto& sample : {std::pair{"idle", .5}, std::pair{"interact", .9}}) {
          pose = evaluateCharacterPose(*asset, sampleCharacterPose(*asset, sample.first, sample.second));
          characters[0].joint_globals = pose;
          require(visibleVertices(*asset, pose, frame.camera) == 0,
                  "Off-screen control contains visible mannequin vertices");
          captureFrame(renderer, capture, frame);
          const auto difference = changed(light_only, capture.rgba);
          evidence.report(std::string("Off-screen ") + sample.first + " shadow pixels=" + std::to_string(difference));
          require(difference > 50, "Off-screen character did not cast a visible receiver shadow");
          evidence.save(capture, std::string("offscreen-") + sample.first);
          if (std::string_view(sample.first) == "idle") idle_shadow = capture.rgba;
          else require(changed(idle_shadow, capture.rgba) > 20, "Animated off-screen shadow did not move");
        }
        enabled[0] = 0;
        captureFrame(renderer, capture, frame);
        require(capture.rgba == ambient_control, "Off-screen character changed static/door ambient control");
        frame.spot_light = character_fixture::flashlight({1.8F, 2, -1.8F}, {1.8F, 0, -1.79F});
        captureFrame(renderer, capture, frame);
        require(capture.rgba == flashlight_control, "Off-screen character changed static/door flashlight control");
        frame.spot_light = {}; enabled[0] = 1;
      }
      frame.opaque_boxes = {};
      for (const auto stage : character_allocation_failures) {
        bool rejected{};
        {
          Failure failure(stage);
          try { Renderer renderer(window, frame.framebuffer, level, resources, diagnostics); }
          catch (const std::exception&) { rejected = true; }
        }
        require(rejected, std::string("Character allocation hook was not exercised: ") + stage);
        balanced(events);
      }
      // Four independent palettes, yaw and colors survive both fenced slots.
      std::array<CharacterPose, 4> poses;
      std::vector<CharacterPoseFrame> four;
      resources.characters.clear();
      for (unsigned i = 0; i < 4; ++i) {
        resources.characters.push_back({i + 1, asset});
        poses[i] = evaluateCharacterPose(*asset, sampleCharacterPose(*asset, i % 2 ? "interact" : "walk", .2 * i));
        four.push_back({i + 1, asset->skeleton_identity, poses[i],
                       {(static_cast<float>(i % 2) - .5F) * 1.6F, 0, -static_cast<float>(i / 2) * 1.6F}, i * 35.F});
      }
      frame.camera = character_fixture::camera({0, 2.5F, 6}, {0, .8F, -.8F}, 16.F / 9);
      frame.characters = four;
      {
        Renderer renderer(window, frame.framebuffer, level, resources, diagnostics);
        for (unsigned i = 0; i < 6; ++i) captureFrame(renderer, capture, frame);
        evidence.save(capture, "four-independent");
      }
      const auto four_shared = capture.rgba;
      {
        // A distinct asset with the same skeleton and exactly the same triangles,
        // but incompatible vertex numbering, must preserve color and shadows.
        auto remapped = std::make_shared<CharacterAsset>(*asset);
        std::reverse(remapped->vertices.begin(), remapped->vertices.end());
        const auto last_vertex = static_cast<std::uint32_t>(remapped->vertices.size() - 1);
        for (auto& index : remapped->indices) index = last_vertex - index;
        auto mixed_resources = resources;
        mixed_resources.characters[1].asset = remapped;
        mixed_resources.characters[3].asset = remapped;
        auto reordered = four;
        std::reverse(reordered.begin(), reordered.end());
        frame.characters = reordered;
        Renderer renderer(window, frame.framebuffer, level, mixed_resources, diagnostics);
        const auto retained_indices = indexEvents(events);
        for (unsigned i = 0; i < 6; ++i) {
          captureFrame(renderer, capture, frame);
          require(capture.rgba == four_shared,
                  "Distinct asset indices or reordered palettes changed equivalent triangles/materials/shadows");
        }
        require(indexEvents(events) == retained_indices,
                "Fenced slot reuse replaced or rewrote immutable mixed-asset indices");
        evidence.save(capture, "four-mixed-equivalent");
        frame.characters = four;
      }
      ImGui::CreateContext();
      try {
        ImGui::GetIO().IniFilename = nullptr;
        ImGui::GetIO().DisplaySize = {960, 540};
        ImGui::GetIO().DeltaTime = 1.F / 60;
        {
          EditorRenderer renderer(window, frame.framebuffer,
              {resources.vertex_shader, resources.fragment_shader, root, &capture}, diagnostics);
          renderer.replaceDocument(doc, resources.characters);
          captureFrame(renderer, capture, frame);
          const auto prior = capture.rgba;
          for (const auto stage : character_allocation_failures) {
            const auto before_failure = indexEvents(events);
            {
              Failure failure(stage);
              bool rejected{};
              try { renderer.replaceDocument(doc, std::span<const CharacterRenderInstance>(resources.characters).first(1)); }
              catch (const std::exception&) { rejected = true; }
              require(rejected, std::string("Editor candidate allocation did not fail: ") + stage);
            }
            const auto after_failure = indexEvents(events);
            require(after_failure[0] - before_failure[0] == after_failure[1] - before_failure[1] &&
                        after_failure[2] - before_failure[2] == after_failure[3] - before_failure[3],
                    std::string("Failed editor candidate leaked index resources: ") + stage);
            captureFrame(renderer, capture, frame);
            require(capture.rgba == prior,
                    std::string("Failed editor candidate did not retain coherent prior scene/pose: ") + stage);
            require(indexEvents(events) == after_failure,
                    std::string("Rendering retained editor scene rebuilt indices after: ") + stage);
          }
          const auto retained_indices = indexEvents(events);
          renderer.requestSwapchainRecreation();
          captureFrame(renderer, capture, frame);
          require(capture.rgba == prior, "Editor recovery changed character presentation");
          require(indexEvents(events) == retained_indices,
                  "Editor recovery replaced or rewrote immutable indices");
          evidence.save(capture, "editor-retained-four");
          renderer.replaceDocument(doc);
          const auto empty_indices = indexEvents(events);
          require(empty_indices[0] == retained_indices[0] &&
                      empty_indices[2] == retained_indices[2] &&
                      empty_indices[4] == retained_indices[4],
                  "Empty editor replacement allocated or uploaded character indices");
          require(empty_indices[0] == empty_indices[1] && empty_indices[2] == empty_indices[3],
                  "Empty editor replacement retained index resources");
          frame.characters = {};
          captureFrame(renderer, capture, frame);
          require(changed(prior, capture.rgba) > 50, "Empty editor replacement retained character geometry");
          require(indexEvents(events) == empty_indices,
                  "Empty editor frame allocated character indices");
        }
      } catch (...) { ImGui::DestroyContext(); throw; }
      ImGui::DestroyContext();
    }
    balanced(events);
    require(diagnostics.errorCount() == 0, "Vulkan character validation errors including final teardown");
    evidence.report("PASS: clip readbacks, interrupted blend, off-screen moving shadows, one/four/mixed/empty, reordered palettes, vertex/index resource failures, immutable indices through resize/recovery, editor transaction and final teardown");
  } catch (...) { setLifecycleLog(nullptr); throw; }
  setLifecycleLog(nullptr);
}
template <class Char> int entry(int argc, Char** argv) {
  try {
    require(argc <= 2, "Usage: character_animation_smoke [new-capture-directory]");
    Evidence evidence(argc == 2 ? std::filesystem::absolute(std::filesystem::path(argv[1])) : std::filesystem::path{});
    run(launcher::executableResourceRoot(), evidence);
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
}  // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return entry(argc, argv); }
#else
int main(int argc, char** argv) { return entry(argc, argv); }
#endif
