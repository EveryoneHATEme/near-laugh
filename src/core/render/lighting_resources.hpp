#ifndef CORE_RENDER_LIGHTING_RESOURCES_HPP
#define CORE_RENDER_LIGHTING_RESOURCES_HPP

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>

#include "core/world/prototype_level.hpp"

struct alignas(16) PrototypePointLightUpload {
  std::array<float, 4> position_and_radius{};
  std::array<float, 4> color_and_intensity{};
};

struct alignas(16) PrototypeLightingUpload {
  std::array<PrototypePointLightUpload, prototype_point_light_count>
      point_lights{};
  std::array<float, 4> ambient_intensity{};
};

static_assert(std::is_standard_layout_v<PrototypePointLightUpload>);
static_assert(alignof(PrototypePointLightUpload) == 16);
static_assert(sizeof(PrototypePointLightUpload) == 32);
static_assert(offsetof(PrototypePointLightUpload, position_and_radius) == 0);
static_assert(offsetof(PrototypePointLightUpload, color_and_intensity) == 16);
static_assert(std::is_standard_layout_v<PrototypeLightingUpload>);
static_assert(alignof(PrototypeLightingUpload) == 16);
static_assert(offsetof(PrototypeLightingUpload, point_lights) == 0);
static_assert(offsetof(PrototypeLightingUpload, ambient_intensity) == 64);
static_assert(sizeof(PrototypeLightingUpload) == 80);

[[nodiscard]] constexpr PrototypeLightingUpload makePrototypeLightingUpload(
    const PrototypeEnvironmentLight& environment_light) noexcept {
  PrototypeLightingUpload upload{};
  for (std::size_t index = 0; index < prototype_point_light_count; ++index) {
    const PrototypePointLight& light = environment_light.point_lights[index];
    upload.point_lights[index].position_and_radius = {
        light.position.x, light.position.y, light.position.z, light.radius};
    upload.point_lights[index].color_and_intensity = {
        light.color[0], light.color[1], light.color[2], light.intensity};
  }
  upload.ambient_intensity[0] = environment_light.ambient_intensity;
  return upload;
}

[[nodiscard]] constexpr float prototypePointLightFalloff(
    float distance, float radius) noexcept {
  const float normalized_distance = std::clamp(distance / radius, 0.0F, 1.0F);
  const float edge = 1.0F - normalized_distance * normalized_distance;
  return edge * edge;
}

[[nodiscard]] constexpr VkDescriptorSetLayoutBinding
prototypeLightingDescriptorBinding() noexcept {
  return {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
          nullptr};
}

class LightingResources {
 public:
  LightingResources(VkDevice device, VkPhysicalDevice physical_device,
                    const PrototypeEnvironmentLight& environment_light);
  ~LightingResources();

  LightingResources(const LightingResources&) = delete;
  LightingResources& operator=(const LightingResources&) = delete;
  LightingResources(LightingResources&&) = delete;
  LightingResources& operator=(LightingResources&&) = delete;

  [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const noexcept {
    return descriptor_set_layout_;
  }
  [[nodiscard]] VkDescriptorSet descriptorSet() const noexcept {
    return descriptor_set_;
  }

 private:
  void createBufferAndUpload(
      const PrototypeEnvironmentLight& environment_light);
  void createDescriptor();
  void cleanup() noexcept;

  VkDevice device_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkBuffer buffer_{VK_NULL_HANDLE};
  VkDeviceMemory memory_{VK_NULL_HANDLE};
  VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
  VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
  bool buffer_recorded_{};
  bool memory_recorded_{};
  bool descriptor_layout_recorded_{};
  bool descriptor_pool_recorded_{};
  bool owner_recorded_{};
};

#endif
