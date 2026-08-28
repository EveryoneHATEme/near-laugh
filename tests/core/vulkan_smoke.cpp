#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/renderer.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/testing/test_controls.hpp"

namespace {
RendererResources smokeResources() {
  const std::filesystem::path resources =
      std::filesystem::absolute("resources").lexically_normal();
  return {resources / "shaders/triangle_vertex.spv",
          resources / "shaders/triangle_fragment.spv"};
}

void requireLifecycle(const std::vector<std::string>& actual,
                      const std::vector<std::string>& expected,
                      std::string_view phase) {
  if (actual != expected) {
    std::string message = "Unexpected lifecycle order during ";
    message += phase;
    message += ': ';
    for (const std::string& event : actual) {
      message += event + ' ';
    }
    throw std::runtime_error(message);
  }
}

void setForcedVulkanStage(const char* stage) {
#if defined(_WIN32)
  if (_putenv_s("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", stage) != 0) {
    throw std::runtime_error("Failed to configure Vulkan failure injection");
  }
#else
  if (stage[0] == '\0') {
    if (unsetenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE") != 0) {
      throw std::runtime_error("Failed to clear Vulkan failure injection");
    }
  } else if (setenv("NEAR_LAUGH_FORCE_VULKAN_FAILURE_STAGE", stage, 1) != 0) {
    throw std::runtime_error("Failed to configure Vulkan failure injection");
  }
#endif
}

void runLifecycleSmoke() {
  ValidationDiagnostics diagnostics;
  std::vector<std::string> events;
  setLifecycleLog(&events);
  {
    Platform platform;
    Window window(platform, 320, 240, "near-laugh lifecycle smoke");
    Renderer renderer(window, window.framebufferExtent(), smokeResources(),
                      diagnostics);
  }
  setLifecycleLog(nullptr);
  requireLifecycle(events,
                   {"platform.created", "window.created", "renderer.created",
                    "renderer.destroyed", "window.destroyed",
                    "platform.destroyed"},
                   "normal shutdown");

  events.clear();
  setForcedVulkanStage("instance");
  setLifecycleLog(&events);
  bool renderer_failed = false;
  try {
    Platform platform;
    Window window(platform, 320, 240, "near-laugh failure smoke");
    Renderer renderer(window, window.framebufferExtent(), smokeResources(),
                      diagnostics);
  } catch (const std::runtime_error&) {
    renderer_failed = true;
  }
  setLifecycleLog(nullptr);
  setForcedVulkanStage("");
  if (!renderer_failed) {
    throw std::runtime_error(
        "Forced renderer construction failure did not occur");
  }
  requireLifecycle(events,
                   {"platform.created", "window.created", "window.destroyed",
                    "platform.destroyed"},
                   "renderer construction failure");
}
}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--lifecycle") {
      runLifecycleSmoke();
      return 0;
    }
    const bool inject_validation_error =
        argc == 2 && std::string_view(argv[1]) == "--inject-validation-error";
    ValidationDiagnostics diagnostics;
    {
      Platform platform;
      Window window(platform, 640, 480, "near-laugh Vulkan smoke");
      window.setCursorCaptured(true);
      if (!window.cursorCaptured()) {
        throw std::runtime_error("Cursor capture did not enable");
      }
      window.setCursorCaptured(false);
      if (window.cursorCaptured()) {
        throw std::runtime_error("Cursor capture did not disable");
      }
      Renderer renderer(window, window.framebufferExtent(), smokeResources(),
                        diagnostics);
      std::cout << "Smoke validation: "
                << (renderer.validationEnabled() ? "enabled" : "unavailable")
                << '\n';
      for (int frame = 0; frame < 120 && !window.shouldClose(); ++frame) {
        window.pollEvents();
        if (frame == 20) {
          window.setSize(800, 600);
        }
        if (frame == 40) {
          renderer.requestSwapchainRecreation();
        }
        if (frame == 60) {
          window.minimize();
          for (int poll = 0; poll < 20 && !window.framebufferExtent().isZero();
               ++poll) {
            window.pollEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
          }
          if (!window.framebufferExtent().isZero()) {
            std::cout << "Smoke note: desktop did not report a zero-sized "
                         "framebuffer while minimized\n";
          }
          window.restore();
          for (int poll = 0; poll < 20 && window.framebufferExtent().isZero();
               ++poll) {
            window.pollEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
          }
        }
        const FrameRequest request{window.framebufferExtent(),
                                   window.consumeFramebufferResize()};
        static_cast<void>(renderer.renderFrame(request));
      }
      if (inject_validation_error) {
        diagnostics.record(ValidationSeverity::Error,
                           ValidationCategory::Validation,
                           "injected smoke validation error");
      }
    }
    if (diagnostics.errorCount() != 0) {
      std::cerr << "Vulkan smoke failed after orderly cleanup: validation "
                << "reported " << diagnostics.errorCount() << " error(s)\n";
      return 2;
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Vulkan smoke failed: " << error.what() << '\n';
    return 1;
  }
}
