#ifndef CORE_RENDER_SAMPLED_TEXTURE_HPP
#define CORE_RENDER_SAMPLED_TEXTURE_HPP

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>

#include "core/render/scene_material.hpp"

class SampledTexture {
 public:
  SampledTexture(VkDevice device, VkPhysicalDevice physical_device,
                 VkQueue graphics_queue, std::uint32_t graphics_queue_family,
                 const SceneMaterialData& material);
  ~SampledTexture();

  SampledTexture(const SampledTexture&) = delete;
  SampledTexture& operator=(const SampledTexture&) = delete;
  SampledTexture(SampledTexture&&) = delete;
  SampledTexture& operator=(SampledTexture&&) = delete;

  [[nodiscard]] VkDescriptorSetLayout descriptorSetLayout() const noexcept {
    return descriptor_set_layout_;
  }
  [[nodiscard]] VkDescriptorSet descriptorSet() const noexcept {
    return descriptor_set_;
  }
  [[nodiscard]] std::uint32_t mipLevelCount() const noexcept {
    return mip_level_count_;
  }
  [[nodiscard]] bool allSubresourcesShaderReadOnly() const noexcept {
    return all_subresources_shader_read_only_;
  }

 private:
  void createImageAndUpload(VkQueue graphics_queue,
                            std::uint32_t graphics_queue_family,
                            const DecodedRgbaImage& image);
  void createImageView();
  void createSampler();
  void createDescriptor(const SceneMaterialData& material);
  void cleanup() noexcept;

  VkDevice device_{VK_NULL_HANDLE};
  VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
  VkImage image_{VK_NULL_HANDLE};
  VkDeviceMemory image_memory_{VK_NULL_HANDLE};
  VkImageView image_view_{VK_NULL_HANDLE};
  VkSampler sampler_{VK_NULL_HANDLE};
  VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
  VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
  VkDescriptorSet descriptor_set_{VK_NULL_HANDLE};
  VkBuffer material_buffer_{VK_NULL_HANDLE};
  VkDeviceMemory material_memory_{VK_NULL_HANDLE};
  std::uint32_t width_{};
  std::uint32_t height_{};
  bool nearest_{};
  std::uint32_t mip_level_count_{};
  bool all_subresources_shader_read_only_{};
  bool image_recorded_{};
  bool view_recorded_{};
  bool sampler_recorded_{};
  bool descriptor_layout_recorded_{};
  bool descriptor_pool_recorded_{};
  bool owner_recorded_{};
};

#endif
