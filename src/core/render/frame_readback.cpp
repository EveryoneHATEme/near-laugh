#include "core/render/frame_readback.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include "core/render/vulkan_utils.hpp"

FrameReadback::FrameReadback(VkDevice device, VkPhysicalDevice physical,
                             VkExtent2D extent, VkFormat format)
    : device_(device),
      extent_(extent),
      format_(format),
      bytes_(frameCaptureByteCount(extent.width, extent.height)) {
  if (format != VK_FORMAT_B8G8R8A8_SRGB && format != VK_FORMAT_R8G8B8A8_SRGB &&
      format != VK_FORMAT_B8G8R8A8_UNORM && format != VK_FORMAT_R8G8B8A8_UNORM)
    throw std::runtime_error(
        "Frame readback requires an RGBA8 or BGRA8 presentation format");
  try {
    VkBufferCreateInfo buffer{};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = bytes_;
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    requireVulkan(vkCreateBuffer(device_, &buffer, nullptr, &buffer_),
                  "Create frame readback buffer");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer_, &requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical, &properties);
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex =
        chooseMemoryType(requirements.memoryTypeBits, properties,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         "frame readback");
    requireVulkan(vkAllocateMemory(device_, &allocation, nullptr, &memory_),
                  "Allocate frame readback memory");
    requireVulkan(vkBindBufferMemory(device_, buffer_, memory_, 0),
                  "Bind frame readback memory");
    requireVulkan(vkMapMemory(device_, memory_, 0, bytes_, 0, &mapped_),
                  "Map frame readback memory");
  } catch (...) {
    cleanup();
    throw;
  }
}
FrameReadback::~FrameReadback() { cleanup(); }
void FrameReadback::cleanup() noexcept {
  if (mapped_) {
    vkUnmapMemory(device_, memory_);
    mapped_ = nullptr;
  }
  if (buffer_) {
    vkDestroyBuffer(device_, buffer_, nullptr);
    buffer_ = {};
  }
  if (memory_) {
    vkFreeMemory(device_, memory_, nullptr);
    memory_ = {};
  }
}
bool FrameReadback::matches(VkExtent2D extent, VkFormat format) const noexcept {
  return extent.width == extent_.width && extent.height == extent_.height &&
         format == format_;
}
void FrameReadback::record(VkCommandBuffer commands, VkImage image) const {
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  VkDependencyInfo dependency{};
  dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(commands, &dependency);
  VkBufferImageCopy copy{};
  copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
  copy.imageExtent = {extent_.width, extent_.height, 1};
  vkCmdCopyImageToBuffer(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                         buffer_, 1, &copy);
  VkBufferMemoryBarrier2 host{};
  host.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
  host.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
  host.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
  host.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
  host.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
  host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  host.buffer = buffer_;
  host.size = bytes_;
  dependency.imageMemoryBarrierCount = 0;
  dependency.bufferMemoryBarrierCount = 1;
  dependency.pBufferMemoryBarriers = &host;
  vkCmdPipelineBarrier2(commands, &dependency);
}
void FrameReadback::complete(FrameCapture& output) const {
  output.rgba.resize(static_cast<std::size_t>(bytes_));
  std::memcpy(output.rgba.data(), mapped_, static_cast<std::size_t>(bytes_));
  if (format_ == VK_FORMAT_B8G8R8A8_SRGB || format_ == VK_FORMAT_B8G8R8A8_UNORM)
    for (std::size_t i = 0; i < output.rgba.size(); i += 4)
      std::swap(output.rgba[i], output.rgba[i + 2]);
  output.width = extent_.width;
  output.height = extent_.height;
  output.requested = false;
}
