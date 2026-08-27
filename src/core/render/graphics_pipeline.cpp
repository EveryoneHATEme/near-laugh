#include "graphics_pipeline.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/render/vulkan_utils.hpp"
#include "core/resources/shader_provider.hpp"

GraphicsPipeline::GraphicsPipeline(
    VkDevice device, VkPhysicalDevice physical_device,
    VkFormat swapchain_format, const std::filesystem::path& vertex_shader_path,
    const std::filesystem::path& fragment_shader_path)
    : device_(device), physical_device_(physical_device) {
  if (device_ == VK_NULL_HANDLE || physical_device_ == VK_NULL_HANDLE) {
    throw std::runtime_error("GraphicsPipeline requires valid Vulkan handles");
  }
  try {
    createPipeline(swapchain_format, vertex_shader_path, fragment_shader_path);
    createVertexBuffer();
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
    VkFormat swapchain_format, const std::filesystem::path& vertex_shader_path,
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
        triangleVertexBindingDescription();
    const auto attributes = triangleVertexAttributeDescriptions();
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
    requireVulkan(
        vkCreatePipelineLayout(device_, &layout_info, nullptr, &layout_),
        "Create Vulkan pipeline layout");

    VkPipelineRenderingCreateInfo rendering_info{
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering_info.colorAttachmentCount = 1;
    rendering_info.pColorAttachmentFormats = &swapchain_format;
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

void GraphicsPipeline::createVertexBuffer() {
  constexpr std::array<PositionColorVertex, 3> vertices = {{
      {{-1.0F, -1.0F, 0.0F}, {255, 0, 0, 255}},
      {{1.0F, -1.0F, 0.0F}, {0, 255, 0, 255}},
      {{0.0F, 1.0F, 0.0F}, {0, 0, 255, 255}},
  }};
  VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  buffer_info.size = sizeof(vertices);
  buffer_info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  requireVulkan(vkCreateBuffer(device_, &buffer_info, nullptr, &vertex_buffer_),
                "Create triangle vertex buffer");

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device_, vertex_buffer_, &requirements);
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
  allocation.allocationSize = requirements.size;
  allocation.memoryTypeIndex = findMemoryType(
      requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  requireVulkan(
      vkAllocateMemory(device_, &allocation, nullptr, &vertex_memory_),
      "Allocate triangle vertex memory");
  requireVulkan(vkBindBufferMemory(device_, vertex_buffer_, vertex_memory_, 0),
                "Bind triangle vertex memory");

  void* mapped = nullptr;
  requireVulkan(
      vkMapMemory(device_, vertex_memory_, 0, sizeof(vertices), 0, &mapped),
      "Map triangle vertex memory");
  std::memcpy(mapped, vertices.data(), sizeof(vertices));
  vkUnmapMemory(device_, vertex_memory_);
}

std::uint32_t GraphicsPipeline::findMemoryType(
    std::uint32_t type_bits, VkMemoryPropertyFlags properties) const {
  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device_, &memory_properties);
  for (std::uint32_t index = 0; index < memory_properties.memoryTypeCount;
       ++index) {
    if ((type_bits & (1U << index)) != 0 &&
        (memory_properties.memoryTypes[index].propertyFlags & properties) ==
            properties) {
      return index;
    }
  }
  throw std::runtime_error(
      "No host-visible coherent Vulkan memory type for triangle vertices");
}

void GraphicsPipeline::bindAndDraw(VkCommandBuffer command_buffer) const {
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
  const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer_, &offset);
  vkCmdDraw(command_buffer, 3, 1, 0, 0);
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
}
