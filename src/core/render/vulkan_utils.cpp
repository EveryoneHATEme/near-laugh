#include "core/render/vulkan_utils.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

const char* vulkanResultName(VkResult result) noexcept {
  switch (result) {
    case VK_SUCCESS:
      return "VK_SUCCESS";
    case VK_NOT_READY:
      return "VK_NOT_READY";
    case VK_TIMEOUT:
      return "VK_TIMEOUT";
    case VK_EVENT_SET:
      return "VK_EVENT_SET";
    case VK_EVENT_RESET:
      return "VK_EVENT_RESET";
    case VK_INCOMPLETE:
      return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
      return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
      return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED:
      return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST:
      return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED:
      return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT:
      return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
      return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
      return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
      return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS:
      return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
      return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL:
      return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_UNKNOWN:
      return "VK_ERROR_UNKNOWN";
    case VK_ERROR_OUT_OF_DATE_KHR:
      return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_SUBOPTIMAL_KHR:
      return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_SURFACE_LOST_KHR:
      return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
      return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT:
      return "VK_ERROR_VALIDATION_FAILED_EXT";
    default:
      return "VK_RESULT_UNKNOWN";
  }
}

void requireVulkan(VkResult result, std::string_view operation) {
  if (result != VK_SUCCESS) {
    throw std::runtime_error(std::string(operation) +
                             " failed: " + vulkanResultName(result) + " (" +
                             std::to_string(result) + ")");
  }
}

std::optional<QueueFamilySelection> chooseQueueFamilies(
    const std::vector<QueueFamilyCandidate>& families) noexcept {
  for (std::uint32_t index = 0; index < families.size(); ++index) {
    if (families[index].supports_graphics && families[index].supports_present) {
      return QueueFamilySelection{index, index};
    }
  }

  std::optional<std::uint32_t> graphics;
  std::optional<std::uint32_t> present;
  for (std::uint32_t index = 0; index < families.size(); ++index) {
    if (!graphics && families[index].supports_graphics) {
      graphics = index;
    }
    if (!present && families[index].supports_present) {
      present = index;
    }
  }
  if (!graphics || !present) {
    return std::nullopt;
  }
  return QueueFamilySelection{*graphics, *present};
}

VkSurfaceFormatKHR chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
  if (formats.empty()) {
    throw std::runtime_error("Surface reports no supported formats");
  }
  for (const VkSurfaceFormatKHR format : formats) {
    if ((format.format == VK_FORMAT_B8G8R8A8_SRGB ||
         format.format == VK_FORMAT_R8G8B8A8_SRGB) &&
        format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return format;
    }
  }
  return formats.front();
}

VkExtent2D chooseSwapchainExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                 VkExtent2D framebuffer) {
  if (capabilities.currentExtent.width !=
      std::numeric_limits<std::uint32_t>::max()) {
    return capabilities.currentExtent;
  }
  return {std::clamp(framebuffer.width, capabilities.minImageExtent.width,
                     capabilities.maxImageExtent.width),
          std::clamp(framebuffer.height, capabilities.minImageExtent.height,
                     capabilities.maxImageExtent.height)};
}
