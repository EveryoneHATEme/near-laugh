#ifndef CORE_RENDER_VULKAN_UTILS_HPP
#define CORE_RENDER_VULKAN_UTILS_HPP

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

struct FormatFeatureSupport {
  VkFormat format{VK_FORMAT_UNDEFINED};
  VkFormatFeatureFlags optimal_tiling_features{};
};

struct QueueFamilyCandidate {
  bool supports_graphics{};
  bool supports_present{};
};

struct QueueFamilySelection {
  std::uint32_t graphics{};
  std::uint32_t present{};

  [[nodiscard]] bool sharesFamily() const noexcept {
    return graphics == present;
  }
};

[[nodiscard]] const char* vulkanResultName(VkResult result) noexcept;
void requireVulkan(VkResult result, std::string_view operation);

[[nodiscard]] std::optional<QueueFamilySelection> chooseQueueFamilies(
    const std::vector<QueueFamilyCandidate>& families) noexcept;
[[nodiscard]] VkSurfaceFormatKHR chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats);
[[nodiscard]] VkExtent2D chooseSwapchainExtent(
    const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D framebuffer);
void requireColorAttachmentSwapchainUsage(VkImageUsageFlags supported_usage);
[[nodiscard]] VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(
    VkCompositeAlphaFlagsKHR supported_modes);
[[nodiscard]] VkFormat chooseDepthFormat(
    const std::vector<FormatFeatureSupport>& candidates);
[[nodiscard]] std::uint32_t fullMipLevelCount(std::uint32_t width,
                                              std::uint32_t height);
[[nodiscard]] constexpr VkFormatFeatureFlags
requiredPrototypeTextureFormatFeatures() noexcept {
  return VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
         VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT |
         VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
         VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
}
void requirePrototypeTextureFormatFeatures(
    VkFormatFeatureFlags optimal_tiling_features);
[[nodiscard]] std::uint32_t chooseMemoryType(
    std::uint32_t type_bits,
    const VkPhysicalDeviceMemoryProperties& memory_properties,
    VkMemoryPropertyFlags required_properties, std::string_view resource);

#endif
