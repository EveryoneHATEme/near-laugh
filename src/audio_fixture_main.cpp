#include <iostream>
#include <string_view>

#include "core/engine.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "launcher/executable_path.hpp"

int main(int argc, char** argv) {
  try {
    const bool silent = argc == 2 && std::string_view(argv[1]) == "--silent";
    if (argc > 1 && !silent)
      throw std::runtime_error("Usage: audio_captions_fixture [--silent]");
    near_laugh::RuntimeConfig config;
    config.resource_root = launcher::executableResourceRoot();
    config.level_path =
        config.resource_root / "levels/audio-captions.level.json";
    config.window_title =
        "P04 audio and captions - F5 restart, M mute, P pause";
    ValidationDiagnostics diagnostics;
    {
      Engine engine(config, diagnostics, true,
                    silent ? AudioOutput::Silent : AudioOutput::Device);
      engine.run();
    }
    return diagnostics.errorCount() ? 2 : 0;
  } catch (const std::exception& error) {
    std::cerr << "Audio fixture: " << error.what() << '\n';
    return 1;
  }
}
