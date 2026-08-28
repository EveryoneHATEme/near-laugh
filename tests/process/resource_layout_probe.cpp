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
        root / "shaders" / "triangle_vertex.spv";
    const std::filesystem::path fragment =
        root / "shaders" / "triangle_fragment.spv";
    if (!std::filesystem::is_regular_file(vertex) ||
        !std::filesystem::is_regular_file(fragment)) {
      std::cerr << "Executable-relative shader resources are missing beneath: "
                << root << '\n';
      return 1;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
