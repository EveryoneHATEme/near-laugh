#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "core/development/frame_capture.hpp"
#include "core/development/frame_timings.hpp"
#include "core/gameplay/character_controller.hpp"
#include "core/gameplay/door_controller.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/player/player_controller.hpp"
#include "core/render/renderer.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/simulation/fixed_step.hpp"
#include "core/world/light_switch.hpp"
#include "development/character_fixture.hpp"
#include "development/scripted_character_measurement.hpp"
#include "launcher/executable_path.hpp"

namespace {
void measure(const std::filesystem::path& root, unsigned count, bool check,
             FrameTimings& timings, FrameCapture* capture,
             ValidationDiagnostics& diagnostics) {
  Platform platform;
  Window window(platform, 1920, 1080, "P07b scripted character measurement");
  window.useFullscreenMode(1920, 1080, 60);
  const auto level =
      makePrototypeLevel(scriptedCharacterMeasurement(root, count));
  const auto assets = prepareCharacterAssets(root, level.characters());
  auto content = prepareAudioContent(root, level.audio());
  validateCharacterAudio(content, level.audio(), level.characters());
  CueCoordinator audio(level.audio(), level.doors(), std::move(content),
                       AudioOutput::Offline, 0);
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  DoorController doors(level.doors());
  CharacterController characters(level.characters(), assets, physics, audio);
  const auto enabled = initialPointLightEnabled(level.environmentLight());
  Renderer renderer(window, window.framebufferExtent(), level,
                    {root / "shaders/prototype_scene_vertex.spv",
                     root / "shaders/prototype_scene_fragment.spv",
                     root,
                     {},
                     &timings,
                     capture,
                     characters.renderInstances()},
                    diagnostics);
  // Drain startup focus/iconify notifications before the timed interval.
  // Any subsequent minimization still invalidates the entire sample.
  window.pollEvents();
  if (window.framebufferExtent().isZero()) {
    window.restore();
    window.pollEvents();
  }
  std::cout << "Validation "
            << (renderer.validationEnabled() ? "enabled" : "disabled")
            << "; FIFO; scripted actors " << count
            << "; 1920x1080/60 Hz; warmup 10 s; sample 60 s; offline mixer\n";
  FixedStepAccumulator accumulator;
  using Clock = FrameTimings::Clock;
  auto previous = Clock::now();
  double active_time{}, audio_frames{};
  std::array<float, 2048> pcm{};
  unsigned frames{};
  std::array<double, 4> distances{};
  std::array<std::uint64_t, 4> completions{};
  while ((!check && timings.elapsedSeconds(Clock::now()) < 70) ||
         (check && frames < 40)) {
    const auto now = Clock::now();
    timings.beginFrame(now);
    window.pollEvents();
    const auto extent = window.framebufferExtent();
    if (window.shouldClose() || extent.width != 1920 || extent.height != 1080)
      throw std::runtime_error(
          "Measurement interrupted or framebuffer is not 1920x1080");
    const auto elapsed = std::chrono::duration<double>(now - previous).count();
    previous = now;
    active_time += elapsed;
    const auto batch = accumulator.advance(elapsed);
    auto& row = timings.current();
    for (int step = 0; step < batch.complete_steps; ++step) {
      auto begin = Clock::now();
      physics.advanceWorld(1.F / 60);
      player.fixedStep(1.F / 60);
      row.world_player_doors_ms +=
          std::chrono::duration<double, std::milli>(Clock::now() - begin)
              .count();
      begin = Clock::now();
      for (std::size_t i = 0; i < count; ++i) {
        const auto& result = characters.result(i);
        if (result.action == CharacterAction::Completed) {
          distances[i] += result.walked_distance;
          ++completions[i];
          const auto& id = level.characters().actors[i].id;
          const auto route =
              id + (result.route.ends_with("-out") ? "-back" : "-out");
          (void)characters.start(id, route);
        }
      }
      row.route_decision_ms +=
          std::chrono::duration<double, std::milli>(Clock::now() - begin)
              .count();
      characters.fixedStep(1.F / 60, &row);
      begin = Clock::now();
      doors.fixedStep(1.F / 60, physics);
      row.world_player_doors_ms +=
          std::chrono::duration<double, std::milli>(Clock::now() - begin)
              .count();
    }
    const auto audio_begin = Clock::now();
    audio.update(active_time);
    audio.listener({-3.15F, 5.65F, 5.85F}, {0, -.5F, -1});
    characters.handoffAudio();
    // Exercise the real mixer at 48 kHz without an output-device callback.
    audio_frames += elapsed * 48000;
    while (audio_frames >= 1) {
      const auto size = static_cast<std::size_t>(std::min(audio_frames, 1024.));
      audio.playback().render(std::span(pcm).first(size * 2));
      audio_frames -= static_cast<double>(size);
    }
    row.character_audio_ms +=
        std::chrono::duration<double, std::milli>(Clock::now() - audio_begin)
            .count();
    row.action = "scripted-" + std::to_string(count);
    FrameRequest frame{
        extent,
        window.consumeFramebufferResize(),
        character_fixture::camera({-3.15F, 5.65F, 5.85F}, {-3.15F, 3.8F, 2.7F},
                                  1920.F / 1080),
        {},
        enabled};
    frame.opaque_boxes = doors.presentation();
    frame.characters = characters.presentation();
    // Keep the exact P07a color workload: no caption overlay in timing runs.
    if (check && frames == 20) renderer.requestSwapchainRecreation();
    if (check && frames == 30) capture->requested = true;
    if (!runtimeContinuesAfter(renderer.renderFrame(frame)))
      throw std::runtime_error("Unexpected render outcome");
    timings.endFrame(Clock::now());
    ++frames;
  }
  for (unsigned i = 0; i < count; ++i) {
    distances[i] += characters.result(i).walked_distance;
    std::cout << level.characters().actors[i].id << ": accepted distance "
              << distances[i] << " m; completed legs " << completions[i]
              << '\n';
    if (!check && (distances[i] < 4 || completions[i] < 4))
      throw std::runtime_error(
          "Measurement actor did not repeatedly traverse its lane");
  }
  std::cout << "Captured " << frames << " frames\n";
}
template <class Char>
int run(int argc, Char** argv) {
  try {
    if (argc != 4)
      throw std::invalid_argument(
          "Usage: scripted_character_measure --check|--measure <0|1|4> "
          "<new.csv>");
    const auto mode = std::filesystem::path(argv[1]).string();
    const auto count_text = std::filesystem::path(argv[2]).string();
    if ((mode != "--check" && mode != "--measure") ||
        (count_text != "0" && count_text != "1" && count_text != "4"))
      throw std::invalid_argument("Invalid measurement mode or actor count");
    const auto count = static_cast<unsigned>(std::stoul(count_text));
    const auto output =
        std::filesystem::absolute(std::filesystem::path(argv[3]));
    auto capture_output = output;
    capture_output.replace_extension(".ppm");
    const bool check = mode == "--check";
    if (std::filesystem::exists(output) ||
        (check && std::filesystem::exists(capture_output)))
      throw std::invalid_argument("Choose fresh output paths");
    FrameTimings timings;
    FrameCapture capture;
    ValidationDiagnostics diagnostics;
    try {
      measure(launcher::executableResourceRoot(), count, check, timings,
              check ? &capture : nullptr, diagnostics);
    } catch (...) {
      timings.writeCsv(output);
      throw;
    }
    timings.writeCsv(output);
    if (check) capture.writePpm(capture_output);
    if (diagnostics.errorCount())
      throw std::runtime_error("Vulkan validation errors through teardown");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
}  // namespace
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return run(argc, argv); }
#else
int main(int argc, char** argv) { return run(argc, argv); }
#endif
