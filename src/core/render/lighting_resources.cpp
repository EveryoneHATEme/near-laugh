#include "core/render/lighting_resources.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include "core/render/changing_mesh_buffer.hpp"
#include "core/render/character_resources.hpp"
#include "core/render/graphics_pipeline.hpp"
#include "core/render/scene_resources.hpp"
#include "core/render/vulkan_utils.hpp"
#include "core/testing/test_controls.hpp"

namespace {
void failure(const char* point) {
  if (forcedVulkanFailureAt(point))
    throw std::runtime_error(std::string("Forced lighting failure at ") +
                             point);
}
constexpr auto depth_stages = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                              VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
VkFormat shadowFormat(VkPhysicalDevice physical, std::uint32_t layers) {
  std::array<PointShadowFormatSupport, 2> support{};
  std::size_t index = 0;
  for (auto format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}) {
    auto& candidate = support[index++];
    candidate.format = format;
    if (format == VK_FORMAT_D32_SFLOAT &&
        forcedVulkanFailureAt("shadow_d32_unavailable"))
      continue;
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(physical, format, &properties);
    candidate.features = properties.optimalTilingFeatures;
    const auto result = vkGetPhysicalDeviceImageFormatProperties(
        physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT,
        0, &candidate.image);
    if (result != VK_SUCCESS) candidate.image = {};
  }
  return choosePointShadowFormat(support, layers);
}
}  // namespace
VkFormat choosePointShadowFormat(
    std::span<const PointShadowFormatSupport> support, std::uint32_t layers) {
  if (layers != 1 && (layers == 0 || layers > 24 || layers % 6 != 0))
    throw std::invalid_argument(
        "Point shadow array requires one dummy or six layers per bounded "
        "caster");
  constexpr auto required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
                            VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  for (auto format : {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM}) {
    for (const auto& candidate : support) {
      const auto& image = candidate.image;
      // Counts are rejected before arithmetic, so this cannot overflow.
      const auto bytes = VkDeviceSize(point_shadow_face_size) *
                         point_shadow_face_size * layers *
                         (format == VK_FORMAT_D32_SFLOAT ? 4 : 2);
      if (candidate.format == format &&
          (candidate.features & required) == required &&
          image.maxExtent.width >= point_shadow_face_size &&
          image.maxExtent.height >= point_shadow_face_size &&
          image.maxArrayLayers >= layers &&
          (image.sampleCounts & VK_SAMPLE_COUNT_1_BIT) &&
          image.maxResourceSize >= bytes)
        return format;
    }
  }
  throw std::runtime_error(
      "Interior lighting requires sampled D32 or D16 depth arrays at 512 "
      "pixels");
}
void validatePointLightEnables(std::span<const std::uint8_t> enabled,
                               std::size_t count) {
  if (count > level_maximum_point_light_count || enabled.size() != count ||
      std::any_of(enabled.begin(), enabled.end(),
                  [](auto value) { return value > 1; }))
    throw std::invalid_argument(
        "Point-light frame must contain exactly one 0/1 enable per loaded "
        "light");
}
PrototypeLightingUpload makePrototypeLightingUpload(
    const PrototypeEnvironmentLight& environment) {
  if (!prototypeEnvironmentLightIsValid(environment))
    throw std::invalid_argument(
        "Lighting requires valid bounded point lights and ambient");
  PrototypeLightingUpload upload{};
  upload.ambient_intensity = {
      environment.ambient_intensity,
      static_cast<float>(environment.point_lights.size()), 0, 0};
  std::size_t layer = 0;
  for (std::size_t i = 0; i < environment.point_lights.size(); ++i) {
    const auto& light = environment.point_lights[i];
    auto& target = upload.point_lights[i];
    target.position_and_radius = {light.position.x, light.position.y,
                                  light.position.z, light.radius};
    target.color_and_intensity = {light.color[0], light.color[1],
                                  light.color[2], light.intensity};
    target.controls = {light.initially_on ? 1.F : 0.F, -1, 0, 0};
    if (light.casts_shadows) {
      target.controls[1] = static_cast<float>(layer);
      for (std::size_t face = 0; face < 6; ++face)
        upload.shadow_cameras[layer++] =
            pointShadowCamera(light.position, light.radius, face);
    }
  }
  return upload;
}

