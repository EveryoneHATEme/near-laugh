#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

#include "editor/editor_application.hpp"
#include "launcher/executable_path.hpp"

int main(int argc, char** argv) {
  try {
    const bool smoke = argc >= 2 && std::string_view(argv[1]) == "--smoke";
    if ((!smoke && argc > 2) || (smoke && argc > 3)) {
      std::cerr << "usage: level_editor [level-path]\n"
                   "       level_editor --smoke [level-path]\n";
      return 2;
    }
    const std::filesystem::path resource_root =
        launcher::executableResourceRoot();
    std::optional<std::filesystem::path> initial_level;
    if (smoke) {
      initial_level = argc == 3
                          ? std::filesystem::path(argv[2])
                          : resource_root / "levels" / "prototype.level.json";
    } else if (argc == 2) {
      initial_level = std::filesystem::path(argv[1]);
    }
    EditorApplication application(resource_root, initial_level);
    if (smoke) {
      application.runSmoke(*initial_level);
    } else {
      application.run();
    }
    if (application.validationErrorCount() != 0) {
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
