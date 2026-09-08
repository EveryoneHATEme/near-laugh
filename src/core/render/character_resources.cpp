#include "core/render/character_resources.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>

#include "core/render/graphics_pipeline.hpp"
#include "core/render/vulkan_utils.hpp"
#include "core/testing/test_controls.hpp"

CharacterResources::CharacterResources(
    VkDevice device, VkPhysicalDevice physical, VkQueue queue,
    std::uint32_t queue_family,
    std::span<const CharacterRenderInstance> instances)
    : device_(device), presentation_(instances) {
  if (presentation_.vertexCount() == 0) return;
  if (presentation_.vertexCount() >
      std::numeric_limits<VkDeviceSize>::max() / sizeof(PositionColorVertex))
    throw std::invalid_argument("Character vertex byte capacity overflows");
  if (presentation_.indices().size() >
      std::numeric_limits<VkDeviceSize>::max() / sizeof(std::uint32_t))
    throw std::invalid_argument("Character index byte capacity overflows");
  const VkDeviceSize bytes =
      presentation_.vertexCount() * sizeof(PositionColorVertex);
  const VkDeviceSize index_bytes =
      presentation_.indices().size() * sizeof(std::uint32_t);
  scratch_.resize(presentation_.scratchCount());
  vertices_.resize(presentation_.vertexCount());
  try {
    for (const auto& factor : presentation_.materials()) {
      SceneMaterialData material;
      material.base_color_factor = factor;
      materials_.push_back(std::make_unique<SampledTexture>(
          device, physical, queue, queue_family, material));
      if (forcedVulkanFailureAt("character_material"))
        throw std::runtime_error(
            "Forced character material allocation failure");
    }
    VkBufferCreateInfo index_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    index_info.size = index_bytes;
    index_info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    index_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    requireVulkan(vkCreateBuffer(device, &index_info, nullptr, &index_buffer_),
                  "Create character index buffer");
    recordLifecycleEvent("character.index.buffer.created");
    if (forcedVulkanFailureAt("character_index_buffer"))
      throw std::runtime_error("Forced character index buffer allocation failure");
    VkMemoryRequirements index_requirements{};
    vkGetBufferMemoryRequirements(device, index_buffer_, &index_requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical, &properties);
    VkMemoryAllocateInfo index_allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    index_allocation.allocationSize = index_requirements.size;
    index_allocation.memoryTypeIndex =
        chooseMemoryType(index_requirements.memoryTypeBits, properties,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         "character immutable indices");
    requireVulkan(vkAllocateMemory(device, &index_allocation, nullptr,
                                  &index_memory_),
                  "Allocate character index memory");
    recordLifecycleEvent("character.index.memory.allocated");
    if (forcedVulkanFailureAt("character_index_memory"))
      throw std::runtime_error("Forced character index memory allocation failure");
    requireVulkan(vkBindBufferMemory(device, index_buffer_, index_memory_, 0),
                  "Bind character index memory");
    requireVulkan(vkMapMemory(device, index_memory_, 0, index_bytes, 0,
                             &mapped_indices_),
                  "Map character index memory");
    if (forcedVulkanFailureAt("character_index_upload"))
      throw std::runtime_error("Forced character index initialization failure");
    std::memcpy(mapped_indices_, presentation_.indices().data(), index_bytes);
    // Coherent host writes finish before any submission can read these indices.
    vkUnmapMemory(device, index_memory_);
    mapped_indices_ = nullptr;
    recordLifecycleEvent("character.indices.uploaded");
    for (auto& slot : slots_) {
      VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
      info.size = bytes;
      info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      requireVulkan(vkCreateBuffer(device, &info, nullptr, &slot.buffer),
                    "Create character frame buffer");
      recordLifecycleEvent("character.buffer.created");
      if (forcedVulkanFailureAt("character_buffer"))
        throw std::runtime_error("Forced character buffer allocation failure");
      VkMemoryRequirements requirements{};
      vkGetBufferMemoryRequirements(device, slot.buffer, &requirements);
      VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
      allocation.allocationSize = requirements.size;
      allocation.memoryTypeIndex =
          chooseMemoryType(requirements.memoryTypeBits, properties,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           "character frame vertices");
      requireVulkan(
          vkAllocateMemory(device, &allocation, nullptr, &slot.memory),
          "Allocate character frame memory");
      recordLifecycleEvent("character.memory.allocated");
      if (forcedVulkanFailureAt("character_memory"))
        throw std::runtime_error("Forced character memory allocation failure");
      requireVulkan(vkBindBufferMemory(device, slot.buffer, slot.memory, 0),
                    "Bind character frame memory");
      requireVulkan(vkMapMemory(device, slot.memory, 0, bytes, 0, &slot.mapped),
                    "Map character frame memory");
      if (&slot == &slots_.back() && forcedVulkanFailureAt("character_slots"))
        throw std::runtime_error(
            "Forced character frame slot allocation failure");
    }
    recordLifecycleEvent("character.resources.created");
    owner_recorded_ = true;
  } catch (...) {
    cleanup();
    throw;
  }
}

