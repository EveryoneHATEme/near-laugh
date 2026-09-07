#include "core/render/text_resources.hpp"

#include <cstring>
#include <fstream>
#include <stdexcept>

#include "core/render/vulkan_utils.hpp"
#include "core/resources/shader_provider.hpp"
#include "core/testing/test_controls.hpp"

TextResources::TextResources(VkDevice device, VkPhysicalDevice physical_device,
                             VkQueue queue, unsigned queue_family,
                             VkFormat color, VkFormat depth,
                             const std::filesystem::path& root,
                             const CaptionFont& font)
    : device_(device), root_(root) {
  try {
    SceneMaterialData material;
    material.image.width = CaptionFont::atlas_size;
    material.image.height = CaptionFont::atlas_size;
    material.image.pixels.resize(font.atlas().size() * 4, 255);
    for (std::size_t i = 0; i < font.atlas().size(); ++i)
      material.image.pixels[i * 4 + 3] = font.atlas()[i];
    atlas_ = std::make_unique<SampledTexture>(device, physical_device, queue,
                                              queue_family, material);
    recordLifecycleEvent("text.atlas.created");
    if (forcedVulkanFailureAt("text-atlas"))
      throw std::runtime_error("Forced text atlas allocation failure");
    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    const auto descriptor_layout = atlas_->descriptorSetLayout();
    layout_info.setLayoutCount = 1;
    layout_info.pSetLayouts = &descriptor_layout;
    requireVulkan(
        vkCreatePipelineLayout(device_, &layout_info, nullptr, &layout_),
        "Create text pipeline layout");
    if (forcedVulkanFailureAt("text-descriptor"))
      throw std::runtime_error("Forced text descriptor/layout failure");
    recreatePipeline(color, depth);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
    for (auto& frame : frames_) {
      VkBufferCreateInfo info{};
      info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
      info.size = CaptionFont::maximum_vertices * sizeof(TextVertex);
      info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
      info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
      requireVulkan(vkCreateBuffer(device_, &info, nullptr, &frame.buffer),
                    "Create text frame buffer");
      if (forcedVulkanFailureAt("text-buffer"))
        throw std::runtime_error("Forced text buffer failure");
      VkMemoryRequirements requirements{};
      vkGetBufferMemoryRequirements(device_, frame.buffer, &requirements);
      VkMemoryAllocateInfo allocation{};
      allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocation.allocationSize = requirements.size;
      allocation.memoryTypeIndex =
          chooseMemoryType(requirements.memoryTypeBits, properties,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           "text glyph vertices");
      requireVulkan(
          vkAllocateMemory(device_, &allocation, nullptr, &frame.memory),
          "Allocate text frame memory");
      requireVulkan(vkBindBufferMemory(device_, frame.buffer, frame.memory, 0),
                    "Bind text frame memory");
      requireVulkan(
          vkMapMemory(device_, frame.memory, 0, info.size, 0, &frame.mapped),
          "Map text frame memory");
    }
    recordLifecycleEvent("text.created");
  } catch (...) {
    cleanup();
    throw;
  }
}
TextResources::~TextResources() { cleanup(); }

