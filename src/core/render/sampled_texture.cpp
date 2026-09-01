#include "core/render/sampled_texture.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

#include "core/render/vulkan_utils.hpp"
#include "core/resources/image_decoder.hpp"
#include "core/testing/test_controls.hpp"
#include "core/world/prototype_level.hpp"

namespace {
constexpr VkFormat prototype_texture_format = VK_FORMAT_R8G8B8A8_SRGB;
constexpr std::size_t rgba_channel_count = 4;
constexpr std::size_t texture_layer_bytes =
    static_cast<std::size_t>(prototype_texture_dimension) *
    prototype_texture_dimension * rgba_channel_count;

static_assert(prototype_texture_layer_count == prototype_surface_count);

VkImageSubresourceRange colorRange(std::uint32_t base_mip,
                                   std::uint32_t level_count) {
  return {VK_IMAGE_ASPECT_COLOR_BIT, base_mip, level_count, 0,
          prototype_texture_layer_count};
}
}  // namespace

SampledTexture::SampledTexture(
    VkDevice device, VkPhysicalDevice physical_device, VkQueue graphics_queue,
    std::uint32_t graphics_queue_family,
    const std::array<std::filesystem::path, prototype_texture_layer_count>&
        paths)
    : device_(device), physical_device_(physical_device) {
  if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE ||
      graphics_queue == VK_NULL_HANDLE) {
    throw std::invalid_argument(
        "SampledTexture requires valid Vulkan device and queue handles");
  }
  try {
    createImageAndUpload(graphics_queue, graphics_queue_family, paths);
    createImageView();
    createSampler();
    createDescriptor();
    recordLifecycleEvent("texture.created");
    owner_recorded_ = true;
  } catch (...) {
    cleanup();
    throw;
  }
}

SampledTexture::~SampledTexture() { cleanup(); }

