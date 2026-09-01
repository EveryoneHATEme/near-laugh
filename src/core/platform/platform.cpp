#include "core/platform/platform.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <string>

#include "core/testing/test_controls.hpp"

namespace {
void reportGlfwError(int code, const char* description) {
  std::cerr << "GLFW error " << code << ": "
            << (description != nullptr ? description : "unknown error") << '\n';
}
}  // namespace

Platform::Platform() {
  if (forcedPlatformInitializationFailure()) {
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
  recordLifecycleEvent("platform.created");
}

Platform::~Platform() {
  if (initialized_) {
    glfwTerminate();
    recordLifecycleEvent("platform.destroyed");
  }
}
