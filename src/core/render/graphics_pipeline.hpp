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

[[nodiscard]] constexpr VkVertexInputBindingDescription
sceneVertexBindingDescription() noexcept {
  return {0, sizeof(PositionColorVertex), VK_VERTEX_INPUT_RATE_VERTEX};
}

[[nodiscard]] constexpr std::array<VkVertexInputAttributeDescription, 2>
sceneVertexAttributeDescriptions() noexcept {
  return {
      {{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<std::uint32_t>(offsetof(PositionColorVertex, position))},
       {1, 0, VK_FORMAT_R8G8B8A8_UNORM,
        static_cast<std::uint32_t>(offsetof(PositionColorVertex, color))}}};
}

[[nodiscard]] constexpr VkPushConstantRange
sceneCameraPushConstantRange() noexcept {
  return {VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(CameraFrame)};
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

  void bindAndDraw(VkCommandBuffer command_buffer,
                   const CameraFrame& camera) const;

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