void SampledTexture::createImageAndUpload(
    VkQueue graphics_queue, std::uint32_t graphics_queue_family,
    const std::array<std::filesystem::path, prototype_texture_layer_count>&
        paths) {
  std::vector<std::uint8_t> staging_pixels;
  staging_pixels.reserve(texture_layer_bytes * prototype_texture_layer_count);
  for (const std::filesystem::path& path : paths) {
    const DecodedRgbaImage decoded = decodePngRgba(path);
    if (decoded.width != prototype_texture_dimension ||
        decoded.height != prototype_texture_dimension ||
        decoded.pixels.size() != texture_layer_bytes) {
      throw std::runtime_error(
          "Prototype texture must decode to 256x256 forced RGBA pixels: " +
          path.string());
    }
    staging_pixels.insert(staging_pixels.end(), decoded.pixels.begin(),
                          decoded.pixels.end());
  }

  VkFormatProperties format_properties{};
  vkGetPhysicalDeviceFormatProperties(physical_device_,
                                      prototype_texture_format,
                                      &format_properties);
  requirePrototypeTextureFormatFeatures(
      format_properties.optimalTilingFeatures);
  mip_level_count_ = fullMipLevelCount(prototype_texture_dimension,
                                       prototype_texture_dimension);

  VkBuffer staging_buffer = VK_NULL_HANDLE;
  VkDeviceMemory staging_memory = VK_NULL_HANDLE;
  VkCommandPool command_pool = VK_NULL_HANDLE;
  VkFence upload_fence = VK_NULL_HANDLE;
  auto cleanup_temporary = [&]() noexcept {
    if (upload_fence != VK_NULL_HANDLE) {
      vkDestroyFence(device_, upload_fence, nullptr);
      upload_fence = VK_NULL_HANDLE;
    }
    if (command_pool != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device_, command_pool, nullptr);
      command_pool = VK_NULL_HANDLE;
    }
    if (staging_buffer != VK_NULL_HANDLE) {
      vkDestroyBuffer(device_, staging_buffer, nullptr);
      staging_buffer = VK_NULL_HANDLE;
    }
    if (staging_memory != VK_NULL_HANDLE) {
      vkFreeMemory(device_, staging_memory, nullptr);
      staging_memory = VK_NULL_HANDLE;
    }
  };

  try {
    const VkDeviceSize staging_byte_count = staging_pixels.size();
    VkBufferCreateInfo staging_info{};
    staging_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_info.size = staging_byte_count;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    requireVulkan(vkCreateBuffer(device_, &staging_info, nullptr,
                                 &staging_buffer),
                  "Create prototype texture staging buffer");
    VkMemoryRequirements staging_requirements{};
    vkGetBufferMemoryRequirements(device_, staging_buffer,
                                  &staging_requirements);
    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
    VkMemoryAllocateInfo staging_allocation{};
    staging_allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    staging_allocation.allocationSize = staging_requirements.size;
    staging_allocation.memoryTypeIndex = chooseMemoryType(
        staging_requirements.memoryTypeBits, memory_properties,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        "prototype texture staging buffer");
    requireVulkan(vkAllocateMemory(device_, &staging_allocation, nullptr,
                                   &staging_memory),
                  "Allocate prototype texture staging memory");
    requireVulkan(vkBindBufferMemory(device_, staging_buffer, staging_memory,
                                     0),
                  "Bind prototype texture staging memory");
    void* mapped = nullptr;
    requireVulkan(vkMapMemory(device_, staging_memory, 0, staging_byte_count,
                              0, &mapped),
                  "Map prototype texture staging memory");
    std::memcpy(mapped, staging_pixels.data(), staging_pixels.size());
    vkUnmapMemory(device_, staging_memory);
    if (forcedVulkanFailureAt("texture_staging")) {
      throw std::runtime_error(
          "Forced failure after prototype texture staging creation");
    }

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = prototype_texture_format;
    image_info.extent = {prototype_texture_dimension,
                         prototype_texture_dimension, 1};
    image_info.mipLevels = mip_level_count_;
    image_info.arrayLayers = prototype_texture_layer_count;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    requireVulkan(vkCreateImage(device_, &image_info, nullptr, &image_),
                  "Create prototype texture array image");
    VkMemoryRequirements image_requirements{};
    vkGetImageMemoryRequirements(device_, image_, &image_requirements);
    VkMemoryAllocateInfo image_allocation{};
    image_allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    image_allocation.allocationSize = image_requirements.size;
    image_allocation.memoryTypeIndex = chooseMemoryType(
        image_requirements.memoryTypeBits, memory_properties,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "prototype texture array image");
    requireVulkan(vkAllocateMemory(device_, &image_allocation, nullptr,
                                   &image_memory_),
                  "Allocate prototype texture device-local memory");
    requireVulkan(vkBindImageMemory(device_, image_, image_memory_, 0),
                  "Bind prototype texture device-local memory");
    recordLifecycleEvent("texture.image.created");
    image_recorded_ = true;
    if (forcedVulkanFailureAt("texture_image")) {
      throw std::runtime_error(
          "Forced failure after prototype texture image creation");
    }

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = graphics_queue_family;
    requireVulkan(
        vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool),
        "Create prototype texture upload command pool");
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo command_allocation{};
    command_allocation.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_allocation.commandPool = command_pool;
    command_allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_allocation.commandBufferCount = 1;
    requireVulkan(vkAllocateCommandBuffers(device_, &command_allocation,
                                            &command_buffer),
                  "Allocate prototype texture upload command buffer");
    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    requireVulkan(vkCreateFence(device_, &fence_info, nullptr, &upload_fence),
                  "Create prototype texture upload fence");
    if (forcedVulkanFailureAt("texture_upload")) {
      throw std::runtime_error(
          "Forced failure before prototype texture upload submission");
    }

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    requireVulkan(vkBeginCommandBuffer(command_buffer, &begin_info),
                  "Begin prototype texture upload command buffer");

    VkImageMemoryBarrier2 to_base_destination{};
    to_base_destination.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    to_base_destination.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    to_base_destination.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_base_destination.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_base_destination.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_base_destination.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_base_destination.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_base_destination.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_base_destination.image = image_;
    to_base_destination.subresourceRange = colorRange(0, 1);
    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &to_base_destination;
    vkCmdPipelineBarrier2(command_buffer, &dependency);

    VkBufferImageCopy2 copy_region{};
    copy_region.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
    copy_region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0,
                                    prototype_texture_layer_count};
    copy_region.imageExtent = {prototype_texture_dimension,
                               prototype_texture_dimension, 1};
    VkCopyBufferToImageInfo2 copy_info{};
    copy_info.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
    copy_info.srcBuffer = staging_buffer;
    copy_info.dstImage = image_;
    copy_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_info.regionCount = 1;
    copy_info.pRegions = &copy_region;
    vkCmdCopyBufferToImage2(command_buffer, &copy_info);

    std::int32_t source_width = prototype_texture_dimension;
    std::int32_t source_height = prototype_texture_dimension;
    for (std::uint32_t mip = 1; mip < mip_level_count_; ++mip) {
      std::array<VkImageMemoryBarrier2, 2> mip_barriers{};
      mip_barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      mip_barriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      mip_barriers[0].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      mip_barriers[0].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      mip_barriers[0].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      mip_barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      mip_barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      mip_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      mip_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      mip_barriers[0].image = image_;
      mip_barriers[0].subresourceRange = colorRange(mip - 1, 1);
      mip_barriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      mip_barriers[1].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
      mip_barriers[1].dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      mip_barriers[1].dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
      mip_barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      mip_barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      mip_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      mip_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      mip_barriers[1].image = image_;
      mip_barriers[1].subresourceRange = colorRange(mip, 1);
      dependency.imageMemoryBarrierCount =
          static_cast<std::uint32_t>(mip_barriers.size());
      dependency.pImageMemoryBarriers = mip_barriers.data();
      vkCmdPipelineBarrier2(command_buffer, &dependency);

      const std::int32_t destination_width =
          source_width > 1 ? source_width / 2 : 1;
      const std::int32_t destination_height =
          source_height > 1 ? source_height / 2 : 1;
      VkImageBlit2 blit_region{};
      blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
      blit_region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0,
                                    prototype_texture_layer_count};
      blit_region.srcOffsets[1] = {source_width, source_height, 1};
      blit_region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0,
                                    prototype_texture_layer_count};
      blit_region.dstOffsets[1] = {destination_width, destination_height, 1};
      VkBlitImageInfo2 blit_info{};
      blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
      blit_info.srcImage = image_;
      blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      blit_info.dstImage = image_;
      blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      blit_info.regionCount = 1;
      blit_info.pRegions = &blit_region;
      blit_info.filter = VK_FILTER_LINEAR;
      vkCmdBlitImage2(command_buffer, &blit_info);
      source_width = destination_width;
      source_height = destination_height;
    }

    std::array<VkImageMemoryBarrier2, 2> to_shader_read{};
    to_shader_read[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    to_shader_read[0].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_shader_read[0].srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    to_shader_read[0].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    to_shader_read[0].dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    to_shader_read[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    to_shader_read[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader_read[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read[0].image = image_;
    to_shader_read[0].subresourceRange = colorRange(0, mip_level_count_ - 1);
    to_shader_read[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    to_shader_read[1].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    to_shader_read[1].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    to_shader_read[1].dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    to_shader_read[1].dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    to_shader_read[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_shader_read[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    to_shader_read[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_shader_read[1].image = image_;
    to_shader_read[1].subresourceRange =
        colorRange(mip_level_count_ - 1, 1);
    dependency.imageMemoryBarrierCount =
        static_cast<std::uint32_t>(to_shader_read.size());
    dependency.pImageMemoryBarriers = to_shader_read.data();
    vkCmdPipelineBarrier2(command_buffer, &dependency);
    requireVulkan(vkEndCommandBuffer(command_buffer),
                  "End prototype texture upload command buffer");

    VkCommandBufferSubmitInfo command_submit{};
    command_submit.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_submit.commandBuffer = command_buffer;
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_submit;
    requireVulkan(vkQueueSubmit2(graphics_queue, 1, &submit_info,
                                 upload_fence),
                  "Submit prototype texture upload with Synchronization 2");
    requireVulkan(
        vkWaitForFences(device_, 1, &upload_fence, VK_TRUE,
                        std::numeric_limits<std::uint64_t>::max()),
        "Wait for prototype texture upload completion");
    all_subresources_shader_read_only_ = true;
    recordLifecycleEvent("texture.uploaded.shader_read_only");
  } catch (...) {
    cleanup_temporary();
    throw;
  }
  cleanup_temporary();
}

void SampledTexture::createImageView() {
  VkImageViewCreateInfo view_info{};
  view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  view_info.image = image_;
  view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  view_info.format = prototype_texture_format;
  view_info.subresourceRange = colorRange(0, mip_level_count_);
  requireVulkan(vkCreateImageView(device_, &view_info, nullptr, &image_view_),
                "Create prototype texture array view");
  recordLifecycleEvent("texture.view.created");
  view_recorded_ = true;
  if (forcedVulkanFailureAt("texture_view")) {
    throw std::runtime_error(
        "Forced failure after prototype texture array view creation");
  }
}

void SampledTexture::createSampler() {
  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_LINEAR;
  sampler_info.minFilter = VK_FILTER_LINEAR;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampler_info.mipLodBias = 0.0F;
  sampler_info.anisotropyEnable = VK_FALSE;
  sampler_info.compareEnable = VK_FALSE;
  sampler_info.minLod = 0.0F;
  sampler_info.maxLod = static_cast<float>(mip_level_count_ - 1);
  sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  sampler_info.unnormalizedCoordinates = VK_FALSE;
  requireVulkan(vkCreateSampler(device_, &sampler_info, nullptr, &sampler_),
                "Create prototype texture repeat/linear sampler");
  recordLifecycleEvent("texture.sampler.created");
  sampler_recorded_ = true;
  if (forcedVulkanFailureAt("texture_sampler")) {
    throw std::runtime_error(
        "Forced failure after prototype texture sampler creation");
  }
}

void SampledTexture::createDescriptor() {
  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = 1;
  layout_info.pBindings = &binding;
  requireVulkan(vkCreateDescriptorSetLayout(device_, &layout_info, nullptr,
                                             &descriptor_set_layout_),
                "Create prototype texture descriptor-set layout");
  recordLifecycleEvent("texture.descriptor_layout.created");
  descriptor_layout_recorded_ = true;
  if (forcedVulkanFailureAt("texture_descriptor_layout")) {
    throw std::runtime_error(
        "Forced failure after prototype texture descriptor layout creation");
  }

  VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  requireVulkan(vkCreateDescriptorPool(device_, &pool_info, nullptr,
                                        &descriptor_pool_),
                "Create one-set prototype texture descriptor pool");
  recordLifecycleEvent("texture.descriptor_pool.created");
  descriptor_pool_recorded_ = true;
  if (forcedVulkanFailureAt("texture_descriptor_pool")) {
    throw std::runtime_error(
        "Forced failure after prototype texture descriptor pool creation");
  }

  VkDescriptorSetAllocateInfo allocation{};
  allocation.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocation.descriptorPool = descriptor_pool_;
  allocation.descriptorSetCount = 1;
  allocation.pSetLayouts = &descriptor_set_layout_;
  requireVulkan(vkAllocateDescriptorSets(device_, &allocation,
                                          &descriptor_set_),
                "Allocate immutable prototype texture descriptor set");
  if (forcedVulkanFailureAt("texture_descriptor_set")) {
    throw std::runtime_error(
        "Forced failure after prototype texture descriptor-set allocation");
  }

  VkDescriptorImageInfo image_info{};
  image_info.sampler = sampler_;
  image_info.imageView = image_view_;
  image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = descriptor_set_;
  write.dstBinding = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &image_info;
  vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
  recordLifecycleEvent("texture.descriptor.updated");
}

void SampledTexture::cleanup() noexcept {
  descriptor_set_ = VK_NULL_HANDLE;
  if (descriptor_pool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_, descriptor_pool_, nullptr);
    descriptor_pool_ = VK_NULL_HANDLE;
    if (descriptor_pool_recorded_) {
      recordLifecycleEvent("texture.descriptor_pool.destroyed");
      descriptor_pool_recorded_ = false;
    }
  }
  if (descriptor_set_layout_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
    descriptor_set_layout_ = VK_NULL_HANDLE;
    if (descriptor_layout_recorded_) {
      recordLifecycleEvent("texture.descriptor_layout.destroyed");
      descriptor_layout_recorded_ = false;
    }
  }
  if (sampler_ != VK_NULL_HANDLE) {
    vkDestroySampler(device_, sampler_, nullptr);
    sampler_ = VK_NULL_HANDLE;
    if (sampler_recorded_) {
      recordLifecycleEvent("texture.sampler.destroyed");
      sampler_recorded_ = false;
    }
  }
  if (image_view_ != VK_NULL_HANDLE) {
    vkDestroyImageView(device_, image_view_, nullptr);
    image_view_ = VK_NULL_HANDLE;
    if (view_recorded_) {
      recordLifecycleEvent("texture.view.destroyed");
      view_recorded_ = false;
    }
  }
  if (image_ != VK_NULL_HANDLE) {
    vkDestroyImage(device_, image_, nullptr);
    image_ = VK_NULL_HANDLE;
    if (image_recorded_) {
      recordLifecycleEvent("texture.image.destroyed");
      image_recorded_ = false;
    }
  }
  if (image_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, image_memory_, nullptr);
    image_memory_ = VK_NULL_HANDLE;
  }
  all_subresources_shader_read_only_ = false;
  if (owner_recorded_) {
    recordLifecycleEvent("texture.destroyed");
    owner_recorded_ = false;
  }
}
