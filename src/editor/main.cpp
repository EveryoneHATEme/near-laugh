#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
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
    const bool household_smoke =
        argc >= 2 && std::filesystem::path(argv[1]) == "--household-smoke";
    if ((!smoke && !household_smoke && argc > 2) ||
        ((smoke || household_smoke) && argc > 3)) {
      std::cerr
          << "usage: level_editor [level-path]\n"
             "       level_editor --smoke [level-path]\n"
             "       level_editor --character-smoke\n"
             "       level_editor --household-smoke [capture-directory]\n";
      return 2;
    }
    const std::filesystem::path resource_root =
        launcher::executableResourceRoot();
    std::optional<std::filesystem::path> initial_level;
    if (household_smoke) {
      initial_level =
          resource_root / "levels/household-interactions.level.json";
    } else if (character_smoke) {
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
    } lifecycle(character_smoke || household_smoke);
    const std::filesystem::path household_captures =
        household_smoke && argc == 3
            ? std::filesystem::path(argv[2])
            : std::filesystem::current_path() / "household-editor-smoke" /
                  std::to_string(std::chrono::steady_clock::now()
                                     .time_since_epoch()
                                     .count());
    FrameCapture capture;
    {
      EditorApplication application(
          resource_root, initial_level, diagnostics,
          character_smoke || household_smoke ? &capture : nullptr);
      if (household_smoke)
        application.runHouseholdSmoke(lifecycle.events, capture,
                                      household_captures);
      else if (character_smoke)
        application.runCharacterSmoke(lifecycle.events, capture);
      else if (smoke)
        application.runSmoke(*initial_level);
      else
        application.run();
    }
    if (household_smoke) {
      std::ofstream evidence(household_captures / "lifecycle.txt");
      for (const auto& event : lifecycle.events) evidence << event << '\n';
      const auto count = [&](const char* event) {
        return std::count(lifecycle.events.begin(), lifecycle.events.end(),
                          event);
      };
      for (const auto& pair :
           {std::pair{"door_preview.mesh.buffer.created",
                      "door_preview.mesh.buffer.destroyed"},
            std::pair{"door_preview.mesh.memory.allocated",
                      "door_preview.mesh.memory.freed"},
            std::pair{"door_preview.mesh.created",
                      "door_preview.mesh.destroyed"},
            std::pair{"world.mesh.buffer.created",
                      "world.mesh.buffer.destroyed"},
            std::pair{"world.mesh.memory.allocated", "world.mesh.memory.freed"},
            std::pair{"texture.created", "texture.destroyed"},
            std::pair{"lighting.created", "lighting.destroyed"},
            std::pair{"shadow.image.created", "shadow.image.destroyed"},
            std::pair{"shadow.memory.allocated", "shadow.memory.freed"},
            std::pair{"character.buffer.created", "character.buffer.destroyed"},
            std::pair{"character.memory.allocated", "character.memory.freed"},
            std::pair{"character.index.buffer.created",
                      "character.index.buffer.destroyed"},
            std::pair{"character.index.memory.allocated",
                      "character.index.memory.freed"},
            std::pair{"character.resources.created",
                      "character.resources.destroyed"},
            std::pair{"editor.imgui-vulkan.created",
                      "editor.imgui-vulkan.destroyed"},
            std::pair{"editor.renderer.created", "editor.renderer.destroyed"},
            std::pair{"device.created", "device.destroyed"}}) {
        evidence << pair.first << '=' << count(pair.first) << ", "
                 << pair.second << '=' << count(pair.second) << '\n';
        if (count(pair.first) == 0 || count(pair.first) != count(pair.second))
          throw std::runtime_error(
              std::string("Unbalanced editor household lifetime: ") +
              pair.first);
      }
      evidence << "Vulkan validation errors after teardown: "
               << diagnostics.errorCount() << '\n';
      if (!evidence)
        throw std::runtime_error("Cannot write household lifecycle evidence");
      std::cout << "Editor household captures: " << household_captures << '\n';
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
