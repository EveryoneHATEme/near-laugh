#include "core/render/lighting_resources.hpp"

#include <cstring>
#include <stdexcept>

#include "core/render/vulkan_utils.hpp"
#include "core/testing/test_controls.hpp"

LightingResources::LightingResources(
    VkDevice device, VkPhysicalDevice physical_device,
    const PrototypeEnvironmentLight& environment_light)
    : device_(device), physical_device_(physical_device) {
  if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE) {
    throw std::invalid_argument(
        "LightingResources requires valid Vulkan device handles");
  }
  if (!prototypeEnvironmentLightIsValid(environment_light)) {
    throw std::invalid_argument(
        "LightingResources requires valid immutable prototype lighting");
  }
  try {
    createBufferAndUpload(environment_light);
    createDescriptor();
    recordLifecycleEvent("lighting.created");
    owner_recorded_ = true;
  } catch (...) {
    cleanup();
    throw;
  }
}

LightingResources::~LightingResources() { cleanup(); }

void LightingResources::createBufferAndUpload(
    const PrototypeEnvironmentLight& environment_light) {
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = sizeof(PrototypeLightingUpload);
  buffer_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  requireVulkan(vkCreateBuffer(device_, &buffer_info, nullptr, &buffer_),
                "Create immutable prototype lighting uniform buffer");
  recordLifecycleEvent("lighting.buffer.created");
  buffer_recorded_ = true;
  if (forcedVulkanFailureAt("lighting_buffer")) {
    throw std::runtime_error(
        "Forced failure after prototype lighting buffer creation");
  }

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, buffer_, &requirements);
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
  VkMemoryAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex =
      chooseMemoryType(requirements.memoryTypeBits, memory_properties,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       "immutable prototype lighting uniform buffer");
  requireVulkan(vkAllocateMemory(device_, &allocation, nullptr, &memory_),
                "Allocate immutable prototype lighting uniform memory");
  recordLifecycleEvent("lighting.memory.allocated");
  memory_recorded_ = true;
  if (forcedVulkanFailureAt("lighting_memory")) {
    throw std::runtime_error(
        "Forced failure after prototype lighting memory allocation");
  }
  requireVulkan(vkBindBufferMemory(device_, buffer_, memory_, 0),
                "Bind immutable prototype lighting uniform memory");

  const PrototypeLightingUpload upload =
      makePrototypeLightingUpload(environment_light);
  void* mapped = nullptr;
  requireVulkan(vkMapMemory(device_, memory_, 0, sizeof(upload), 0, &mapped),
                "Map immutable prototype lighting uniform memory");
  std::memcpy(mapped, &upload, sizeof(upload));
  vkUnmapMemory(device_, memory_);
  recordLifecycleEvent("lighting.uploaded");
  if (forcedVulkanFailureAt("lighting_upload")) {
    throw std::runtime_error(
        "Forced failure after prototype lighting uniform upload");
  }
}

void LightingResources::createDescriptor() {
  const VkDescriptorSetLayoutBinding binding =
      prototypeLightingDescriptorBinding();
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 1;
  layout_info.pBindings = &binding;
  requireVulkan(vkCreateDescriptorSetLayout(device_, &layout_info, nullptr,
                                            &descriptor_set_layout_),
                "Create prototype lighting descriptor-set layout");
  recordLifecycleEvent("lighting.descriptor_layout.created");
  descriptor_layout_recorded_ = true;
  if (forcedVulkanFailureAt("lighting_descriptor_layout")) {
    throw std::runtime_error(
        "Forced failure after prototype lighting descriptor layout creation");
  }

  VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  requireVulkan(
      vkCreateDescriptorPool(device_, &pool_info, nullptr, &descriptor_pool_),
      "Create one-set prototype lighting descriptor pool");
  recordLifecycleEvent("lighting.descriptor_pool.created");
  descriptor_pool_recorded_ = true;
  if (forcedVulkanFailureAt("lighting_descriptor_pool")) {
    throw std::runtime_error(
        "Forced failure after prototype lighting descriptor pool creation");
  }

  VkDescriptorSetAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation.descriptorPool = descriptor_pool_;
  allocation.descriptorSetCount = 1;
  allocation.pSetLayouts = &descriptor_set_layout_;
  requireVulkan(
      vkAllocateDescriptorSets(device_, &allocation, &descriptor_set_),
      "Allocate immutable prototype lighting descriptor set");
  if (forcedVulkanFailureAt("lighting_descriptor_set")) {
    throw std::runtime_error(
        "Forced failure after prototype lighting descriptor-set allocation");
  }

  VkDescriptorBufferInfo buffer_info{};
  buffer_info.buffer = buffer_;
  buffer_info.offset = 0;
  buffer_info.range = sizeof(PrototypeLightingUpload);
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptor_set_;
  write.dstBinding = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write.pBufferInfo = &buffer_info;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  recordLifecycleEvent("lighting.descriptor.updated");
}

void LightingResources::cleanup() noexcept {
  descriptor_set_ = VK_NULL_HANDLE;
  if (descriptor_pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
    descriptor_pool_ = VK_NULL_HANDLE;
    if (descriptor_pool_recorded_) {
      recordLifecycleEvent("lighting.descriptor_pool.destroyed");
      descriptor_pool_recorded_ = false;
    }
  }
  if (descriptor_set_layout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
    descriptor_set_layout_ = VK_NULL_HANDLE;
    if (descriptor_layout_recorded_) {
      recordLifecycleEvent("lighting.descriptor_layout.destroyed");
      descriptor_layout_recorded_ = false;
    }
  }
  if (buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_, buffer_, nullptr);
    buffer_ = VK_NULL_HANDLE;
    if (buffer_recorded_) {
      recordLifecycleEvent("lighting.buffer.destroyed");
      buffer_recorded_ = false;
    }
  }
  if (memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, memory_, nullptr);
    memory_ = VK_NULL_HANDLE;
    if (memory_recorded_) {
      recordLifecycleEvent("lighting.memory.freed");
      memory_recorded_ = false;
    }
  }
  if (owner_recorded_) {
    recordLifecycleEvent("lighting.destroyed");
    owner_recorded_ = false;
  }
}
