#ifndef CORE_RENDER_FRAME_READBACK_HPP
#define CORE_RENDER_FRAME_READBACK_HPP
#include <vulkan/vulkan.h>

#include "core/development/frame_capture.hpp"

class FrameReadback {
 public:
  FrameReadback(VkDevice device, VkPhysicalDevice physical, VkExtent2D extent,
                VkFormat format);
  ~FrameReadback();
  FrameReadback(const FrameReadback&) = delete;
  FrameReadback& operator=(const FrameReadback&) = delete;
  [[nodiscard]] bool matches(VkExtent2D extent, VkFormat format) const noexcept;
  void record(VkCommandBuffer commands, VkImage image) const;
  // Caller waits the submission's completion fence before host access/reuse.
  void complete(FrameCapture& output) const;

 private:
  void cleanup() noexcept;
  VkDevice device_;
  VkExtent2D extent_;
  VkFormat format_;
  VkDeviceSize bytes_;
  VkBuffer buffer_{};
  VkDeviceMemory memory_{};
  void* mapped_{};
};
#endif
