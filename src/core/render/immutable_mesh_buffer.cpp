#include "core/render/immutable_mesh_buffer.hpp"

#include <cstring>
#include <stdexcept>

#include "core/render/vulkan_utils.hpp"
#include "core/testing/test_controls.hpp"

ImmutableMeshBuffer::ImmutableMeshBuffer(
    VkDevice device, VkPhysicalDevice physical_device,
    std::span<const PositionColorVertex> vertices,
    std::string_view lifecycle_name)
    : device_(device), lifecycle_name_(lifecycle_name) {
  if (!immutableMeshVertexCountIsValid(vertices.size())) {
    throw std::invalid_argument(
        "Immutable mesh buffer requires a non-empty complete triangle stream "
        "within the 32-bit draw limit");
  }
  if (device_ == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE) {
    throw std::invalid_argument(
        "Immutable mesh buffer requires valid Vulkan device handles");
  }
  if (lifecycle_name_ != "world" && lifecycle_name_ != "chair" &&
      lifecycle_name_ != "prop" && lifecycle_name_ != "door_preview") {
    throw std::invalid_argument(
        "Immutable mesh buffer requires a concrete scene mesh name");
  }

  try {
    const VkDeviceSize vertex_bytes =
        sizeof(PositionColorVertex) * vertices.size();
    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = vertex_bytes;
    buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    requireVulkan(vkCreateBuffer(device_, &buffer_info, nullptr, &buffer_),
                  "Create " + lifecycle_name_ + " immutable mesh buffer");
    recordLifecycleEvent(event("buffer.created"));
    if (forcedVulkanFailureAt(failureStage("buffer").c_str())) {
      throw std::runtime_error("Forced " + lifecycle_name_ +
                               " mesh buffer creation failure");
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer_, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);
    allocation.memoryTypeIndex =
        chooseMemoryType(requirements.memoryTypeBits, memory_properties,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         lifecycle_name_ + " immutable mesh vertices");
    requireVulkan(vkAllocateMemory(device_, &allocation, nullptr, &memory_),
                  "Allocate " + lifecycle_name_ + " immutable mesh memory");
    recordLifecycleEvent(event("memory.allocated"));
    if (forcedVulkanFailureAt(failureStage("memory").c_str())) {
      throw std::runtime_error("Forced " + lifecycle_name_ +
                               " mesh memory allocation failure");
    }

    requireVulkan(vkBindBufferMemory(device_, buffer_, memory_, 0),
                  "Bind " + lifecycle_name_ + " immutable mesh memory");
    if (forcedVulkanFailureAt(failureStage("bind").c_str())) {
      throw std::runtime_error("Forced " + lifecycle_name_ +
                               " mesh memory binding failure");
    }

    void* mapped = nullptr;
    requireVulkan(vkMapMemory(device_, memory_, 0, vertex_bytes, 0, &mapped),
                  "Map " + lifecycle_name_ + " immutable mesh memory");
    if (forcedVulkanFailureAt(failureStage("map").c_str())) {
      vkUnmapMemory(device_, memory_);
      throw std::runtime_error("Forced " + lifecycle_name_ +
                               " mesh memory mapping failure");
    }
    std::memcpy(mapped, vertices.data(),
                static_cast<std::size_t>(vertex_bytes));
    vkUnmapMemory(device_, memory_);
    recordLifecycleEvent(event("uploaded"));
    if (forcedVulkanFailureAt(failureStage("upload").c_str())) {
      throw std::runtime_error("Forced " + lifecycle_name_ +
                               " mesh upload failure");
    }

    vertex_count_ = static_cast<std::uint32_t>(vertices.size());
    recordLifecycleEvent(event("created"));
    lifecycle_recorded_ = true;
  } catch (...) {
    cleanup();
    throw;
  }
}

ImmutableMeshBuffer::~ImmutableMeshBuffer() { cleanup(); }

void ImmutableMeshBuffer::bindAndDraw(VkCommandBuffer command_buffer) const {
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer_, &offset);
  vkCmdDraw(command_buffer, vertex_count_, 1, 0, 0);
  if (!draw_recorded_) {
    recordLifecycleEvent(event("drawn"));
    draw_recorded_ = true;
  }
}

std::string ImmutableMeshBuffer::event(std::string_view suffix) const {
  return lifecycle_name_ + ".mesh." + std::string(suffix);
}

std::string ImmutableMeshBuffer::failureStage(std::string_view suffix) const {
  return lifecycle_name_ + "_mesh_" + std::string(suffix);
}

void ImmutableMeshBuffer::cleanup() noexcept {
  if (buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_, buffer_, nullptr);
    buffer_ = VK_NULL_HANDLE;
    recordLifecycleEvent(event("buffer.destroyed"));
  }
  if (memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
    recordLifecycleEvent(event("memory.freed"));
  }
  if (lifecycle_recorded_) {
    recordLifecycleEvent(event("destroyed"));
    lifecycle_recorded_ = false;
  }
}
