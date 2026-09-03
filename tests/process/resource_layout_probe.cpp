#include <array>
#include <filesystem>
#include <iostream>

#include "launcher/executable_path.hpp"

int main(int argc, char** argv) {
  char misleading_invocation[] = "not-the-running-executable";
  if (argc > 0 && argv != nullptr) {
    argv[0] = misleading_invocation;
  }

  try {
    const std::filesystem::path root = launcher::executableResourceRoot();
    const std::filesystem::path vertex =
        root / "shaders" / "prototype_scene_vertex.spv";
    const std::filesystem::path fragment =
        root / "shaders" / "prototype_scene_fragment.spv";
    const std::array<std::filesystem::path, 3> textures = {
        root / "textures" / "prototype_floor.png",
        root / "textures" / "prototype_boundary.png",
        root / "textures" / "prototype_obstacle.png"};
    const std::filesystem::path chair = root / "models" / "prototype_chair.glb";
    const std::filesystem::path level =
        root / "levels" / "prototype.level.json";
    if (!std::filesystem::is_regular_file(vertex) ||
        !std::filesystem::is_regular_file(fragment)) {
      std::cerr << "Executable-relative shader resources are missing beneath: "
                << root << '\n';
      return 1;
    }
    for (const std::filesystem::path& texture : textures) {
      if (!std::filesystem::is_regular_file(texture)) {
        std::cerr << "Executable-relative texture resource is missing: "
                  << texture << '\n';
        return 1;
      }
    }
    if (!std::filesystem::is_regular_file(chair)) {
      std::cerr << "Executable-relative model resource is missing: " << chair
                << '\n';
      return 1;
    }
    if (!std::filesystem::is_regular_file(level)) {
      std::cerr << "Executable-relative level resource is missing: " << level
                << '\n';
      return 1;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
