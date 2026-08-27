#ifndef CORE_RENDER_VULKAN_UTILS_HPP
#define CORE_RENDER_VULKAN_UTILS_HPP

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

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

#endif