CharacterResources::~CharacterResources() { cleanup(); }

void CharacterResources::validate(
    std::span<const CharacterPoseFrame> poses) const {
  presentation_.validate(poses);
}

void CharacterResources::deform(std::span<const CharacterPoseFrame> poses) {
  presentation_.deform(poses, scratch_, vertices_);
  if (!vertices_.empty()) recordLifecycleEvent("character.pose.deformed");
}

void CharacterResources::upload(std::size_t index) {
  auto& slot = slots_.at(index);
  if (vertices_.empty()) return;
  if (forcedVulkanFailureAt("character_upload"))
    throw std::runtime_error("Forced character upload failure");
  std::memcpy(slot.mapped, vertices_.data(),
              vertices_.size() * sizeof(PositionColorVertex));
  // Memory is persistently mapped HOST_COHERENT; no flush is required.
  slot.uploaded = true;
  recordLifecycleEvent("character.vertices.uploaded");
}

void CharacterResources::draw(VkCommandBuffer commands, std::size_t index,
                              const GraphicsPipeline& pipeline) const {
  const auto& slot = slots_.at(index);
  if (!slot.uploaded) return;
  constexpr VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(commands, 0, 1, &slot.buffer, &offset);
  vkCmdBindIndexBuffer(commands, index_buffer_, 0, VK_INDEX_TYPE_UINT32);
  for (const auto& range : presentation_.draws()) {
    pipeline.bindMaterial(commands,
                          materials_[range.material]->descriptorSet());
    vkCmdDrawIndexed(commands, range.index_count, 1, range.first_index,
                     range.vertex_offset, 0);
  }
  recordLifecycleEvent("character.vertices.drawn");
}

void CharacterResources::cleanup() noexcept {
  for (auto& slot : slots_) {
    if (slot.mapped) {
      vkUnmapMemory(device_, slot.memory);
      slot.mapped = nullptr;
    }
    if (slot.buffer) {
      vkDestroyBuffer(device_, slot.buffer, nullptr);
      slot.buffer = VK_NULL_HANDLE;
      recordLifecycleEvent("character.buffer.destroyed");
    }
    if (slot.memory) {
      vkFreeMemory(device_, slot.memory, nullptr);
      slot.memory = VK_NULL_HANDLE;
      recordLifecycleEvent("character.memory.freed");
    }
  }
  if (mapped_indices_) {
    vkUnmapMemory(device_, index_memory_);
    mapped_indices_ = nullptr;
  }
  if (index_buffer_) {
    vkDestroyBuffer(device_, index_buffer_, nullptr);
    index_buffer_ = VK_NULL_HANDLE;
    recordLifecycleEvent("character.index.buffer.destroyed");
  }
  if (index_memory_) {
    vkFreeMemory(device_, index_memory_, nullptr);
    index_memory_ = VK_NULL_HANDLE;
    recordLifecycleEvent("character.index.memory.freed");
  }
  materials_.clear();
  if (owner_recorded_) {
    recordLifecycleEvent("character.resources.destroyed");
    owner_recorded_ = false;
  }
}
