#include "launcher/executable_path.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#error Unsupported desktop host for executable path discovery
#endif

namespace launcher {
namespace {
constexpr std::size_t kInitialPathCapacity = 256;
constexpr std::size_t kMaximumPathCapacity = 1024 * 1024;

#if defined(_WIN32)
ExecutablePathProbeResult nativeExecutablePathProbe(std::size_t capacity) {
  std::vector<wchar_t> buffer(capacity);
  const DWORD length = GetModuleFileNameW(
      nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0) {
    return {{}, false,
            "GetModuleFileNameW returned error " +
                std::to_string(GetLastError())};
  }
  if (length >= buffer.size()) {
    return {{}, false, {}};
  }
  return {std::filesystem::path(buffer.data(), buffer.data() + length), true,
          {}};
}
#elif defined(__linux__)
ExecutablePathProbeResult nativeExecutablePathProbe(std::size_t capacity) {
  std::vector<char> buffer(capacity);
  const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size());
  if (length < 0) {
    return {{}, false,
            std::string("readlink(/proc/self/exe) failed: ") +
                std::strerror(errno)};
  }
  if (static_cast<std::size_t>(length) >= buffer.size()) {
    return {{}, false, {}};
  }
  return {std::filesystem::path(buffer.data(), buffer.data() + length), true,
          {}};
}
#elif defined(__APPLE__)
ExecutablePathProbeResult nativeExecutablePathProbe(std::size_t capacity) {
  std::vector<char> buffer(capacity);
  auto native_capacity = static_cast<std::uint32_t>(buffer.size());
  if (_NSGetExecutablePath(buffer.data(), &native_capacity) != 0) {
    return {{}, false, {}};
  }
  return {std::filesystem::path(buffer.data()), true, {}};
}
#endif
}  // namespace

std::filesystem::path executablePathFromProbe(ExecutablePathProbe probe) {
  if (probe == nullptr) {
    throw std::runtime_error(
        "Executable path discovery failed: no native path probe");
  }

  for (std::size_t capacity = kInitialPathCapacity;
       capacity <= kMaximumPathCapacity; capacity *= 2) {
    ExecutablePathProbeResult result = probe(capacity);
    if (!result.failure.empty()) {
      throw std::runtime_error("Executable path discovery failed: " +
                               result.failure);
    }
    if (!result.complete) {
      continue;
    }
    if (result.path.empty()) {
      throw std::runtime_error(
          "Executable path discovery failed: native path was empty");
    }
    return std::filesystem::absolute(result.path).lexically_normal();
  }

  throw std::runtime_error(
      "Executable path discovery failed: native path exceeds 1 MiB");
}

std::filesystem::path currentExecutablePath() {
  return executablePathFromProbe(nativeExecutablePathProbe);
}

std::filesystem::path executableResourceRoot() {
  return currentExecutablePath().parent_path() / "resources";
}

}  // namespace launcher
