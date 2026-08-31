#include "core/render/vulkan_utils.hpp"

#include <algorithm>
#include <array>
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

void requireColorAttachmentSwapchainUsage(VkImageUsageFlags supported_usage) {
  if ((supported_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
    throw std::runtime_error(
        "Swapchain creation failed: surface does not support required "
        "color-attachment image usage");
  }
}

VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(
    VkCompositeAlphaFlagsKHR supported_modes) {
  constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> preference = {
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
      VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
  for (const VkCompositeAlphaFlagBitsKHR mode : preference) {
    if ((supported_modes & mode) != 0) {
      return mode;
    }
  }
  throw std::runtime_error(
      "Swapchain creation failed: surface reports no supported "
      "composite-alpha mode");
}

VkFormat chooseDepthFormat(
    const std::vector<FormatFeatureSupport>& candidates) {
  for (const FormatFeatureSupport candidate : candidates) {
    if ((candidate.optimal_tiling_features &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
      return candidate.format;
    }
  }
  throw std::runtime_error(
      "Depth attachment creation failed: no supported depth format in the "
      "renderer candidate list");
}

std::uint32_t fullMipLevelCount(std::uint32_t width, std::uint32_t height) {
  if (width == 0 || height == 0) {
    throw std::invalid_argument(
        "Texture mip calculation requires non-zero dimensions");
  }
  std::uint32_t largest_dimension = std::max(width, height);
  std::uint32_t level_count = 1;
  while (largest_dimension > 1) {
    largest_dimension /= 2;
    ++level_count;
  }
  return level_count;
}

void requirePrototypeTextureFormatFeatures(
    VkFormatFeatureFlags optimal_tiling_features) {
  struct RequiredFeature {
    VkFormatFeatureFlagBits bit;
    const char* description;
  };
  constexpr std::array<RequiredFeature, 6> required = {{
      {VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "sampling"},
      {VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
       "linear filtering"},
      {VK_FORMAT_FEATURE_BLIT_SRC_BIT, "blit source"},
      {VK_FORMAT_FEATURE_BLIT_DST_BIT, "blit destination"},
      {VK_FORMAT_FEATURE_TRANSFER_SRC_BIT, "transfer source"},
      {VK_FORMAT_FEATURE_TRANSFER_DST_BIT, "transfer destination"},
  }};
  for (const RequiredFeature feature : required) {
    if ((optimal_tiling_features & feature.bit) == 0) {
      throw std::runtime_error(
          "Prototype texture format VK_FORMAT_R8G8B8A8_SRGB lacks required "
          "optimal-tiling " +
          std::string(feature.description) + " support");
    }
  }
}

std::uint32_t chooseMemoryType(
    std::uint32_t type_bits,
    const VkPhysicalDeviceMemoryProperties& memory_properties,
    VkMemoryPropertyFlags required_properties, std::string_view resource) {
  for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount;
       ++index) {
    if ((type_bits & (1U << index)) != 0 &&
        (memory_properties.memoryTypes[index].propertyFlags &
         required_properties) == required_properties) {
      return index;
    }
  }
  throw std::runtime_error("No suitable Vulkan memory type for " +
                           std::string(resource));
}
