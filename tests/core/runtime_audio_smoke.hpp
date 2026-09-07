#ifndef TESTS_RUNTIME_AUDIO_SMOKE_HPP
#define TESTS_RUNTIME_AUDIO_SMOKE_HPP
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <chrono>
#include <thread>

#include "core/engine.hpp"

struct EngineAudioSmoke {
  static void constructionFailure(ValidationDiagnostics& diagnostics) {
    near_laugh::RuntimeConfig config;
    config.resource_root = std::filesystem::absolute("resources");
    config.level_path =
        config.resource_root / "levels/audio-captions.level.json";
    bool failed = false;
    try {
      Engine engine(config, diagnostics, true, AudioOutput::Device);
    } catch (const std::runtime_error&) {
      failed = true;
    }
    if (!failed)
      throw std::runtime_error(
          "Expected GPU failure after audio initialization");
  }
  static void run(ValidationDiagnostics& diagnostics) {
    near_laugh::RuntimeConfig config;
    config.resource_root = std::filesystem::absolute("resources");
    config.level_path =
        config.resource_root / "levels/audio-captions.level.json";
    config.window_width = 800;
    config.window_height = 600;
    // Merely opening the fixture file in an ordinary runtime starts no
    // sequence.
    {
      Engine ordinary(config, diagnostics, false, AudioOutput::Silent);
      if (!ordinary.tick() ||
          ordinary.audio_.status("phone-ring") != CueStatus::Idle)
        throw std::runtime_error(
            "Ordinary launch implicitly ran the audio fixture");
    }
    for (const auto output : {AudioOutput::Silent, AudioOutput::Device}) {
      Engine engine(config, diagnostics, true, output);
      if (!engine.tick())
        throw std::runtime_error("Audio runtime did not tick");
      const auto serial = engine.audio_.instance("phone-ring");
      const auto check = [&](bool value, const char* error) {
        if (!value) throw std::runtime_error(error);
      };
      engine.window_.setCursorCaptured(false);
      const auto before = engine.audio_.activeTime();
      std::this_thread::sleep_for(std::chrono::milliseconds(30));
      check(engine.tick() && engine.audio_.activeTime() > before,
            "Cursor release paused audio");
      engine.renderer_.requestSwapchainRecreation();
      check(engine.tick() && engine.audio_.instance("phone-ring") == serial,
            "Recovery replayed audio");
      engine.window_.minimize();
      engine.window_.pollEvents();
      check(engine.window_.framebufferExtent().isZero(),
            "Desktop did not minimize runtime");
      for (int repeat = 0; repeat < 2; ++repeat) {
        std::jthread wake([] {
          std::this_thread::sleep_for(std::chrono::milliseconds(120));
          glfwPostEmptyEvent();
        });
        check(engine.tick() && engine.audio_.suspended(),
              "Minimized wait did not suspend audio");
        const auto frozen = engine.audio_.activeTime();
        wake.join();
        check(engine.audio_.activeTime() == frozen,
              "Blocked event wait advanced cue time");
      }
      const auto offset = engine.audio_.offset("phone-ring");
      engine.window_.restore();
      engine.window_.pollEvents();
      check(engine.tick() && !engine.audio_.suspended(),
            "Restore did not resume audio");
      check(engine.audio_.offset("phone-ring") == offset &&
                engine.audio_.instance("phone-ring") == serial,
            "Restore changed cue offset or identity");
      engine.window_.minimize();
      engine.window_.requestClose();
      check(!engine.tick(), "Close while minimized did not terminate");
      check(engine.audio_.status("phone-ring") == CueStatus::Cancelled,
            "Closing retained an active cue");
    }
  }
};
#endif
