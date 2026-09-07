#ifndef CORE_RENDER_LIGHTING_RESOURCES_HPP
#define CORE_RENDER_LIGHTING_RESOURCES_HPP

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <type_traits>
#include <vector>

#include "core/render/point_shadow.hpp"
#include "core/world/prototype_level.hpp"

struct alignas(16) PrototypePointLightUpload {
  std::array<float, 4> position_and_radius{};
  std::array<float, 4> color_and_intensity{};
  // Enabled, first shadow layer (-1 for an unshadowed light), reserved.
  std::array<float, 4> controls{};
};
struct alignas(16) PrototypeLightingUpload {
  std::array<PrototypePointLightUpload, level_maximum_point_light_count>
      point_lights{};
  // Ambient, light count, depth quantization tolerance, reserved.
  std::array<float, 4> ambient_intensity{};
  std::array<CameraFrame, level_maximum_shadow_light_count * 6>
      shadow_cameras{};
};
static_assert(std::is_standard_layout_v<PrototypeLightingUpload>);
static_assert(sizeof(PrototypePointLightUpload) == 48);
static_assert(offsetof(PrototypeLightingUpload, ambient_intensity) == 384);
static_assert(offsetof(PrototypeLightingUpload, shadow_cameras) == 400);
static_assert(sizeof(PrototypeLightingUpload) == 1936);

[[nodiscard]] PrototypeLightingUpload makePrototypeLightingUpload(
    const PrototypeEnvironmentLight& environment);
void validatePointLightEnables(std::span<const std::uint8_t> enabled,
                               std::size_t count);
struct PointShadowFormatSupport {
  VkFormat format{};
  VkFormatFeatureFlags features{};
  VkImageFormatProperties image{};  // Zero limits when the image query failed.
};
[[nodiscard]] VkFormat choosePointShadowFormat(
    std::span<const PointShadowFormatSupport> support, std::uint32_t layers);
[[nodiscard]] constexpr float prototypePointLightFalloff(
    float distance, float radius) noexcept {
  const float normalized = std::clamp(distance / radius, 0.0F, 1.0F);
  const float edge = 1 - normalized * normalized;
  return edge * edge;
}
[[nodiscard]] constexpr VkDescriptorSetLayoutBinding
prototypeLightingDescriptorBinding() noexcept {
  return {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT,
          nullptr};
}

class GraphicsPipeline;
class SceneResources;
class ChangingMeshBuffer;
class ImmutableMeshBuffer;

// Shared game/editor implementation. All mutable uploads and depth storage
// belong to a frame slot; callers wait its completion fence before update.
class LightingResources {
 public:
  LightingResources(VkDevice device, VkPhysicalDevice physical,
                    const PrototypeEnvironmentLight& environment);
  ~LightingResources();
  LightingResources(const LightingResources&) = delete;
  LightingResources& operator=(const LightingResources&) = delete;
  void createShadowPipeline(const SceneResources& scene,
                            const std::filesystem::path& shader_directory);
  void validateEnables(std::span<const std::uint8_t> enabled) const;
  void update(std::size_t slot, std::span<const std::uint8_t> enabled);
  void recordShadows(VkCommandBuffer commands, std::size_t slot,
                     const SceneResources& scene,
                     const ChangingMeshBuffer* changing,
                     const ImmutableMeshBuffer* editor_doors);
  [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const noexcept {
    return layout_;
  }
  [[nodiscard]] VkDescriptorSet descriptorSet(std::size_t slot = 0) const {
    return slots_.at(slot).set;
  }
  [[nodiscard]] VkDeviceSize shadowMemoryBytes() const noexcept {
    return shadow_memory_bytes_;
  }

 private:
  struct Slot {
    VkBuffer buffer{};
    VkDeviceMemory uniform_memory{};
    void* mapped{};
    VkImage image{};
    VkDeviceMemory image_memory{};
    VkImageView array_view{};
    std::vector<VkImageView> faces;
    VkDescriptorSet set{};
    PrototypeLightingUpload upload{};
    bool initialized{};
  };
  void createSlot(Slot& slot);
  void createDescriptors();
  void cleanup() noexcept;
  VkDevice device_;
  VkPhysicalDevice physical_;
  VkFormat depth_format_{};
  VkDescriptorSetLayout layout_{};
  VkDescriptorPool pool_{};
  VkSampler sampler_{};
  std::uint32_t layer_count_{1};
  std::size_t light_count_{};
  VkDeviceSize shadow_memory_bytes_{};
  std::array<Slot, lighting_frame_slot_count> slots_{};
  std::unique_ptr<GraphicsPipeline> shadow_pipeline_;
  bool owner_recorded_{};
};
#endif
