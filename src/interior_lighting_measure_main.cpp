#include <cmath>
#include <filesystem>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

#include "core/development/frame_timings.hpp"
#include "core/engine.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "launcher/executable_path.hpp"

// Development-only driver of the ordinary runtime. Its waypoint route uses
// normal sampled player actions, physics and interaction; it never teleports.
struct InteriorLightingMeasurement {
  unsigned stage{};
  double stage_start{};
  bool pressed{};
  bool light_before{}, flashlight_before{};
  unsigned completed_routes{};
  unsigned accepted_switches{}, accepted_doors{}, accepted_flashlights{};

  static void aim(Engine& engine, PlayerActionSnapshot& input,
                  WorldPosition at) {
    const auto eye = engine.player_.viewPose(1).position;
    const double dx = at.x - eye.x, dy = at.y - eye.y, dz = at.z - eye.z;
    const double degrees = 180 / std::numbers::pi;
    const double yaw = std::atan2(dz, dx) * degrees;
    const double pitch = std::atan2(dy, std::hypot(dx, dz)) * degrees;
    input.look_delta_x =
        std::remainder(yaw - engine.player_.yawDegrees(), 360.0) * 10;
    input.look_delta_y = (engine.player_.pitchDegrees() - pitch) * 10;
  }

  PlayerActionSnapshot route(Engine& engine, double now,
                             FrameTimingSample& row) {
    PlayerActionSnapshot input;
    const auto advance = [&] {
      ++stage;
      stage_start = now;
      pressed = false;
    };
    const auto walk = [&](float x, float z) {
      const auto feet = engine.player_.state().foot_position;
      row.action = "walk-" + std::to_string(stage);
      if (std::hypot(x - feet.x, z - feet.z) < .12F) {
        advance();
        return;
      }
      aim(engine, input, {x, feet.y + player_standing_eye_height, z});
      input.move_forward = true;
    };
    const auto door = [&](std::size_t index, bool open) {
      row.action = "door-" + std::to_string(stage);
      aim(engine, input,
          doorLeafPose(engine.level_.doors().at(index),
                       engine.doors_.state(index).angle)
              .center);
      // First aim and release, then one edge; leave time for accepted motion.
      if (now - stage_start > .15 && !pressed) {
        input.interact = true;
        pressed = true;
      }
      if (now - stage_start > 1.6) {
        const float target =
            open ? engine.level_.doors()[index].open_angle_degrees : 0;
        if (engine.doors_.state(index).moving ||
            std::abs(engine.doors_.state(index).angle - target) > .01F)
          throw std::runtime_error(
              "Measurement door did not reach its requested endpoint: angle " +
              std::to_string(engine.doors_.state(index).angle));
        ++accepted_doors;
        advance();
      }
    };
    switch (stage) {
      case 0:
        walk(-3.0F, 3.5F);
        break;
      case 1:
        door(0, true);
        break;
      case 2:
        walk(-2.3F, 4.05F);
        break;
      case 3: {
        const auto& light_switch = engine.level_.lightSwitches().at(0);
        const auto& lights = engine.level_.environmentLight().point_lights;
        const auto target =
            std::find_if(lights.begin(), lights.end(), [&](const auto& light) {
              return light.id == light_switch.light_id;
            });
        const auto target_index =
            static_cast<std::size_t>(target - lights.begin());
        row.action = "switch";
        aim(engine, input, light_switch.position);
        if (now - stage_start > .15 && !pressed) {
          light_before =
              engine.light_switch_.pointLightEnabled().at(target_index);
          input.interact = true;
          pressed = true;
        }
        if (now - stage_start > .5) {
          if (engine.light_switch_.pointLightEnabled().at(target_index) ==
              light_before)
            throw std::runtime_error("Measurement switch was not activated");
          ++accepted_switches;
          advance();
        }
        break;
      }
      case 4:
        walk(-3.0F, 3.5F);
        break;
      case 5:
        walk(0, 3.5F);
        break;
      case 6:
        walk(0, -2.3F);
        break;
      case 7:
        if (engine.level_.doors().size() > 1)
          door(1, false);
        else
          advance();
        break;
      case 8:
        if (engine.level_.doors().size() > 1)
          door(1, true);
        else
          advance();
        break;
      case 9:
        walk(1.7F, -2.3F);
        break;
      case 10:
        walk(2.5F, -1.9F);
        break;
      case 11:
        row.action = "flashlight";
        if (!pressed) flashlight_before = engine.flashlight_.enabled();
        input.primary_action = !pressed;
        pressed = true;
        if (now - stage_start > .3) {
          if (engine.flashlight_.enabled() == flashlight_before)
            throw std::runtime_error("Measurement flashlight did not toggle");
          ++accepted_flashlights;
          advance();
        }
        break;
      case 12:
        walk(1.7F, -2.3F);
        break;
      case 13:
        walk(0, -2.3F);
        break;
      case 14:
        walk(0, -16.2F);
        break;
      case 15:
        walk(0, -5);
        break;
      case 16:
        walk(0, 3.5F);
        break;
      case 17:
        walk(-3.0F, 3.5F);
        break;
      case 18:
        door(0, false);
        break;
      default:
        ++completed_routes;
        stage = 0;
        stage_start = now;
        break;
    }
    if (now - stage_start > 15)
      throw std::runtime_error("Measurement route stuck at stage " +
                               std::to_string(stage));
    return input;
  }

