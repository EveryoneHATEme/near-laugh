#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "core/development/frame_capture.hpp"
#include "core/testing/test_controls.hpp"
#include "editor/editor_application.hpp"
#include "launcher/executable_path.hpp"

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
  try {
    const bool smoke = argc >= 2 && std::filesystem::path(argv[1]) == "--smoke";
    const bool character_smoke =
        argc == 2 && std::filesystem::path(argv[1]) == "--character-smoke";
    if ((!smoke && argc > 2) || (smoke && argc > 3)) {
      std::cerr << "usage: level_editor [level-path]\n"
                   "       level_editor --smoke [level-path]\n";
      return 2;
    }
    const std::filesystem::path resource_root =
        launcher::executableResourceRoot();
    std::optional<std::filesystem::path> initial_level;
    if (character_smoke) {
      initial_level =
          resource_root / "levels/scripted-characters-four.level.json";
    } else if (smoke) {
      initial_level = argc == 3
                          ? std::filesystem::path(argv[2])
                          : resource_root / "levels" / "prototype.level.json";
    } else if (argc == 2) {
      initial_level = std::filesystem::path(argv[1]);
    }
    ValidationDiagnostics diagnostics;
    struct CharacterLifecycle {
      std::vector<std::string> events;
      explicit CharacterLifecycle(bool enabled) {
        if (enabled) setLifecycleLog(&events);
      }
      ~CharacterLifecycle() { setLifecycleLog(nullptr); }
    } lifecycle(character_smoke);
    FrameCapture capture;
    {
      EditorApplication application(resource_root, initial_level, diagnostics,
                                    character_smoke ? &capture : nullptr);
      if (character_smoke)
        application.runCharacterSmoke(lifecycle.events, capture);
      else if (smoke)
        application.runSmoke(*initial_level);
      else
        application.run();
    }
    if (character_smoke) {
      const auto count = [&](const char* event) {
        return std::count(lifecycle.events.begin(), lifecycle.events.end(),
                          event);
      };
      for (const auto& pair :
           {std::pair{"character.buffer.created", "character.buffer.destroyed"},
            std::pair{"character.memory.allocated", "character.memory.freed"},
            std::pair{"character.index.buffer.created",
                      "character.index.buffer.destroyed"},
            std::pair{"character.index.memory.allocated",
                      "character.index.memory.freed"},
            std::pair{"character.resources.created",
                      "character.resources.destroyed"},
            std::pair{"texture.created", "texture.destroyed"},
            std::pair{"lighting.created", "lighting.destroyed"},
            std::pair{"device.created", "device.destroyed"}}) {
        if (count(pair.first) == 0 || count(pair.first) != count(pair.second))
          throw std::runtime_error(
              std::string("Unbalanced editor character lifetime: ") +
              pair.first);
      }
      if (count("editor.renderer.destroyed") != 1)
        throw std::runtime_error(
            "Editor character smoke did not destroy its renderer");
    }
    if (diagnostics.errorCount() != 0) {
      std::cerr << "level editor recorded Vulkan validation errors\n";
      return 1;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "level editor startup/runtime failure: " << error.what()
              << '\n';
    return 1;
  } catch (...) {
    std::cerr << "level editor startup/runtime failure: unknown exception\n";
    return 1;
  }
}
