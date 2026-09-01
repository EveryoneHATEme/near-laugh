#include "core/render/depth_attachment.hpp"

#include <stdexcept>

#include "core/render/vulkan_utils.hpp"
#include "core/testing/test_controls.hpp"

DepthAttachment::DepthAttachment(VkDevice device,
                                 VkPhysicalDevice physical_device,
                                 VkExtent2D extent, VkFormat format)
    : device_(device) {
  if (device_ == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE ||
      extent.width == 0 || extent.height == 0 ||
      format == VK_FORMAT_UNDEFINED) {
    throw std::invalid_argument(
        "Depth attachment requires valid Vulkan handles, extent, and format");
  }

  try {
    VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = format;
    image_info.extent = {extent.width, extent.height, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    requireVulkan(vkCreateImage(device_, &image_info, nullptr, &image_),
                  "Create per-swapchain-image depth image");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, image_, &requirements);
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = chooseMemoryType(
        requirements.memoryTypeBits, memory_properties,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "depth attachment");
    requireVulkan(vkAllocateMemory(device_, &allocation, nullptr, &memory_),
                  "Allocate device-local depth image memory");
    requireVulkan(vkBindImageMemory(device_, image_, memory_, 0),
                  "Bind device-local depth image memory");

    VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view_info.image = image_;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.layerCount = 1;
    requireVulkan(vkCreateImageView(device_, &view_info, nullptr, &view_),
                  "Create per-swapchain-image depth view");
    recordLifecycleEvent("depth.created");
    lifecycle_recorded_ = true;
    if (forcedVulkanFailureAt("depth")) {
      throw std::runtime_error(
          "Forced failure after Vulkan depth attachment creation");
    }
  } catch (...) {
    cleanup();
    throw;
  }
}

DepthAttachment::~DepthAttachment() { cleanup(); }

void DepthAttachment::cleanup() noexcept {
  if (view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, view_, nullptr);
    view_ = VK_NULL_HANDLE;
  }
  if (image_ != VK_NULL_HANDLE) {
    vkDestroyImage(device_, image_, nullptr);
    image_ = VK_NULL_HANDLE;
  }
  if (memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
  }
  if (lifecycle_recorded_) {
    recordLifecycleEvent("depth.destroyed");
    lifecycle_recorded_ = false;
  }
}
