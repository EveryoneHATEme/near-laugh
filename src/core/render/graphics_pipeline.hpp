#ifndef CORE_RENDER_GRAPHICS_PIPELINE_H
#define CORE_RENDER_GRAPHICS_PIPELINE_H

#include <vulkan/vulkan.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <type_traits>

#include "core/frame.hpp"
#include "core/render/prototype_scene.hpp"

static_assert(offsetof(PositionColorVertex, position) == 0);
static_assert(offsetof(PositionColorVertex, color) == sizeof(float) * 3);
static_assert(offsetof(PositionColorVertex, normal) == sizeof(float) * 3 + 4);

[[nodiscard]] constexpr VkVertexInputBindingDescription
sceneVertexBindingDescription() noexcept {
  return {0, sizeof(PositionColorVertex), VK_VERTEX_INPUT_RATE_VERTEX};
}
static_assert(sceneVertexBindingDescription().stride ==
              sizeof(PositionColorVertex));

[[nodiscard]] constexpr std::array<VkVertexInputAttributeDescription, 3>
sceneVertexAttributeDescriptions() noexcept {
  return {
      {{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<std::uint32_t>(offsetof(PositionColorVertex, position))},
       {1, 0, VK_FORMAT_R8G8B8A8_UNORM,
        static_cast<std::uint32_t>(offsetof(PositionColorVertex, color))},
       {2, 0, VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<std::uint32_t>(offsetof(PositionColorVertex, normal))}}};
}

struct alignas(16) ScenePushConstant {
  CameraFrame camera{};
  alignas(16) std::array<float, 4> direction_and_directional_intensity{};
  alignas(16) std::array<float, 4> ambient_intensity{};
};

inline constexpr std::size_t vulkan_minimum_push_constant_size = 128;
static_assert(std::is_standard_layout_v<ScenePushConstant>);
static_assert(offsetof(ScenePushConstant, camera) == 0);
static_assert(offsetof(ScenePushConstant,
                       direction_and_directional_intensity) ==
              sizeof(CameraFrame));
static_assert(offsetof(ScenePushConstant, ambient_intensity) ==
              sizeof(CameraFrame) + sizeof(float) * 4);
static_assert(sizeof(ScenePushConstant) == sizeof(float) * 24);
static_assert(sizeof(ScenePushConstant) <= vulkan_minimum_push_constant_size);

[[nodiscard]] constexpr ScenePushConstant makeScenePushConstant(
    const CameraFrame& camera,
    const PrototypeEnvironmentLight& environment_light) noexcept {
  ScenePushConstant push_constant{};
  push_constant.camera = camera;
  push_constant.direction_and_directional_intensity = {
      environment_light.direction_to_light[0],
      environment_light.direction_to_light[1],
      environment_light.direction_to_light[2],
      environment_light.directional_intensity};
  push_constant.ambient_intensity[0] = environment_light.ambient_intensity;
  return push_constant;
}

[[nodiscard]] constexpr VkPushConstantRange scenePushConstantRange() noexcept {
  return {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
          sizeof(ScenePushConstant)};
}

class GraphicsPipeline {
 public:
  GraphicsPipeline(VkDevice device, VkPhysicalDevice physical_device,
                   VkFormat swapchain_format, VkFormat depth_format,
                   const std::filesystem::path& vertex_shader_path,
                   const std::filesystem::path& fragment_shader_path,
                   const PrototypeLevel& level);
  ~GraphicsPipeline();

  GraphicsPipeline(const GraphicsPipeline&) = delete;
  GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
  GraphicsPipeline(GraphicsPipeline&&) = delete;
  GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

  void bindAndDraw(VkCommandBuffer command_buffer, const CameraFrame& camera,
                   const PrototypeEnvironmentLight& environment_light) const;

 private:
  [[nodiscard]] VkShaderModule createShaderModule(
      const std::filesystem::path& path) const;
  void createPipeline(VkFormat swapchain_format, VkFormat depth_format,
                      const std::filesystem::path& vertex_shader_path,
                      const std::filesystem::path& fragment_shader_path);
  void createVertexBuffer(const PrototypeLevel& level);
  void cleanup() noexcept;

  VkDevice device_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkPipelineLayout layout_{VK_NULL_HANDLE};
  VkPipeline pipeline_{VK_NULL_HANDLE};
  VkBuffer vertex_buffer_{VK_NULL_HANDLE};
  VkDeviceMemory vertex_memory_{VK_NULL_HANDLE};
  std::uint32_t vertex_count_{};
};

#endif
