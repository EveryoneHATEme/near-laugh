#include "core/platform/platform.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
bool platform_active = false;

void reportGlfwError(int code, const char* description) {
  std::cerr << "GLFW error " << code << ": "
            << (description != nullptr ? description : "unknown error") << '\n';
}
}  // namespace

Platform::Platform() {
  if (platform_active) {
    throw std::runtime_error(
        "Platform initialization failed: GLFW already has an active owner");
  }
  if (forcedInitializationFailureRequested()) {
    throw std::runtime_error(
        "Platform initialization forced to fail by "
        "NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE");
  }

  glfwSetErrorCallback(reportGlfwError);
  if (glfwInit() != GLFW_TRUE) {
    const char* description = nullptr;
    const int code = glfwGetError(&description);
    throw std::runtime_error(
        "Platform initialization failed (GLFW error " + std::to_string(code) +
        "): " + (description != nullptr ? description : "unknown error"));
  }
  initialized_ = true;
  platform_active = true;
}

Platform::~Platform() {
  if (initialized_) {
    glfwTerminate();
    platform_active = false;
  }
}

bool Platform::forcedInitializationFailureRequested() noexcept {
#if defined(_WIN32)
  char* value = nullptr;
  std::size_t size = 0;
  if (_dupenv_s(&value, &size, "NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE") != 0) {
    return false;
  }
  const bool requested = value != nullptr && std::string(value) == "1";
  std::free(value);
  return requested;
#else
  const char* value = std::getenv("NEAR_LAUGH_FORCE_GLFW_INIT_FAILURE");
  return value != nullptr && std::string(value) == "1";
#endif
}
