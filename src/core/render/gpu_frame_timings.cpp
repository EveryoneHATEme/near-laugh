#include "core/render/gpu_frame_timings.hpp"

#include <array>
#include <iostream>
#include <vector>

#include "core/render/vulkan_utils.hpp"

GpuFrameTimings::GpuFrameTimings(VkDevice device, VkPhysicalDevice physical,
                                 std::uint32_t family, FrameTimings& timings)
    : device_(device), timings_(timings) {
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical, &properties);
  period_ = properties.limits.timestampPeriod;
  std::uint32_t count{};
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
  std::vector<VkQueueFamilyProperties> queues(count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, queues.data());
  valid_bits_ = queues.at(family).timestampValidBits;
  if (!timestampMilliseconds(0, 0, valid_bits_, period_, true, true)) {
    std::cout
        << "GPU timing unavailable: graphics queue timestamps unsupported\n";
    return;
  }
  VkQueryPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = 4;
  requireVulkan(vkCreateQueryPool(device_, &info, nullptr, &pool_),
                "Create frame timing timestamp queries");
  std::cout << "GPU timing: vendor " << properties.vendorID << ", device "
            << properties.deviceID << ", driver " << properties.driverVersion
            << ", timestampPeriod " << period_ << " ns, valid bits "
            << valid_bits_ << '\n';
}

GpuFrameTimings::~GpuFrameTimings() {
  if (pool_) vkDestroyQueryPool(device_, pool_, nullptr);
}
void GpuFrameTimings::begin(VkCommandBuffer command) {
  if (!pool_) return;
  vkCmdResetQueryPool(command, pool_, 0, 4);
  vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, pool_, 0);
}
void GpuFrameTimings::end(VkCommandBuffer command) {
  if (pool_)
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, pool_,
                         1);
}
void GpuFrameTimings::beginShadows(VkCommandBuffer command) {
  if (pool_)
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, pool_,
                         2);
}
void GpuFrameTimings::endShadows(VkCommandBuffer command) {
  if (pool_)
    vkCmdWriteTimestamp2(command, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, pool_,
                         3);
}
void GpuFrameTimings::submitted() {
  if (pool_) pending_ = timings_.currentIndex();
}
void GpuFrameTimings::readCompleted() noexcept {
  if (!pending_) return;
  struct Query {
    std::uint64_t ticks{}, available{};
  };
  std::array<Query, 4> results{};
  const auto status = vkGetQueryPoolResults(
      device_, pool_, 0, 4, sizeof(results), results.data(), sizeof(Query),
      VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  auto& row = timings_.sample(*pending_);
  if (status == VK_SUCCESS)
    row.gpu_frame_ms = timestampMilliseconds(
        results[0].ticks, results[1].ticks, valid_bits_, period_,
        results[0].available != 0, results[1].available != 0);
  if (!row.gpu_frame_ms)
    std::cerr << "GPU timing unavailable for frame " << *pending_
              << ": query result " << status << '\n';
  if (status == VK_SUCCESS)
    row.gpu_shadow_ms = timestampMilliseconds(
        results[2].ticks, results[3].ticks, valid_bits_, period_,
        results[2].available != 0, results[3].available != 0);
  pending_.reset();
}
