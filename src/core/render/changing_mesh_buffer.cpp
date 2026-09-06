#include "core/render/changing_mesh_buffer.hpp"

#include <cstring>
#include <stdexcept>

#include "core/render/vulkan_utils.hpp"
#include "core/testing/test_controls.hpp"

namespace {
constexpr auto maximum_vertices = frame_maximum_opaque_box_count * 36;
constexpr VkDeviceSize byte_capacity =
    maximum_vertices * sizeof(PositionColorVertex);
}  // namespace

ChangingMeshBuffer::ChangingMeshBuffer(VkDevice device,
                                       VkPhysicalDevice physical_device)
    : device_(device) {
  try {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = byte_capacity;
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    requireVulkan(vkCreateBuffer(device_, &info, nullptr, &buffer_),
                  "Create changing opaque frame buffer");
    recordLifecycleEvent("changing.mesh.buffer.created");
    if (forcedVulkanFailureAt("changing_mesh_buffer"))
      throw std::runtime_error("Forced changing opaque allocation failure");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer_, &requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex =
        chooseMemoryType(requirements.memoryTypeBits, properties,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         "changing opaque vertices");
    requireVulkan(vkAllocateMemory(device_, &allocation, nullptr, &memory_),
                  "Allocate changing opaque frame memory");
    recordLifecycleEvent("changing.mesh.memory.allocated");
    requireVulkan(vkBindBufferMemory(device_, buffer_, memory_, 0),
                  "Bind changing opaque frame memory");
    requireVulkan(vkMapMemory(device_, memory_, 0, byte_capacity, 0, &mapped_),
                  "Map changing opaque frame memory");
    recordLifecycleEvent("changing.mesh.created");
    owner_recorded_ = true;
  } catch (...) {
    cleanup();
    throw;
  }
}

ChangingMeshBuffer::~ChangingMeshBuffer() { cleanup(); }

void ChangingMeshBuffer::update(std::span<const PositionColorVertex> vertices) {
  if (vertices.size() > maximum_vertices || vertices.size() % 3 != 0)
    throw std::runtime_error(
        "Changing opaque vertices exceed the frame profile");
  if (forcedVulkanFailureAt("changing_mesh_upload"))
    throw std::runtime_error("Forced changing opaque upload failure");
  if (!vertices.empty())
    std::memcpy(mapped_, vertices.data(), vertices.size_bytes());
  count_ = static_cast<std::uint32_t>(vertices.size());
  recordLifecycleEvent("changing.mesh.updated");
}

void ChangingMeshBuffer::draw(VkCommandBuffer commands) const {
  if (count_ == 0) return;
  constexpr VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(commands, 0, 1, &buffer_, &offset);
  vkCmdDraw(commands, count_, 1, 0, 0);
  recordLifecycleEvent("changing.mesh.drawn");
}

void ChangingMeshBuffer::cleanup() noexcept {
  if (mapped_) {
    vkUnmapMemory(device_, memory_);
    mapped_ = nullptr;
  }
  if (buffer_) {
    vkDestroyBuffer(device_, buffer_, nullptr);
    buffer_ = VK_NULL_HANDLE;
    recordLifecycleEvent("changing.mesh.buffer.destroyed");
  }
  if (memory_) {
    vkFreeMemory(device_, memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
    recordLifecycleEvent("changing.mesh.memory.freed");
  }
  if (owner_recorded_) {
    recordLifecycleEvent("changing.mesh.destroyed");
    owner_recorded_ = false;
  }
}
