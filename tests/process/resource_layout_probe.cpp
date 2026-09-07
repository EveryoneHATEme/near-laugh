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
    const std::filesystem::path interior =
        root / "levels" / "apartment-stairs.level.json";
    if (!std::filesystem::is_regular_file(vertex) ||
        !std::filesystem::is_regular_file(fragment) ||
        !std::filesystem::is_regular_file(root /
                                          "shaders/caption_vertex.spv") ||
        !std::filesystem::is_regular_file(root /
                                          "shaders/caption_fragment.spv")) {
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
    if (!std::filesystem::is_regular_file(level) ||
        !std::filesystem::is_regular_file(interior) ||
        !std::filesystem::is_regular_file(root /
                                          "levels/audio-captions.level.json")) {
      std::cerr << "Executable-relative level resource is missing: " << level
                << '\n';
      return 1;
    }
    for (const auto* name : {"radio", "phone-ring", "footsteps",
                             "phone-conversation", "invitation"}) {
      for (const auto& asset :
           {root / "audio" / (std::string(name) + ".wav"),
            root / "captions" / (std::string(name) + ".captions")}) {
        if (!std::filesystem::is_regular_file(asset) ||
            std::filesystem::file_size(asset) == 0) {
          std::cerr << "Executable-relative audio/caption resource missing: "
                    << asset << '\n';
          return 1;
        }
      }
    }
    for (const auto* name : {"NotoSans-Regular.ttf", "OFL.txt"}) {
      if (!std::filesystem::is_regular_file(root / "fonts" / name)) {
        std::cerr << "Executable-relative font resource missing: "
                  << root / "fonts" / name << '\n';
        return 1;
      }
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
