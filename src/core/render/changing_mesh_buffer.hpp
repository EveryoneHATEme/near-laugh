#ifndef CORE_RENDER_CHANGING_MESH_BUFFER_HPP
#define CORE_RENDER_CHANGING_MESH_BUFFER_HPP

#include <vulkan/vulkan.h>

#include <span>

#include "core/render/prototype_scene.hpp"

// One owner per frame slot. The caller waits that slot's fence before update.
class ChangingMeshBuffer {
 public:
  ChangingMeshBuffer(VkDevice device, VkPhysicalDevice physical_device);
  ~ChangingMeshBuffer();
  ChangingMeshBuffer(const ChangingMeshBuffer&) = delete;
  ChangingMeshBuffer& operator=(const ChangingMeshBuffer&) = delete;
  void update(std::span<const PositionColorVertex> vertices);
  void draw(VkCommandBuffer commands) const;

 private:
  void cleanup() noexcept;
  VkDevice device_;
  VkBuffer buffer_{VK_NULL_HANDLE};
  VkDeviceMemory memory_{VK_NULL_HANDLE};
  void* mapped_{};
  std::uint32_t count_{};
  bool owner_recorded_{};
};

#endif