void TextResources::recreatePipeline(VkFormat color, VkFormat depth) {
  VkShaderModule vertex{}, fragment{};
  const auto module = [&](const std::filesystem::path& path,
                          VkShaderModule& shader) {
    const auto bytes = readSpirvFile(path);
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytes.size() * sizeof(std::uint32_t);
    info.pCode = bytes.data();
    requireVulkan(vkCreateShaderModule(device_, &info, nullptr, &shader),
                  "Create text shader module");
  };
  VkPipeline replacement{};
  try {
    module(root_ / "shaders/caption_vertex.spv", vertex);
    module(root_ / "shaders/caption_fragment.spv", fragment);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaders{};
    for (auto& stage : shaders) {
      stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      stage.pName = "main";
    }
    shaders[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaders[0].module = vertex;
    shaders[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaders[1].module = fragment;
    const VkVertexInputBindingDescription binding{0, sizeof(TextVertex),
                                                  VK_VERTEX_INPUT_RATE_VERTEX};
    const std::array<VkVertexInputAttributeDescription, 3> attributes{
        {{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, x)},
         {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TextVertex, u)},
         {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(TextVertex, color)}}};
    VkPipelineVertexInputStateCreateInfo input{};
    input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input.vertexBindingDescriptionCount = 1;
    input.pVertexBindingDescriptions = &binding;
    input.vertexAttributeDescriptionCount = attributes.size();
    input.pVertexAttributeDescriptions = attributes.data();
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo samples{};
    samples.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    samples.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth_state{};
    depth_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    VkPipelineColorBlendAttachmentState blend{};
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    blend.colorWriteMask = 15;
    VkPipelineColorBlendStateCreateInfo blend_state{};
    blend_state.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &blend;
    const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT,
                                    VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = dynamic_states.size();
    dynamic.pDynamicStates = dynamic_states.data();
    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachmentFormats = &color;
    rendering.depthAttachmentFormat = depth;
    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.pNext = &rendering;
    info.stageCount = shaders.size();
    info.pStages = shaders.data();
    info.pVertexInputState = &input;
    info.pInputAssemblyState = &assembly;
    info.pViewportState = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState = &samples;
    info.pDepthStencilState = &depth_state;
    info.pColorBlendState = &blend_state;
    info.pDynamicState = &dynamic;
    info.layout = layout_;
    if (forcedVulkanFailureAt("text-pipeline"))
      throw std::runtime_error("Forced text pipeline failure");
    requireVulkan(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info,
                                            nullptr, &replacement),
                  "Create caption pipeline");
  } catch (...) {
    if (vertex) vkDestroyShaderModule(device_, vertex, nullptr);
    if (fragment) vkDestroyShaderModule(device_, fragment, nullptr);
    if (replacement) vkDestroyPipeline(device_, replacement, nullptr);
    throw;
  }
  vkDestroyShaderModule(device_, vertex, nullptr);
  vkDestroyShaderModule(device_, fragment, nullptr);
  if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
  pipeline_ = replacement;
  recordLifecycleEvent("text.pipeline.created");
}
void TextResources::update(unsigned slot,
                           std::span<const TextVertex> vertices) {
  if (slot >= frames_.size() ||
      vertices.size() > CaptionFont::maximum_vertices || vertices.size() % 3)
    throw std::invalid_argument("invalid text frame slot or glyph count");
  auto& frame = frames_[slot];
  if (!vertices.empty())
    std::memcpy(frame.mapped, vertices.data(), vertices.size_bytes());
  frame.count = static_cast<unsigned>(vertices.size());
}
void TextResources::draw(VkCommandBuffer commands, unsigned slot) const {
  const auto& frame = frames_.at(slot);
  if (!frame.count) return;
  vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  const auto descriptor = atlas_->descriptorSet();
  vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0,
                          1, &descriptor, 0, nullptr);
  constexpr VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(commands, 0, 1, &frame.buffer, &offset);
  vkCmdDraw(commands, frame.count, 1, 0, 0);
  recordLifecycleEvent("text.drawn");
}
void TextResources::cleanup() noexcept {
  for (auto& frame : frames_) {
    if (frame.mapped) vkUnmapMemory(device_, frame.memory);
    if (frame.buffer) vkDestroyBuffer(device_, frame.buffer, nullptr);
    if (frame.memory) vkFreeMemory(device_, frame.memory, nullptr);
    frame = {};
  }
  if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
  if (layout_) vkDestroyPipelineLayout(device_, layout_, nullptr);
  pipeline_ = VK_NULL_HANDLE;
  layout_ = VK_NULL_HANDLE;
  if (atlas_) {
    atlas_.reset();
    recordLifecycleEvent("text.atlas.destroyed");
  }
}
