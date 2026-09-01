#ifndef CORE_RENDER_DEPTH_ATTACHMENT_HPP
#define CORE_RENDER_DEPTH_ATTACHMENT_HPP

#include <vulkan/vulkan.h>

class DepthAttachment {
 public:
  DepthAttachment(VkDevice device, VkPhysicalDevice physical_device,
                  VkExtent2D extent, VkFormat format);
  ~DepthAttachment();

  DepthAttachment(const DepthAttachment&) = delete;
  DepthAttachment& operator=(const DepthAttachment&) = delete;
  DepthAttachment(DepthAttachment&&) = delete;
  DepthAttachment& operator=(DepthAttachment&&) = delete;

  [[nodiscard]] VkImage image() const noexcept { return image_; }
  [[nodiscard]] VkImageView view() const noexcept { return view_; }
  [[nodiscard]] bool initialized() const noexcept { return initialized_; }
  void markInitialized() noexcept { initialized_ = true; }

 private:
  void cleanup() noexcept;

  VkDevice device_{VK_NULL_HANDLE};
  VkImage image_{VK_NULL_HANDLE};
  VkDeviceMemory memory_{VK_NULL_HANDLE};
  VkImageView view_{VK_NULL_HANDLE};
  bool initialized_{};
  bool lifecycle_recorded_{};
};

#endif
