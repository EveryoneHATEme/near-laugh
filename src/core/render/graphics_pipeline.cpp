#include "graphics_pipeline.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/render/vulkan_utils.hpp"
#include "core/resources/shader_provider.hpp"
#include "core/testing/test_controls.hpp"

GraphicsPipeline::GraphicsPipeline(
    VkDevice device, VkPhysicalDevice physical_device,
    VkFormat swapchain_format, VkFormat depth_format,
    VkDescriptorSetLayout texture_descriptor_layout,
    VkDescriptorSet texture_descriptor_set,
    VkDescriptorSetLayout lighting_descriptor_layout,
    VkDescriptorSet lighting_descriptor_set,
    const std::filesystem::path& vertex_shader_path,
    const std::filesystem::path& fragment_shader_path,
    const PrototypeLevel& level)
    : device_(device),
      physical_device_(physical_device),
      texture_descriptor_layout_(texture_descriptor_layout),
      texture_descriptor_set_(texture_descriptor_set),
      lighting_descriptor_layout_(lighting_descriptor_layout),
      lighting_descriptor_set_(lighting_descriptor_set) {
  if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE ||
      texture_descriptor_layout_ == VK_NULL_HANDLE ||
      texture_descriptor_set_ == VK_NULL_HANDLE ||
      lighting_descriptor_layout_ == VK_NULL_HANDLE ||
      lighting_descriptor_set_ == VK_NULL_HANDLE) {
    throw std::runtime_error(
        "GraphicsPipeline requires valid Vulkan, texture, and lighting "
        "descriptor handles");
  }
  try {
    createPipeline(swapchain_format, depth_format, vertex_shader_path,
                   fragment_shader_path);
    createVertexBuffer(level);
    recordLifecycleEvent("pipeline.created");
    lifecycle_recorded_ = true;
  } catch (...) {
    cleanup();
    throw;
  }
}

GraphicsPipeline::~GraphicsPipeline() { cleanup(); }

VkShaderModule GraphicsPipeline::createShaderModule(
    const std::filesystem::path& path) const {
  const std::vector<std::uint32_t> words = readSpirvFile(path);
  VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  info.codeSize = words.size() * sizeof(std::uint32_t);
  info.pCode = words.data();
  VkShaderModule module = VK_NULL_HANDLE;
  const VkResult result =
      vkCreateShaderModule(device_, &info, nullptr, &module);
  if (result != VK_SUCCESS) {
    throw std::runtime_error("Create shader module for " + path.string() +
                             " failed: " + vulkanResultName(result));
  }
  return module;
}

