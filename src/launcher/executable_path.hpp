#ifndef LAUNCHER_EXECUTABLE_PATH_HPP
#define LAUNCHER_EXECUTABLE_PATH_HPP

#include <cstddef>
#include <filesystem>
#include <string>

namespace launcher {

struct ExecutablePathProbeResult {
  std::filesystem::path path{};
  bool complete{};
  std::string failure{};
};

using ExecutablePathProbe = ExecutablePathProbeResult (*)(std::size_t);

[[nodiscard]] std::filesystem::path executablePathFromProbe(
    ExecutablePathProbe probe);
[[nodiscard]] std::filesystem::path currentExecutablePath();
[[nodiscard]] std::filesystem::path executableResourceRoot();

}  // namespace launcher

#endif
