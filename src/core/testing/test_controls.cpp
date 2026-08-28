#include "core/testing/test_controls.hpp"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

namespace {
std::mutex lifecycle_mutex;
std::vector<std::string>* lifecycle_events = nullptr;

bool environmentEquals(const char* name, const char* expected) noexcept {
#if defined(_WIN32)
  char* value = nullptr;
  std::size_t size = 0;
  if (_dupenv_s(&value, &size, name) != 0) {
    return false;
  }
  const bool matches = value != nullptr && std::strcmp(value, expected) == 0;
  std::free(value);
  return matches;
#else
  const char* value = std::getenv(name);
  return value != nullptr && std::strcmp(value, expected) == 0;
#endif
}
}  // namespace

bool forcedPlatformInitializationFailure() noexcept {
  return environmentEquals("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE", "1");
}

bool forcedVulkanFailureAt(const char* stage) noexcept {
  return environmentEquals("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", stage);
}

void setLifecycleLog(std::vector<std::string>* events) noexcept {
  std::lock_guard lock(lifecycle_mutex);
  lifecycle_events = events;
}

void recordLifecycleEvent(std::string_view event) noexcept {
  try {
    std::lock_guard lock(lifecycle_mutex);
    if (lifecycle_events != nullptr) {
      lifecycle_events->emplace_back(event);
    }
  } catch (...) {
    // Test instrumentation must not affect runtime cleanup.
  }
}