LightingResources::LightingResources(
    VkDevice device, VkPhysicalDevice physical,
    const PrototypeEnvironmentLight& environment)
    : device_(device),
      physical_(physical),
      light_count_(environment.point_lights.size()) {
  if (!device_ || !physical_)
    throw std::invalid_argument("Lighting requires Vulkan device handles");
  const auto upload = makePrototypeLightingUpload(environment);
  const auto casters = std::count_if(
      environment.point_lights.begin(), environment.point_lights.end(),
      [](const auto& light) { return light.casts_shadows; });
  // Validation above bounds the multiplication to 24 layers.
  layer_count_ =
      static_cast<std::uint32_t>(std::max<std::ptrdiff_t>(1, casters * 6));
  depth_format_ = shadowFormat(physical_, layer_count_);
  try {
    for (auto& slot : slots_) {
      slot.upload = upload;
      // D16 rounds projected depth to UNORM. Account for half a stored unit
      // separately from the world-space contact bias used by both formats.
      slot.upload.ambient_intensity[2] =
          depth_format_ == VK_FORMAT_D16_UNORM ? .5F / 65535 : 0;
      createSlot(slot);
    }
    createDescriptors();
    recordLifecycleEvent("lighting.created");
    owner_recorded_ = true;
    std::cout << "Point shadow depth: "
              << (depth_format_ == VK_FORMAT_D32_SFLOAT ? "D32" : "D16") << ", "
              << layer_count_ << " layers per slot, " << shadow_memory_bytes_
              << " allocated bytes across two slots\n";
  } catch (...) {
    cleanup();
    throw;
  }
}
LightingResources::~LightingResources() { cleanup(); }

void LightingResources::createSlot(Slot& slot) {
  VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer.size = sizeof(PrototypeLightingUpload);
  buffer.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
  buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  requireVulkan(vkCreateBuffer(device_, &buffer, nullptr, &slot.buffer),
                "Create frame lighting buffer");
  recordLifecycleEvent("lighting.buffer.created");
  failure("lighting_buffer");
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_, &properties);
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, slot.buffer, &requirements);
  VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocate.allocationSize = requirements.size;
  allocate.memoryTypeIndex =
      chooseMemoryType(requirements.memoryTypeBits, properties,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       "frame lighting");
  requireVulkan(
      vkAllocateMemory(device_, &allocate, nullptr, &slot.uniform_memory),
      "Allocate frame lighting");
  recordLifecycleEvent("lighting.memory.allocated");
  failure("lighting_memory");
  requireVulkan(
      vkBindBufferMemory(device_, slot.buffer, slot.uniform_memory, 0),
      "Bind frame lighting");
  requireVulkan(vkMapMemory(device_, slot.uniform_memory, 0,
                            sizeof(slot.upload), 0, &slot.mapped),
                "Map frame lighting");
  std::memcpy(slot.mapped, &slot.upload, sizeof(slot.upload));
  recordLifecycleEvent("lighting.uploaded");
  failure("lighting_upload");

  VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  image.imageType = VK_IMAGE_TYPE_2D;
  image.format = depth_format_;
  image.extent = {point_shadow_face_size, point_shadow_face_size, 1};
  image.mipLevels = 1;
  image.arrayLayers = layer_count_;
  image.samples = VK_SAMPLE_COUNT_1_BIT;
  image.tiling = VK_IMAGE_TILING_OPTIMAL;
  image.usage =
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  requireVulkan(vkCreateImage(device_, &image, nullptr, &slot.image),
                "Create point shadow array");
  recordLifecycleEvent("shadow.image.created");
  failure("shadow_image");
  vkGetImageMemoryRequirements(device_, slot.image, &requirements);
  allocate.allocationSize = requirements.size;
  allocate.memoryTypeIndex = chooseMemoryType(
      requirements.memoryTypeBits, properties,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "point shadow array");
  requireVulkan(
      vkAllocateMemory(device_, &allocate, nullptr, &slot.image_memory),
      "Allocate point shadows");
  shadow_memory_bytes_ += requirements.size;
  recordLifecycleEvent("shadow.memory.allocated");
  failure("shadow_memory");
  requireVulkan(vkBindImageMemory(device_, slot.image, slot.image_memory, 0),
                "Bind point shadows");
  VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  view.image = slot.image;
  view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  view.format = depth_format_;
  view.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, layer_count_};
  requireVulkan(vkCreateImageView(device_, &view, nullptr, &slot.array_view),
                "Create sampled shadow array view");
  recordLifecycleEvent("shadow.array_view.created");
  failure("shadow_array_view");
  slot.faces.resize(layer_count_);
  view.viewType = VK_IMAGE_VIEW_TYPE_2D;
  view.subresourceRange.layerCount = 1;
  for (std::uint32_t layer = 0; layer < layer_count_; ++layer) {
    view.subresourceRange.baseArrayLayer = layer;
    requireVulkan(
        vkCreateImageView(device_, &view, nullptr, &slot.faces[layer]),
        "Create shadow face view");
    recordLifecycleEvent("shadow.face_view.created");
    failure("shadow_face_view");
  }
}

