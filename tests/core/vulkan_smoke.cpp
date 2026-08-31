#include <algorithm>
#include <array>
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

#include "core/physics/physics_world.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/player/player_controller.hpp"
#include "core/render/renderer.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/testing/test_controls.hpp"
#include "core/world/prototype_level.hpp"

namespace {
RendererResources smokeResources() {
  const std::filesystem::path resources =
      std::filesystem::absolute("resources").lexically_normal();
  return {resources / "shaders/prototype_scene_vertex.spv",
          resources / "shaders/prototype_scene_fragment.spv",
          {resources / "textures/prototype_floor.png",
           resources / "textures/prototype_boundary.png",
           resources / "textures/prototype_obstacle.png",
           resources / "textures/prototype_shooting_target.png"}};
}

void requireLifecycle(const std::vector<std::string>& actual,
                      const std::vector<std::string>& expected,
                      std::string_view phase) {
  if (actual != expected) {
    std::string message = "Unexpected lifecycle order during ";
    message += phase;
    message += ": ";
    for (const std::string& event : actual) {
      message += event + ' ';
    }
    throw std::runtime_error(message);
  }
}

std::size_t eventCount(const std::vector<std::string>& events,
                       std::string_view event) {
  return static_cast<std::size_t>(
      std::count(events.begin(), events.end(), std::string(event)));
}

void requireBalancedEvent(const std::vector<std::string>& events,
                          std::string_view created, std::string_view destroyed,
                          std::string_view phase) {
  if (eventCount(events, created) != eventCount(events, destroyed)) {
    throw std::runtime_error("Unbalanced " + std::string(created) +
                             " lifetime during " + std::string(phase));
  }
}

void requireBefore(const std::vector<std::string>& events,
                   std::string_view first, std::string_view second,
                   std::string_view phase) {
  const auto first_position =
      std::find(events.begin(), events.end(), std::string(first));
  const auto second_position =
      std::find(events.begin(), events.end(), std::string(second));
  if (first_position == events.end() || second_position == events.end() ||
      first_position >= second_position) {
    throw std::runtime_error(
        "Invalid lifecycle order between " + std::string(first) + " and " +
        std::string(second) + " during " + std::string(phase));
  }
}

void requireBalancedTextureLifecycle(const std::vector<std::string>& events,
                                     std::string_view phase) {
  requireBalancedEvent(events, "device.created", "device.destroyed", phase);
  requireBalancedEvent(events, "texture.image.created",
                       "texture.image.destroyed", phase);
  requireBalancedEvent(events, "texture.view.created", "texture.view.destroyed",
                       phase);
  requireBalancedEvent(events, "texture.sampler.created",
                       "texture.sampler.destroyed", phase);
  requireBalancedEvent(events, "texture.descriptor_layout.created",
                       "texture.descriptor_layout.destroyed", phase);
  requireBalancedEvent(events, "texture.descriptor_pool.created",
                       "texture.descriptor_pool.destroyed", phase);
  requireBalancedEvent(events, "texture.created", "texture.destroyed", phase);
}

void requireBalancedLightingLifecycle(const std::vector<std::string>& events,
                                      std::string_view phase) {
  requireBalancedEvent(events, "lighting.buffer.created",
                       "lighting.buffer.destroyed", phase);
  requireBalancedEvent(events, "lighting.memory.allocated",
                       "lighting.memory.freed", phase);
  requireBalancedEvent(events, "lighting.descriptor_layout.created",
                       "lighting.descriptor_layout.destroyed", phase);
  requireBalancedEvent(events, "lighting.descriptor_pool.created",
                       "lighting.descriptor_pool.destroyed", phase);
  requireBalancedEvent(events, "lighting.created", "lighting.destroyed", phase);
}

void requireBalancedDepthLifecycle(const std::vector<std::string>& events,
                                   std::string_view phase) {
  const auto created =
      std::count(events.begin(), events.end(), std::string{"depth.created"});
  const auto destroyed =
      std::count(events.begin(), events.end(), std::string{"depth.destroyed"});
  if (created == 0 || created != destroyed) {
    throw std::runtime_error("Unbalanced depth attachment lifetime during " +
                             std::string(phase));
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
    PrototypeLevel level;
    Renderer renderer(window, window.framebufferExtent(), level,
                      smokeResources(), diagnostics);
    renderer.requestSwapchainRecreation();
    const FrameRequest recovery_request{window.framebufferExtent(), false};
    if (renderer.renderFrame(recovery_request) != FrameOutcome::Recovered) {
      throw std::runtime_error(
          "Forced swapchain recreation did not report recovery");
    }
  }
  setLifecycleLog(nullptr);
  requireBalancedDepthLifecycle(events, "normal shutdown");
  requireBalancedTextureLifecycle(events, "normal shutdown");
  requireBalancedLightingLifecycle(events, "normal shutdown");
  if (eventCount(events, "texture.image.created") != 1 ||
      eventCount(events, "texture.sampler.created") != 1 ||
      eventCount(events, "texture.descriptor_layout.created") != 1 ||
      eventCount(events, "texture.descriptor_pool.created") != 1 ||
      eventCount(events, "texture.descriptor.updated") != 1 ||
      eventCount(events, "texture.uploaded.shader_read_only") != 1) {
    throw std::runtime_error(
        "Swapchain recreation rebuilt or incompletely initialized the "
        "prototype texture");
  }
  if (eventCount(events, "lighting.buffer.created") != 1 ||
      eventCount(events, "lighting.memory.allocated") != 1 ||
      eventCount(events, "lighting.descriptor_layout.created") != 1 ||
      eventCount(events, "lighting.descriptor_pool.created") != 1 ||
      eventCount(events, "lighting.uploaded") != 1 ||
      eventCount(events, "lighting.descriptor.updated") != 1) {
    throw std::runtime_error(
        "Swapchain recreation rebuilt or rewrote immutable prototype "
        "lighting");
  }
  requireBefore(events, "pipeline.destroyed",
                "texture.descriptor_pool.destroyed", "normal shutdown");
  requireBefore(events, "pipeline.destroyed",
                "lighting.descriptor_pool.destroyed", "normal shutdown");
  requireBefore(events, "lighting.destroyed", "device.destroyed",
                "normal shutdown");
  requireBefore(events, "texture.destroyed", "device.destroyed",
                "normal shutdown");

  events.clear();
  setForcedVulkanStage("instance");
  setLifecycleLog(&events);
  bool renderer_failed = false;
  try {
    Platform platform;
    Window window(platform, 320, 240, "near-laugh failure smoke");
    PrototypeLevel level;
    Renderer renderer(window, window.framebufferExtent(), level,
                      smokeResources(), diagnostics);
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

  events.clear();
  setForcedVulkanStage("depth");
  setLifecycleLog(&events);
  renderer_failed = false;
  try {
    Platform platform;
    Window window(platform, 320, 240, "near-laugh depth failure smoke");
    PrototypeLevel level;
    Renderer renderer(window, window.framebufferExtent(), level,
                      smokeResources(), diagnostics);
  } catch (const std::runtime_error&) {
    renderer_failed = true;
  }
  setLifecycleLog(nullptr);
  setForcedVulkanStage("");
  if (!renderer_failed) {
    throw std::runtime_error(
        "Forced depth attachment construction failure did not occur");
  }
  requireBalancedDepthLifecycle(events, "depth construction failure");
  requireBalancedTextureLifecycle(events, "depth construction failure");
  requireBalancedLightingLifecycle(events, "depth construction failure");

  constexpr std::array<const char*, 8> texture_failure_stages = {
      "texture_staging",         "texture_image",
      "texture_upload",          "texture_view",
      "texture_sampler",         "texture_descriptor_layout",
      "texture_descriptor_pool", "texture_descriptor_set"};
  for (const char* stage : texture_failure_stages) {
    events.clear();
    setForcedVulkanStage(stage);
    setLifecycleLog(&events);
    renderer_failed = false;
    try {
      Platform platform;
      Window window(platform, 320, 240, "near-laugh texture failure smoke");
      PrototypeLevel level;
      Renderer renderer(window, window.framebufferExtent(), level,
                        smokeResources(), diagnostics);
    } catch (const std::runtime_error&) {
      renderer_failed = true;
    }
    setLifecycleLog(nullptr);
    setForcedVulkanStage("");
    if (!renderer_failed) {
      throw std::runtime_error(
          "Forced texture construction failure did not occur at " +
          std::string(stage));
    }
    requireBalancedTextureLifecycle(events, stage);
    if (eventCount(events, "texture.created") != 0) {
      throw std::runtime_error(
          "Partially constructed texture reported complete ownership at " +
          std::string(stage));
    }
  }

  constexpr std::array<const char*, 6> lighting_failure_stages = {
      "lighting_buffer",          "lighting_memory",
      "lighting_upload",          "lighting_descriptor_layout",
      "lighting_descriptor_pool", "lighting_descriptor_set"};
  for (const char* stage : lighting_failure_stages) {
    events.clear();
    setForcedVulkanStage(stage);
    setLifecycleLog(&events);
    renderer_failed = false;
    try {
      Platform platform;
      Window window(platform, 320, 240, "near-laugh lighting failure smoke");
      PrototypeLevel level;
      Renderer renderer(window, window.framebufferExtent(), level,
                        smokeResources(), diagnostics);
    } catch (const std::runtime_error&) {
      renderer_failed = true;
    }
    setLifecycleLog(nullptr);
    setForcedVulkanStage("");
    if (!renderer_failed) {
      throw std::runtime_error(
          "Forced lighting construction failure did not occur at " +
          std::string(stage));
    }
    requireBalancedTextureLifecycle(events, stage);
    requireBalancedLightingLifecycle(events, stage);
    if (eventCount(events, "lighting.created") != 0) {
      throw std::runtime_error(
          "Partially constructed lighting reported complete ownership at " +
          std::string(stage));
    }
  }

  if (diagnostics.errorCount() != 0) {
    throw std::runtime_error(
        "Lifecycle smoke recorded Vulkan validation errors");
  }
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
      PrototypeLevel level;
      PhysicsWorld physics(level);
      PlayerController player(physics, level.playerSpawn().yaw_degrees);
      for (int step = 0; step < 120; ++step) {
        player.fixedStep(1.0F / 60.0F);
      }
      Renderer renderer(window, window.framebufferExtent(), level,
                        smokeResources(), diagnostics);
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
        const FramebufferExtent extent = window.framebufferExtent();
        const CameraFrame camera = player.cameraFrame(
            extent.isZero() ? 1.0F
                            : static_cast<float>(extent.width) /
                                  static_cast<float>(extent.height),
            1.0F);
        PrototypeScenePresentation presentation{};
        const std::uint32_t target_mask =
            std::uint32_t{1} << level.targetDescriptions()[0].solid_index;
        if (frame >= 30 && frame < 60) {
          presentation.highlighted_solid_mask = target_mask;
        } else if (frame >= 60 && frame < 90) {
          presentation.dimmed_solid_mask = target_mask;
        } else if (frame >= 90) {
          presentation.highlighted_solid_mask = target_mask;
          presentation.dimmed_solid_mask = target_mask;
        }
        const FrameRequest request{extent, window.consumeFramebufferResize(),
                                   camera, presentation};
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
