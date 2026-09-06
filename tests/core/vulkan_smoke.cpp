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

#include "core/engine.hpp"
#include "core/physics/physics_world.hpp"
#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/player/player_controller.hpp"
#include "core/render/renderer.hpp"
#include "core/render/sampled_texture.hpp"
#include "core/render/scene_assets.hpp"
#include "core/render/static_model_loader.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "core/render/vulkan_context.hpp"
#include "core/testing/test_controls.hpp"
#include "core/world/door.hpp"
#include "core/world/prototype_level.hpp"
#include "prototype_level_fixture.hpp"

namespace {
RendererResources smokeResources() {
  const std::filesystem::path resources =
      std::filesystem::absolute("resources").lexically_normal();
  return {resources / "shaders/prototype_scene_vertex.spv",
          resources / "shaders/prototype_scene_fragment.spv", resources};
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
  requireBalancedEvent(events, "material.buffer.created",
                       "material.buffer.destroyed", phase);
  requireBalancedEvent(events, "material.memory.allocated",
                       "material.memory.freed", phase);
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

void requireBalancedMeshLifecycle(const std::vector<std::string>& events,
                                  std::string_view name,
                                  std::string_view phase) {
  const std::string prefix = std::string(name) + ".mesh.";
  requireBalancedEvent(events, prefix + "buffer.created",
                       prefix + "buffer.destroyed", phase);
  requireBalancedEvent(events, prefix + "memory.allocated",
                       prefix + "memory.freed", phase);
  requireBalancedEvent(events, prefix + "created", prefix + "destroyed", phase);
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
  std::size_t material_count = 0, world_count = 0, prop_count = 0;
  {
    Platform platform;
    Window window(platform, 320, 240, "near-laugh lifecycle smoke");
    const auto level =
        loadPrototypeLevel("resources/levels/apartment-stairs.level.json");
    const auto prepared =
        prepareSceneAssets(smokeResources().resource_root, level);
    material_count = prepared.materials.size();
    world_count = prepared.world.size();
    prop_count = prepared.props.size();
    if (level.doors().empty() || prop_count < 4)
      throw std::runtime_error(
          "Combined lifecycle scene requires selected props and a door");
    Renderer renderer(window, window.framebufferExtent(), level,
                      smokeResources(), diagnostics);
    for (int frame = 0; frame < 8; ++frame) {
      const auto boxes = doorPresentationBoxes(level.doors().front(),
                                               static_cast<float>(frame) * 10,
                                               frame < 2, 0.5F, 0.3F);
      FrameRequest request{window.framebufferExtent(), false};
      request.opaque_boxes = boxes;
      request.point_light_enabled = {(frame & 1) != 0, (frame & 2) != 0};
      if (frame == 3) {
        renderer.requestSwapchainRecreation();
        if (renderer.renderFrame(request) != FrameOutcome::Recovered)
          throw std::runtime_error(
              "Forced swapchain recreation did not report recovery");
      }
      if (renderer.renderFrame(request) == FrameOutcome::Skipped)
        throw std::runtime_error(
            "Lifecycle smoke skipped a changing scene draw");
    }
    // A frame without boxes must not redraw data retained by this frame slot.
    static_cast<void>(
        renderer.renderFrame({window.framebufferExtent(), false}));
  }
  setLifecycleLog(nullptr);
  requireBalancedDepthLifecycle(events, "normal shutdown");
  requireBalancedTextureLifecycle(events, "normal shutdown");
  requireBalancedLightingLifecycle(events, "normal shutdown");
  for (const auto name : {"world", "prop", "changing"})
    requireBalancedMeshLifecycle(events, name, "normal shutdown");
  for (const auto event :
       {"texture.image.created", "texture.sampler.created",
        "texture.descriptor_layout.created", "texture.descriptor_pool.created",
        "texture.descriptor.updated", "texture.uploaded.shader_read_only",
        "material.buffer.created", "material.memory.allocated",
        "material.uploaded"}) {
    if (eventCount(events, event) != material_count)
      throw std::runtime_error(
          "Recovery rebuilt or incompletely initialized scene materials");
  }
  for (const auto event :
       {"lighting.buffer.created", "lighting.memory.allocated",
        "lighting.descriptor_layout.created",
        "lighting.descriptor_pool.created", "lighting.uploaded",
        "lighting.descriptor.updated"}) {
    if (eventCount(events, event) != 1)
      throw std::runtime_error(
          "Recovery rebuilt or rewrote immutable lighting");
  }
  for (const std::string name : {"world", "prop"}) {
    const auto expected = name == "world" ? world_count : prop_count;
    const auto prefix = name + ".mesh.";
    if (eventCount(events, prefix + "created") != expected ||
        eventCount(events, prefix + "uploaded") != expected ||
        eventCount(events, prefix + "drawn") < expected)
      throw std::runtime_error(
          "Changing presentation rebuilt or failed to draw static " + name);
  }
  if (eventCount(events, "changing.mesh.created") != 2 ||
      eventCount(events, "changing.mesh.updated") != 8 ||
      eventCount(events, "changing.mesh.drawn") != 8)
    throw std::runtime_error(
        "Changing presentation must reuse two fenced slots and omit empty "
        "frames");
  for (const auto event : {"texture.descriptor_pool.destroyed",
                           "lighting.descriptor_pool.destroyed",
                           "world.mesh.destroyed", "prop.mesh.destroyed"})
    requireBefore(events, "pipeline.destroyed", event, "normal shutdown");
  for (const auto event :
       {"world.mesh.destroyed", "prop.mesh.destroyed",
        "changing.mesh.destroyed", "lighting.destroyed", "texture.destroyed"})
    requireBefore(events, event, "device.destroyed", "normal shutdown");

  events.clear();
  setForcedVulkanStage("instance");
  setLifecycleLog(&events);
  bool renderer_failed = false;
  try {
    Platform platform;
    Window window(platform, 320, 240, "near-laugh failure smoke");
    PrototypeLevel level = loadPackagedPrototypeLevel();
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
    PrototypeLevel level = loadPackagedPrototypeLevel();
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

  constexpr std::array<const char*, 11> texture_failure_stages = {
      "texture_staging",         "texture_image",
      "texture_upload",          "texture_view",
      "texture_sampler",         "texture_descriptor_layout",
      "texture_descriptor_pool", "texture_descriptor_set",
      "material_buffer",         "material_memory",
      "material_upload"};
  for (const char* stage : texture_failure_stages) {
    events.clear();
    setForcedVulkanStage(stage);
    setLifecycleLog(&events);
    renderer_failed = false;
    try {
      Platform platform;
      Window window(platform, 320, 240, "near-laugh texture failure smoke");
      PrototypeLevel level = loadPackagedPrototypeLevel();
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
      PrototypeLevel level = loadPackagedPrototypeLevel();
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

  constexpr std::array<const char*, 10> mesh_failure_stages = {
      "world_mesh_buffer", "world_mesh_memory", "world_mesh_bind",
      "world_mesh_map",    "world_mesh_upload", "prop_mesh_buffer",
      "prop_mesh_memory",  "prop_mesh_bind",    "prop_mesh_map",
      "prop_mesh_upload"};
  for (const char* stage : mesh_failure_stages) {
    events.clear();
    setForcedVulkanStage(stage);
    setLifecycleLog(&events);
    renderer_failed = false;
    try {
      Platform platform;
      Window window(platform, 320, 240, "near-laugh mesh failure smoke");
      PrototypeLevel level = loadPackagedPrototypeLevel();
      Renderer renderer(window, window.framebufferExtent(), level,
                        smokeResources(), diagnostics);
    } catch (const std::runtime_error&) {
      renderer_failed = true;
    }
    setLifecycleLog(nullptr);
    setForcedVulkanStage("");
    if (!renderer_failed) {
      throw std::runtime_error(
          "Forced immutable mesh construction failure did not occur at " +
          std::string(stage));
    }
    requireBalancedMeshLifecycle(events, "world", stage);
    requireBalancedMeshLifecycle(events, "prop", stage);
    requireBalancedTextureLifecycle(events, stage);
    requireBalancedLightingLifecycle(events, stage);
    const std::string failed_name =
        std::string_view(stage).starts_with("world") ? "world" : "prop";
    if (eventCount(events, failed_name + ".mesh.created") != 0) {
      throw std::runtime_error(
          "Partially constructed mesh reported complete ownership at " +
          std::string(stage));
    }
  }

  for (const auto* stage : {"changing_mesh_buffer", "changing_mesh_upload"}) {
    events.clear();
    setLifecycleLog(&events);
    renderer_failed = false;
    try {
      Platform platform;
      Window window(platform, 320, 240, "near-laugh changing failure smoke");
      const auto level =
          loadPrototypeLevel("resources/levels/apartment-stairs.level.json");
      Renderer renderer(window, window.framebufferExtent(), level,
                        smokeResources(), diagnostics);
      const auto boxes =
          doorPresentationBoxes(level.doors().front(), 30, false);
      FrameRequest request{window.framebufferExtent(), false};
      request.opaque_boxes = boxes;
      setForcedVulkanStage(stage);
      static_cast<void>(renderer.renderFrame(request));
    } catch (const std::runtime_error&) {
      renderer_failed = true;
    }
    setLifecycleLog(nullptr);
    setForcedVulkanStage("");
    if (!renderer_failed)
      throw std::runtime_error("Changing opaque forced failure did not occur");
    requireBalancedMeshLifecycle(events, "changing", stage);
    requireBalancedMeshLifecycle(events, "world", stage);
    requireBalancedMeshLifecycle(events, "prop", stage);
    requireBalancedTextureLifecycle(events, stage);
    requireBalancedLightingLifecycle(events, stage);
  }

  events.clear();
  setLifecycleLog(&events);
  {
    Platform platform;
    Window window(platform, 320, 240, "near-laugh material mip smoke");
    VulkanContext context(window, diagnostics);
    const SceneMaterialData constant_white;
    SampledTexture white(context.device(), context.physicalDevice(),
                         context.graphicsQueue(),
                         context.queueFamilies().graphics, constant_white);
    const auto phone = loadStaticModel(
        sceneModelPath(smokeResources().resource_root, "apartment-phone"));
    SampledTexture cutout(context.device(), context.physicalDevice(),
                          context.graphicsQueue(),
                          context.queueFamilies().graphics, phone.material);
    if (white.mipLevelCount() != 1 || cutout.mipLevelCount() != 8 ||
        !white.allSubresourcesShaderReadOnly() ||
        !cutout.allSubresourcesShaderReadOnly())
      throw std::runtime_error(
          "White and cutout material mip chains were not fully uploaded");
  }
  setLifecycleLog(nullptr);
  requireBalancedTextureLifecycle(events, "white and cutout mip uploads");

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
    const bool interior =
        argc == 3 && std::string_view(argv[1]) == "--interior";
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
      PrototypeLevel level =
          interior ? loadPrototypeLevel(
                         "resources/levels/apartment-stairs.level.json")
                   : loadPackagedPrototypeLevel();
      const auto* entry =
          level.entry(interior ? argv[2] : level.defaultEntryId());
      if (!entry) throw std::runtime_error("Smoke selected an unknown entry");
      PhysicsWorld physics(level, *entry);
      PlayerController player(physics, entry->pose.yaw_degrees);
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
        const PlayerViewPose view = player.viewPose(1.0F);
        const CameraFrame camera = player.cameraFrame(
            extent.isZero() ? 1.0F
                            : static_cast<float>(extent.width) /
                                  static_cast<float>(extent.height),
            view);
        SpotLightFrame spot_light{};
        if (frame >= 30 && frame < 60) {
          spot_light = {
              {view.position.x, view.position.y, view.position.z, 16.0F},
              {view.direction.x, view.direction.y, view.direction.z, 0.97F},
              {0.92F, 0.96F, 1.0F, 1.35F},
              {0.90F, 1.0F, 0.0F, 0.0F}};
        } else if (frame >= 90) {
          spot_light = {{-4.0F, 2.0F, -3.0F, 8.0F},
                        {1.0F, 0.0F, 0.0F, 0.95F},
                        {1.0F, 0.35F, 0.15F, 1.0F},
                        {0.85F, 1.0F, 0.0F, 0.0F}};
        }
        if (!spotLightFrameIsValid(spot_light)) {
          throw std::runtime_error("Smoke generated an invalid spot light");
        }
        FrameRequest request{extent,
                             window.consumeFramebufferResize(),
                             camera,
                             spot_light,
                             {(frame & 1) == 0, (frame & 2) == 0}};
        std::vector<OpaqueBoxFrame> boxes;
        for (const auto& door : level.doors()) {
          const auto geometry = doorPresentationBoxes(
              door,
              door.open_angle_degrees * static_cast<float>(frame % 60) / 59,
              false, (frame % 12) / 12.0F, (frame % 10) / 10.0F);
          boxes.insert(boxes.end(), geometry.begin(), geometry.end());
        }
        request.opaque_boxes = boxes;
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
    if (interior) {
      near_laugh::RuntimeConfig config;
      config.resource_root = std::filesystem::absolute("resources");
      config.level_path =
          config.resource_root / "levels/apartment-stairs.level.json";
      config.entry_id = argv[2];
      config.window_width = 320;
      config.window_height = 240;
      {
        Engine engine(config, diagnostics);
        for (int i = 0; i < 3; ++i) static_cast<void>(engine.tick());
      }
      config.entry_id = "missing-interior-entry";
      bool rejected = false;
      try {
        Engine engine(config, diagnostics);
      } catch (const std::runtime_error& error) {
        rejected =
            std::string_view(error.what()).find("missing-interior-entry") !=
            std::string_view::npos;
      }
      if (!rejected)
        throw std::runtime_error(
            "Runtime did not reject the selected missing entry");
      config.entry_id = argv[2];
      config.level_path = config.resource_root / "levels/missing-interior.json";
      rejected = false;
      try {
        Engine engine(config, diagnostics);
      } catch (const std::runtime_error& error) {
        rejected =
            std::string_view(error.what()).find("missing-interior.json") !=
            std::string_view::npos;
      }
      if (!rejected)
        throw std::runtime_error(
            "Runtime did not reject the selected missing file");
      if (diagnostics.errorCount())
        throw std::runtime_error("Interior startup recorded validation errors");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "Vulkan smoke failed: " << error.what() << '\n';
    return 1;
  }
}