  void run(Engine& engine, FrameTimings& timings, const std::string& mode) {
    engine.window_.useFullscreenMode(1920, 1080, 60);
    engine.window_.pollEvents();
    std::cout << "Validation "
              << (engine.renderer_.validationEnabled() ? "enabled" : "disabled")
              << "; FIFO; mode " << mode << "; warmup 10 s; sample 60 s\n";
    unsigned frames{};
    while (true) {
      const auto now = FrameTimings::Clock::now();
      const double seconds = timings.elapsedSeconds(now);
      if (seconds >= 70 || (mode == "check" && frames >= 40)) break;
      if (engine.window_.framebufferExtent().isZero())
        throw std::runtime_error("Measurement interrupted by minimization");
      timings.beginFrame(now);
      auto& row = timings.current();
      row.action = "stationary";
      const auto input = mode == "route" ? route(engine, seconds, row)
                                         : PlayerActionSnapshot{};
      if (mode == "check" && frames == 20)
        engine.renderer_.requestSwapchainRecreation();
      const bool running = engine.tick(&input);
      timings.endFrame(FrameTimings::Clock::now());
      if (row.submitted && (row.width != 1920 || row.height != 1080))
        throw std::runtime_error(
            "Measurement requires an actual 1920x1080 framebuffer");
      if (!running)
        throw std::runtime_error("Measurement window closed before completion");
      ++frames;
    }
    if (mode == "route" && completed_routes == 0)
      throw std::runtime_error(
          "Measurement did not complete its walking route");
    std::cout << "Captured " << frames << " frames; completed routes "
              << completed_routes << "; accepted door endpoints "
              << accepted_doors << "; switch toggles " << accepted_switches
              << "; flashlight toggles " << accepted_flashlights << '\n';
  }
};

template <class Char>
int measure(int argc, Char** argv) {
  try {
    if (argc != 4)
      throw std::invalid_argument(
          "Usage: interior_lighting_measure <level> <stationary|route|check> "
          "<output.csv>");
    const auto mode = std::filesystem::path(argv[2]).string();
    if (mode != "stationary" && mode != "route" && mode != "check")
      throw std::invalid_argument("Unknown measurement mode");
    const auto output =
        std::filesystem::absolute(std::filesystem::path(argv[3]));
    if (std::filesystem::exists(output))
      throw std::invalid_argument(
          "Measurement output already exists; choose a fresh path");
    near_laugh::RuntimeConfig config;
    config.resource_root = launcher::executableResourceRoot();
    config.level_path =
        std::filesystem::absolute(std::filesystem::path(argv[1]));
    config.window_width = 1920;
    config.window_height = 1080;
    config.window_title = "P10 lighting measurement";
    FrameTimings timings;
    ValidationDiagnostics diagnostics;
    try {
      Engine engine(config, diagnostics, false, AudioOutput::Silent, &timings);
      InteriorLightingMeasurement{}.run(engine, timings, mode);
    } catch (...) {
      timings.writeCsv(output);  // Retain partial evidence, but fail the run.
      throw;
    }
    timings.writeCsv(
        output);  // Renderer teardown has drained the last queries.
    if (diagnostics.errorCount() != 0)
      throw std::runtime_error("Vulkan validation errors including teardown");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv) { return measure(argc, argv); }
#else
int main(int argc, char** argv) { return measure(argc, argv); }
#endif