void GraphicsPipeline::createPipeline(
    VkFormat swapchain_format, VkFormat depth_format,
    const std::filesystem::path& vertex_shader_path,
    const std::filesystem::path& fragment_shader_path) {
  VkShaderModule vertex_shader = VK_NULL_HANDLE;
  VkShaderModule fragment_shader = VK_NULL_HANDLE;
  try {
    vertex_shader = createShaderModule(vertex_shader_path);
    fragment_shader = createShaderModule(fragment_shader_path);

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertex_shader;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragment_shader;
    stages[1].pName = "main";

    const VkVertexInputBindingDescription binding =
        sceneVertexBindingDescription();
    const auto attributes = sceneVertexAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertex_input{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount =
        static_cast<std::uint32_t>(attributes.size());
    vertex_input.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport_state{
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rasterization{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization.lineWidth = 1.0F;
    VkPipelineMultisampleStateCreateInfo multisample{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo depth_stencil{
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blend_attachment;
    const std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{
        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount =
        static_cast<std::uint32_t>(dynamic_states.size());
    dynamic.pDynamicStates = dynamic_states.data();

    VkPipelineLayoutCreateInfo layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    const VkPushConstantRange camera_push_constant = scenePushConstantRange();
    const auto descriptor_layouts = sceneDescriptorSetLayouts(
        texture_descriptor_layout_, lighting_descriptor_layout_);
    layout_info.setLayoutCount =
        static_cast<std::uint32_t>(descriptor_layouts.size());
    layout_info.pSetLayouts = descriptor_layouts.data();
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &camera_push_constant;
    requireVulkan(
        vkCreatePipelineLayout(device_, &layout_info, nullptr, &layout_),
        "Create Vulkan pipeline layout");

    VkPipelineRenderingCreateInfo rendering_info{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &swapchain_format;
    rendering_info.depthAttachmentFormat = depth_format;
    VkGraphicsPipelineCreateInfo pipeline_info{
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipeline_info.pNext = &rendering_info;
    pipeline_info.stageCount = static_cast<std::uint32_t>(stages.size());
    pipeline_info.pStages = stages.data();
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterization;
    pipeline_info.pMultisampleState = &multisample;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &blend;
    pipeline_info.pDynamicState = &dynamic;
    pipeline_info.layout = layout_;
    pipeline_info.renderPass = VK_NULL_HANDLE;
    requireVulkan(
        vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipeline_info,
                                  nullptr, &pipeline_),
        "Create Dynamic Rendering graphics pipeline");
  } catch (...) {
    if (fragment_shader != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device_, fragment_shader, nullptr);
    }
    if (vertex_shader != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device_, vertex_shader, nullptr);
    }
    throw;
  }
  vkDestroyShaderModule(device_, fragment_shader, nullptr);
  vkDestroyShaderModule(device_, vertex_shader, nullptr);
}

void GraphicsPipeline::createVertexBuffer(const PrototypeLevel& level) {
  const std::vector<PositionColorVertex> vertices =
      buildPrototypeSceneVertices(level);
  if (vertices.empty() ||
      vertices.size() >
          static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    throw std::runtime_error("Prototype scene vertex count is invalid");
  }
  const VkDeviceSize vertex_bytes =
      sizeof(PositionColorVertex) * vertices.size();
  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.size = vertex_bytes;
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  requireVulkan(vkCreateBuffer(device_, &buffer_info, nullptr, &vertex_buffer_),
                "Create prototype scene vertex buffer");

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, vertex_buffer_, &requirements);
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocation.allocationSize = requirements.size;
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
  allocation.memoryTypeIndex =
      chooseMemoryType(requirements.memoryTypeBits, memory_properties,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       "prototype scene vertices");
  requireVulkan(
      vkAllocateMemory(device_, &allocation, nullptr, &vertex_memory_),
      "Allocate prototype scene vertex memory");
  requireVulkan(vkBindBufferMemory(device_, vertex_buffer_, vertex_memory_, 0),
                "Bind prototype scene vertex memory");

  void* mapped = nullptr;
  requireVulkan(
      vkMapMemory(device_, vertex_memory_, 0, vertex_bytes, 0, &mapped),
      "Map prototype scene vertex memory");
  std::memcpy(mapped, vertices.data(), static_cast<std::size_t>(vertex_bytes));
  vkUnmapMemory(device_, vertex_memory_);
  vertex_count_ = static_cast<std::uint32_t>(vertices.size());
}

void GraphicsPipeline::bindAndDraw(VkCommandBuffer command_buffer,
                                   const CameraFrame& camera,
                                   SpotLightFrame spot_light) const {
  const ScenePushConstant push_constant =
      makeScenePushConstant(camera, spot_light);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  const auto descriptor_sets =
      sceneDescriptorSets(texture_descriptor_set_, lighting_descriptor_set_);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          layout_, scene_texture_descriptor_set,
                          static_cast<std::uint32_t>(descriptor_sets.size()),
                          descriptor_sets.data(), 0, nullptr);
  vkCmdPushConstants(command_buffer, layout_,
                     scenePushConstantRange().stageFlags, 0,
                     sizeof(ScenePushConstant), &push_constant);
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_, &offset);
  vkCmdDraw(command_buffer, vertex_count_, 1, 0, 0);
}

void GraphicsPipeline::cleanup() noexcept {
  if (vertex_buffer_ != VK_NULL_HANDLE) {
    vkDestroyBuffer(device_, vertex_buffer_, nullptr);
    vertex_buffer_ = VK_NULL_HANDLE;
  }
  if (vertex_memory_ != VK_NULL_HANDLE) {
    vkFreeMemory(device_, vertex_memory_, nullptr);
    vertex_memory_ = VK_NULL_HANDLE;
  }
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, pipeline_, nullptr);
    pipeline_ = VK_NULL_HANDLE;
  }
  if (layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, layout_, nullptr);
    layout_ = VK_NULL_HANDLE;
  }
  if (lifecycle_recorded_) {
    recordLifecycleEvent("pipeline.destroyed");
    lifecycle_recorded_ = false;
  }
}
