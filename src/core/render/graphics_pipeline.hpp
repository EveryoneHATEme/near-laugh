#ifndef CORE_RENDER_GRAPHICS_PIPELINE_H
#define CORE_RENDER_GRAPHICS_PIPELINE_H

#include <vulkan/vulkan.h>

#include <algorithm>
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
static_assert(offsetof(PositionColorVertex, texture_coordinates) ==
              sizeof(float) * 6 + 4);
static_assert(offsetof(PositionColorVertex, texture_layer) ==
              sizeof(float) * 8 + 4);

[[nodiscard]] constexpr VkVertexInputBindingDescription
sceneVertexBindingDescription() noexcept {
  return {0, sizeof(PositionColorVertex), VK_VERTEX_INPUT_RATE_VERTEX};
}
static_assert(sceneVertexBindingDescription().stride ==
              sizeof(PositionColorVertex));

[[nodiscard]] constexpr std::array<VkVertexInputAttributeDescription, 5>
sceneVertexAttributeDescriptions() noexcept {
  return {
      {{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<std::uint32_t>(offsetof(PositionColorVertex, position))},
       {1, 0, VK_FORMAT_R8G8B8A8_UNORM,
        static_cast<std::uint32_t>(offsetof(PositionColorVertex, color))},
       {2, 0, VK_FORMAT_R32G32B32_SFLOAT,
        static_cast<std::uint32_t>(offsetof(PositionColorVertex, normal))},
       {3, 0, VK_FORMAT_R32G32_SFLOAT,
        static_cast<std::uint32_t>(
            offsetof(PositionColorVertex, texture_coordinates))},
       {4, 0, VK_FORMAT_R32_UINT,
        static_cast<std::uint32_t>(
            offsetof(PositionColorVertex, texture_layer))}}};
}

struct alignas(16) ScenePushConstant {
  CameraFrame camera{};
  std::array<float, 4> spot_position_and_range{};
  std::array<float, 4> spot_direction_and_inner_cosine{};
  std::array<float, 4> spot_color_and_intensity{};
  // outer cosine, spotlight enabled, point light 0 enabled, point light 1
  // enabled
  std::array<float, 4> light_controls{};
};

inline constexpr std::size_t vulkan_minimum_push_constant_size = 128;
static_assert(std::is_standard_layout_v<ScenePushConstant>);
static_assert(offsetof(ScenePushConstant, camera) == 0);
static_assert(offsetof(ScenePushConstant, spot_position_and_range) == 64);
static_assert(offsetof(ScenePushConstant, spot_direction_and_inner_cosine) ==
              80);
static_assert(offsetof(ScenePushConstant, spot_color_and_intensity) == 96);
static_assert(offsetof(ScenePushConstant, light_controls) == 112);
static_assert(sizeof(ScenePushConstant) == 128);
static_assert(sizeof(ScenePushConstant) <= vulkan_minimum_push_constant_size);

[[nodiscard]] constexpr ScenePushConstant makeScenePushConstant(
    const CameraFrame& camera, SpotLightFrame spot_light = {},
    std::array<bool, 2> point_light_enabled = {true, true}) noexcept {
  return {camera,
          spot_light.position_and_range,
          spot_light.direction_and_inner_cosine,
          spot_light.color_and_intensity,
          {spot_light.outer_cosine_and_enabled[0],
           spot_light.outer_cosine_and_enabled[1],
           point_light_enabled[0] ? 1.0F : 0.0F,
           point_light_enabled[1] ? 1.0F : 0.0F}};
}

[[nodiscard]] constexpr float spotLightDistanceFalloff(float distance,
                                                       float range) noexcept {
  const float normalized = std::clamp(distance / range, 0.0F, 1.0F);
  const float edge = 1.0F - normalized * normalized;
  return edge * edge;
}

[[nodiscard]] constexpr float spotLightAngularFalloff(
    float direction_cosine, float inner_cosine, float outer_cosine) noexcept {
  const float transition = std::clamp(
      (direction_cosine - outer_cosine) / (inner_cosine - outer_cosine), 0.0F,
      1.0F);
  return transition * transition * (3.0F - 2.0F * transition);
}

