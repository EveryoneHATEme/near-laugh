#include "graphics_pipeline.hpp"

#include <array>

#include "../resources/shader_provider.hpp"

struct PositionColorVertex {
  float position[3];
  uint8_t color[4];
};

SDL_GPUShader* GraphicsPipeline::createShader(
    const std::vector<uint8_t>& shader_source, ShaderType shader_type) const {
  const SDL_GPUShaderStage shader_stage =
      shader_type == ShaderType::VERTEX_SHADER ? SDL_GPU_SHADERSTAGE_VERTEX
                                               : SDL_GPU_SHADERSTAGE_FRAGMENT;

  SDL_GPUShaderCreateInfo shader_create_info = {
      .code_size = shader_source.size(),
      .code = shader_source.data(),
      .entrypoint = "main",
      .format = SDL_GPU_SHADERFORMAT_SPIRV,
      .stage = shader_stage};

  SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shader_create_info);
  if (shader == nullptr) {
    throw std::runtime_error("GraphicsPipeline: CreateGPUShader failed");
  }
  return shader;
}

GraphicsPipeline::GraphicsPipeline(
    SDL_GPUDevice* device, SDL_GPUTextureFormat swapchain_format,
    const std::filesystem::path& vertex_shader_path,
    const std::filesystem::path& fragment_shader_path)
    : device(device) {
  const ShaderProvider& shader_provider = ShaderProvider::get();
  const std::vector<uint8_t>& vertex_shader_source =
      shader_provider.readShader(vertex_shader_path);
  const std::vector<uint8_t>& fragment_shader_source =
      shader_provider.readShader(fragment_shader_path);

  SDL_GPUShader* vertex_shader =
      createShader(vertex_shader_source, ShaderType::VERTEX_SHADER);
  SDL_GPUShader* fragment_shader =
      createShader(fragment_shader_source, ShaderType::FRAGMENT_SHADER);

  std::array<SDL_GPUVertexBufferDescription, 1> vertex_buffer_descriptions{
      {{.slot = 0,
        .pitch = sizeof(PositionColorVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0}}};
  std::array<SDL_GPUVertexAttribute, 2> vertex_attributes{
      {{.location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = 0},
       {.location = 1,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
        .offset = sizeof(float) * 3}}};
  std::array<SDL_GPUColorTargetDescription, 1> color_target_descriptions = {
      {{.format = swapchain_format}}};

  SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {
      .vertex_shader = vertex_shader,
      .fragment_shader = fragment_shader,
      .vertex_input_state = {.vertex_buffer_descriptions =
                                 vertex_buffer_descriptions.data(),
                             .num_vertex_buffers =
                                 vertex_buffer_descriptions.size(),
                             .vertex_attributes = vertex_attributes.data(),
                             .num_vertex_attributes = vertex_attributes.size()},
      .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST};
  pipeline_create_info.target_info = {
      .color_target_descriptions = color_target_descriptions.data(),
      .num_color_targets = color_target_descriptions.size()};

  graphicsPipeline =
      SDL_CreateGPUGraphicsPipeline(device, &pipeline_create_info);
  if (graphicsPipeline == nullptr) {
    throw std::runtime_error(
        "GraphicsPipeline: CreateGPUGraphicsPipeline failed");
  }

  SDL_ReleaseGPUShader(device, vertex_shader);
  SDL_ReleaseGPUShader(device, fragment_shader);

  SDL_GPUBufferCreateInfo buffer_create_info{
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = sizeof(PositionColorVertex) * 3};
  vertexBuffer = SDL_CreateGPUBuffer(device, &buffer_create_info);

  SDL_GPUTransferBufferCreateInfo transfer_buffer_create_info{
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = sizeof(PositionColorVertex) * 3};
  SDL_GPUTransferBuffer* transfer_buffer =
      SDL_CreateGPUTransferBuffer(device, &transfer_buffer_create_info);

  PositionColorVertex* transfer_data = static_cast<PositionColorVertex*>(
      SDL_MapGPUTransferBuffer(device, transfer_buffer, false));
  transfer_data[0] = {.position = {-1, -1, 0}, .color = {255, 0, 0, 255}};
  transfer_data[1] = {.position = {1, -1, 0}, .color = {0, 255, 0, 255}};
  transfer_data[2] = {.position = {0, 1, 0}, .color = {0, 0, 255, 255}};
  SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

  SDL_GPUCommandBuffer* upload_command_buffer =
      SDL_AcquireGPUCommandBuffer(device);
  if (upload_command_buffer == nullptr) {
    throw std::runtime_error(
        "GraphicsPipeline: AcquireGPUCommandBuffer failed");
  }

  SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_command_buffer);
  SDL_GPUTransferBufferLocation transfer_buffer_location{
      .transfer_buffer = transfer_buffer, .offset = 0};
  SDL_GPUBufferRegion buffer_region{.buffer = vertexBuffer,
                                    .offset = 0,
                                    .size = sizeof(PositionColorVertex) * 3};
  SDL_UploadToGPUBuffer(copy_pass, &transfer_buffer_location, &buffer_region,
                        false);
  SDL_EndGPUCopyPass(copy_pass);

  SDL_GPUFence* fence =
      SDL_SubmitGPUCommandBufferAndAcquireFence(upload_command_buffer);
  if (fence == nullptr) {
    throw std::runtime_error(
        "GraphicsPipeline: SubmitGPUCommandBufferAndAcquireFence failed");
  }
  SDL_WaitForGPUFences(device, true, &fence, 1);
  SDL_ReleaseGPUFence(device, fence);

  SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
}

void GraphicsPipeline::draw(SDL_GPURenderPass* render_pass) const {
  SDL_BindGPUGraphicsPipeline(render_pass, graphicsPipeline);
  SDL_GPUBufferBinding buffer_binding{vertexBuffer, 0};
  SDL_BindGPUVertexBuffers(render_pass, 0, &buffer_binding, 1);
  SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
}

GraphicsPipeline::~GraphicsPipeline() {
  SDL_ReleaseGPUGraphicsPipeline(device, graphicsPipeline);
  SDL_ReleaseGPUBuffer(device, vertexBuffer);
}
