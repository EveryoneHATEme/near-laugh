#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "launcher/executable_path.hpp"

namespace {
std::vector<std::size_t> observed_capacities;

launcher::ExecutablePathProbeResult growingProbe(std::size_t capacity) {
  observed_capacities.push_back(capacity);
  if (capacity < 512) {
    return {{}, false, {}};
  }
  return {std::filesystem::current_path() / "bin" / ".." / "fps.exe", true,
          {}};
}

launcher::ExecutablePathProbeResult failingProbe(std::size_t) {
  return {{}, false, "simulated native failure"};
}
}  // namespace

TEST(ExecutablePath, GrowsNativeBufferAndNormalizesTheResult) {
  observed_capacities.clear();
  const std::filesystem::path path =
      launcher::executablePathFromProbe(growingProbe);
  EXPECT_EQ(observed_capacities, (std::vector<std::size_t>{256, 512}));
  EXPECT_EQ(path,
            (std::filesystem::current_path() / "fps.exe").lexically_normal());
}

TEST(ExecutablePath, ReportsActionableNativeDiscoveryFailure) {
  try {
    static_cast<void>(launcher::executablePathFromProbe(failingProbe));
    FAIL() << "Expected executable path discovery to fail";
  } catch (const std::runtime_error& error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("Executable path discovery failed"),
              std::string::npos);
    EXPECT_NE(message.find("simulated native failure"), std::string::npos);
  }
}

TEST(ExecutablePath, DiscoversTheRunningModuleWithoutNativeTypes) {
  const std::filesystem::path path = launcher::currentExecutablePath();
  EXPECT_TRUE(path.is_absolute());
  EXPECT_TRUE(std::filesystem::is_regular_file(path));
}
