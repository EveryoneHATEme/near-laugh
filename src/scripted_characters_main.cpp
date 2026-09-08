#include <iostream>
#include <string_view>

#include "core/engine.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "launcher/executable_path.hpp"

int main(int argc, char** argv) {
  try {
    bool four = false, silent = false;
    for (int i = 1; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (arg == "--four")
        four = true;
      else if (arg == "--silent")
        silent = true;
      else
        throw std::runtime_error(
            "Usage: scripted_characters [--four] [--silent]");
    }
    near_laugh::RuntimeConfig config;
    config.resource_root = launcher::executableResourceRoot();
    config.level_path = config.resource_root / "levels" /
                        (four ? "scripted-characters-four.level.json"
                              : "scripted-characters.level.json");
    config.window_title =
        "P07b characters - F5 restart routes, F6 cancel, P pause, M mute";
    ValidationDiagnostics diagnostics;
    {
      Engine engine(config, diagnostics, false,
                    silent ? AudioOutput::Silent : AudioOutput::Device, nullptr,
                    true);
      engine.run();
    }
    return diagnostics.errorCount() ? 2 : 0;
  } catch (const std::exception& error) {
    std::cerr << "Scripted characters: " << error.what() << '\n';
    return 1;
  }
}