void LightingResources::createDescriptors() {
  const std::array<VkDescriptorSetLayoutBinding, 2> bindings{
      prototypeLightingDescriptorBinding(),
      VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
  VkDescriptorSetLayoutCreateInfo layout{
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
  layout.bindingCount = static_cast<std::uint32_t>(bindings.size());
  layout.pBindings = bindings.data();
  requireVulkan(
      vkCreateDescriptorSetLayout(device_, &layout, nullptr, &layout_),
      "Create lighting layout");
  recordLifecycleEvent("lighting.descriptor_layout.created");
  failure("lighting_descriptor_layout");
  const std::array<VkDescriptorPoolSize, 2> sizes{
      {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, lighting_frame_slot_count},
       {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, lighting_frame_slot_count}}};
  VkDescriptorPoolCreateInfo pool{
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  pool.maxSets = lighting_frame_slot_count;
  pool.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
  pool.pPoolSizes = sizes.data();
  requireVulkan(vkCreateDescriptorPool(device_, &pool, nullptr, &pool_),
                "Create lighting descriptor pool");
  recordLifecycleEvent("lighting.descriptor_pool.created");
  failure("lighting_descriptor_pool");
  VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  sampler.magFilter = sampler.minFilter = VK_FILTER_NEAREST;
  sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler.addressModeU = sampler.addressModeV = sampler.addressModeW =
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler.maxLod = 0;
  requireVulkan(vkCreateSampler(device_, &sampler, nullptr, &sampler_),
                "Create manual shadow comparison sampler");
  recordLifecycleEvent("shadow.sampler.created");
  failure("shadow_sampler");
  for (auto& slot : slots_) {
    VkDescriptorSetAllocateInfo allocate{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool = pool_;
    allocate.descriptorSetCount = 1;
    allocate.pSetLayouts = &layout_;
    requireVulkan(vkAllocateDescriptorSets(device_, &allocate, &slot.set),
                  "Allocate frame lighting set");
    failure("lighting_descriptor_set");
    VkDescriptorBufferInfo buffer{slot.buffer, 0,
                                  sizeof(PrototypeLightingUpload)};
    VkDescriptorImageInfo image{sampler_, slot.array_view,
                                VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (auto& write : writes) {
      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = slot.set;
      write.descriptorCount = 1;
    }
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &buffer;
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &image;
    vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()),
                           writes.data(), 0, nullptr);
    recordLifecycleEvent("lighting.descriptor.updated");
  }
}
void LightingResources::createShadowPipeline(
    const SceneResources& scene, const std::filesystem::path& shaders) {
  auto pipeline = std::make_unique<GraphicsPipeline>(
      device_, VK_FORMAT_UNDEFINED, depth_format_, scene.materialLayout(),
      scene.firstMaterial(), layout_, descriptorSet(),
      shaders / "point_shadow_vertex.spv",
      shaders / "point_shadow_fragment.spv",
      GraphicsPipeline::Pass::PointShadow);
  failure("shadow_pipeline");
  shadow_pipeline_ = std::move(pipeline);
}
void LightingResources::validateEnables(
    std::span<const std::uint8_t> enabled) const {
  validatePointLightEnables(enabled, light_count_);
}
void LightingResources::update(std::size_t index,
                               std::span<const std::uint8_t> enabled) {
  validateEnables(enabled);
  auto& slot = slots_.at(index);
  for (std::size_t i = 0; i < light_count_; ++i)
    slot.upload.point_lights[i].controls[0] = enabled[i];
  std::memcpy(slot.mapped, &slot.upload, sizeof(slot.upload));
}
void LightingResources::recordShadows(VkCommandBuffer commands,
                                      std::size_t index,
                                      const SceneResources& scene,
                                      const ChangingMeshBuffer* changing,
                                      const ImmutableMeshBuffer* editor_doors,
                                      const CharacterResources* characters) {
  auto& slot = slots_.at(index);
  if (!shadow_pipeline_)
    throw std::logic_error("Shadow pipeline was not prepared");
  VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
  barrier.srcStageMask = slot.initialized
                             ? VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT
                             : VK_PIPELINE_STAGE_2_NONE;
  barrier.srcAccessMask =
      slot.initialized ? VK_ACCESS_2_SHADER_SAMPLED_READ_BIT : 0;
  barrier.dstStageMask = depth_stages;
  barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.oldLayout = slot.initialized ? VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL
                                       : VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex =
      VK_QUEUE_FAMILY_IGNORED;
  barrier.image = slot.image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, layer_count_};
  VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
  dependency.imageMemoryBarrierCount = 1;
  dependency.pImageMemoryBarriers = &barrier;
  vkCmdPipelineBarrier2(commands, &dependency);

  const VkViewport viewport{
      0, 0, float(point_shadow_face_size), float(point_shadow_face_size), 0, 1};
  const VkRect2D scissor{{0, 0},
                         {point_shadow_face_size, point_shadow_face_size}};
  vkCmdSetViewport(commands, 0, 1, &viewport);
  vkCmdSetScissor(commands, 0, 1, &scissor);
  for (std::uint32_t layer = 0; layer < layer_count_; ++layer) {
    bool enabled = false;
    for (std::size_t i = 0; i < light_count_; ++i) {
      const auto& control = slot.upload.point_lights[i].controls;
      if (control[1] >= 0 && layer >= control[1] && layer < control[1] + 6)
        enabled = control[0] > .5F;
    }
    // Initialize every descriptor-visible layer once. Disabled sources need
    // no passes after that; enabling a source always redraws all six faces.
    if (!enabled && slot.initialized) continue;
    VkRenderingAttachmentInfo depth{
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    depth.imageView = slot.faces[layer];
    depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth.clearValue.depthStencil = {1, 0};
    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering.renderArea = scissor;
    rendering.layerCount = 1;
    rendering.pDepthAttachment = &depth;
    vkCmdBeginRendering(commands, &rendering);
    if (enabled) {
      shadow_pipeline_->bindSceneState(
          commands, slot.upload.shadow_cameras[layer], {}, slot.set);
      scene.draw(commands, *shadow_pipeline_);
      if (changing || editor_doors) {
        shadow_pipeline_->bindMaterial(commands, scene.obstacleMaterial());
        if (changing) changing->draw(commands);
        if (editor_doors) editor_doors->bindAndDraw(commands);
      }
      if (characters) characters->draw(commands, index, *shadow_pipeline_);
    }
    vkCmdEndRendering(commands);
  }
  barrier.srcStageMask = depth_stages;
  barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
  vkCmdPipelineBarrier2(commands, &dependency);
  slot.initialized = true;
}
void LightingResources::cleanup() noexcept {
  shadow_pipeline_.reset();
  if (pool_) {
    vkDestroyDescriptorPool(device_, pool_, nullptr);
    pool_ = {};
    recordLifecycleEvent("lighting.descriptor_pool.destroyed");
  }
  if (sampler_) {
    vkDestroySampler(device_, sampler_, nullptr);
    sampler_ = {};
    recordLifecycleEvent("shadow.sampler.destroyed");
  }
  if (layout_) {
    vkDestroyDescriptorSetLayout(device_, layout_, nullptr);
    layout_ = {};
    recordLifecycleEvent("lighting.descriptor_layout.destroyed");
  }
  for (auto& slot : slots_) {
    for (auto view : slot.faces)
      if (view) {
        vkDestroyImageView(device_, view, nullptr);
        recordLifecycleEvent("shadow.face_view.destroyed");
      }
    slot.faces.clear();
    if (slot.array_view) {
      vkDestroyImageView(device_, slot.array_view, nullptr);
      slot.array_view = {};
      recordLifecycleEvent("shadow.array_view.destroyed");
    }
    if (slot.image) {
      vkDestroyImage(device_, slot.image, nullptr);
      slot.image = {};
      recordLifecycleEvent("shadow.image.destroyed");
    }
    if (slot.image_memory) {
      vkFreeMemory(device_, slot.image_memory, nullptr);
      slot.image_memory = {};
      recordLifecycleEvent("shadow.memory.freed");
    }
    if (slot.mapped) {
      vkUnmapMemory(device_, slot.uniform_memory);
      slot.mapped = nullptr;
    }
    if (slot.buffer) {
      vkDestroyBuffer(device_, slot.buffer, nullptr);
      slot.buffer = {};
      recordLifecycleEvent("lighting.buffer.destroyed");
    }
    if (slot.uniform_memory) {
      vkFreeMemory(device_, slot.uniform_memory, nullptr);
      slot.uniform_memory = {};
      recordLifecycleEvent("lighting.memory.freed");
    }
  }
  if (owner_recorded_) {
    recordLifecycleEvent("lighting.destroyed");
    owner_recorded_ = false;
  }
}