[[nodiscard]] constexpr float spotLightDiffuseFactor(
    float normal_dot_to_light, float distance, float range,
    float direction_cosine, float inner_cosine, float outer_cosine,
    bool enabled) noexcept {
  if (!enabled || normal_dot_to_light <= 0.0F || distance >= range ||
      direction_cosine <= outer_cosine) {
    return 0.0F;
  }
  return std::clamp(normal_dot_to_light, 0.0F, 1.0F) *
         spotLightDistanceFalloff(distance, range) *
         spotLightAngularFalloff(direction_cosine, inner_cosine, outer_cosine);
}

inline constexpr std::uint32_t scene_texture_descriptor_set = 0;
inline constexpr std::uint32_t scene_lighting_descriptor_set = 1;
inline constexpr std::uint32_t scene_descriptor_set_count = 2;

[[nodiscard]] inline std::array<VkDescriptorSetLayout,
                                scene_descriptor_set_count>
sceneDescriptorSetLayouts(VkDescriptorSetLayout texture,
                          VkDescriptorSetLayout lighting) noexcept {
  std::array<VkDescriptorSetLayout, scene_descriptor_set_count> layouts{};
  layouts[scene_texture_descriptor_set] = texture;
  layouts[scene_lighting_descriptor_set] = lighting;
  return layouts;
}

[[nodiscard]] inline std::array<VkDescriptorSet, scene_descriptor_set_count>
sceneDescriptorSets(VkDescriptorSet texture,
                    VkDescriptorSet lighting) noexcept {
  std::array<VkDescriptorSet, scene_descriptor_set_count> sets{};
  sets[scene_texture_descriptor_set] = texture;
  sets[scene_lighting_descriptor_set] = lighting;
  return sets;
}

[[nodiscard]] constexpr VkPushConstantRange scenePushConstantRange() noexcept {
  return {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
          sizeof(ScenePushConstant)};
}

class GraphicsPipeline {
 public:
  GraphicsPipeline(VkDevice device, VkFormat swapchain_format,
                   VkFormat depth_format,
                   VkDescriptorSetLayout texture_descriptor_layout,
                   VkDescriptorSet texture_descriptor_set,
                   VkDescriptorSetLayout lighting_descriptor_layout,
                   VkDescriptorSet lighting_descriptor_set,
                   const std::filesystem::path& vertex_shader_path,
                   const std::filesystem::path& fragment_shader_path);
  ~GraphicsPipeline();

  GraphicsPipeline(const GraphicsPipeline&) = delete;
  GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
  GraphicsPipeline(GraphicsPipeline&&) = delete;
  GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

  void bindSceneState(VkCommandBuffer command_buffer, const CameraFrame& camera,
                      SpotLightFrame spot_light,
                      std::array<bool, 2> point_light_enabled) const;
  void bindMaterial(VkCommandBuffer command_buffer,
                    VkDescriptorSet material) const;

 private:
  [[nodiscard]] VkShaderModule createShaderModule(
      const std::filesystem::path& path) const;
  void createPipeline(VkFormat swapchain_format, VkFormat depth_format,
                      const std::filesystem::path& vertex_shader_path,
                      const std::filesystem::path& fragment_shader_path);
  void cleanup() noexcept;

  VkDevice device_{VK_NULL_HANDLE};
  VkDescriptorSetLayout texture_descriptor_layout_{VK_NULL_HANDLE};
  VkDescriptorSet texture_descriptor_set_{VK_NULL_HANDLE};
  VkDescriptorSetLayout lighting_descriptor_layout_{VK_NULL_HANDLE};
  VkDescriptorSet lighting_descriptor_set_{VK_NULL_HANDLE};
  VkPipelineLayout layout_{VK_NULL_HANDLE};
  VkPipeline pipeline_{VK_NULL_HANDLE};
  bool lifecycle_recorded_{};
};

#endif
