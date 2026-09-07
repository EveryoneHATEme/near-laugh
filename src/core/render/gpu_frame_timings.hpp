#ifndef CORE_RENDER_GPU_FRAME_TIMINGS_HPP
#define CORE_RENDER_GPU_FRAME_TIMINGS_HPP

#include <vulkan/vulkan.h>

#include <optional>

#include "core/development/frame_timings.hpp"

// One owner per fenced frame slot. Read back only after that slot's fence (or
// device idle at teardown), before recording the next reset of its queries.
class GpuFrameTimings {
 public:
  GpuFrameTimings(VkDevice device, VkPhysicalDevice physical_device,
                  std::uint32_t queue_family, FrameTimings& timings);
  ~GpuFrameTimings();
  GpuFrameTimings(const GpuFrameTimings&) = delete;
  GpuFrameTimings& operator=(const GpuFrameTimings&) = delete;
  void begin(VkCommandBuffer command);
  void beginShadows(VkCommandBuffer command);
  void endShadows(VkCommandBuffer command);
  void end(VkCommandBuffer command);
  void submitted();
  void readCompleted() noexcept;

 private:
  VkDevice device_;
  VkQueryPool pool_{VK_NULL_HANDLE};
  FrameTimings& timings_;
  unsigned valid_bits_{};
  double period_{};
  std::optional<std::size_t> pending_;
};
#endif
