#include "graphics_pipeline.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <stdexcept>

#include "../resources/shader_provider.hpp"

namespace {

constexpr std::size_t kInitialVertexCapacity = 1024;
constexpr std::size_t kInitialIndexCapacity = 2048;

template <typename T>
T growCapacity(T current, T required) {
  T next = std::max<T>(current, 1);
  while (next < required) {
    next *= 2;
  }
  return next;
}

}  // namespace

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
    SDL_GPUTextureFormat depth_format,
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

  SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info{};
  pipeline_create_info.vertex_shader = vertex_shader;
  pipeline_create_info.fragment_shader = fragment_shader;
  pipeline_create_info.vertex_input_state.vertex_buffer_descriptions =
      vertex_buffer_descriptions.data();
  pipeline_create_info.vertex_input_state.num_vertex_buffers =
      vertex_buffer_descriptions.size();
  pipeline_create_info.vertex_input_state.vertex_attributes =
      vertex_attributes.data();
  pipeline_create_info.vertex_input_state.num_vertex_attributes =
      vertex_attributes.size();
  pipeline_create_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline_create_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  pipeline_create_info.depth_stencil_state.compare_op =
      SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
  pipeline_create_info.depth_stencil_state.enable_depth_test = true;
  pipeline_create_info.depth_stencil_state.enable_depth_write = true;
  pipeline_create_info.target_info.color_target_descriptions =
      color_target_descriptions.data();
  pipeline_create_info.target_info.num_color_targets =
      color_target_descriptions.size();
  pipeline_create_info.target_info.depth_stencil_format = depth_format;
  pipeline_create_info.target_info.has_depth_stencil_target = true;

  graphicsPipeline =
      SDL_CreateGPUGraphicsPipeline(device, &pipeline_create_info);
  SDL_ReleaseGPUShader(device, vertex_shader);
  SDL_ReleaseGPUShader(device, fragment_shader);

  if (graphicsPipeline == nullptr) {
    throw std::runtime_error(
        "GraphicsPipeline: CreateGPUGraphicsPipeline failed");
  }

  ensureBuffers(kInitialVertexCapacity, kInitialIndexCapacity);
}

void GraphicsPipeline::releaseBuffers() {
  if (vertexBuffer != nullptr) {
    SDL_ReleaseGPUBuffer(device, vertexBuffer);
    vertexBuffer = nullptr;
  }
  if (indexBuffer != nullptr) {
    SDL_ReleaseGPUBuffer(device, indexBuffer);
    indexBuffer = nullptr;
  }
  vertexCapacity = 0;
  indexCapacity = 0;
}

void GraphicsPipeline::ensureBuffers(std::size_t vertex_count,
                                     std::size_t index_count) {
  const bool needs_vertex_buffer =
      vertexBuffer == nullptr || vertex_count > vertexCapacity;
  const bool needs_index_buffer =
      indexBuffer == nullptr || index_count > indexCapacity;

  if (!needs_vertex_buffer && !needs_index_buffer) {
    return;
  }

  if (needs_vertex_buffer && vertexBuffer != nullptr) {
    SDL_ReleaseGPUBuffer(device, vertexBuffer);
    vertexBuffer = nullptr;
  }
  if (needs_index_buffer && indexBuffer != nullptr) {
    SDL_ReleaseGPUBuffer(device, indexBuffer);
    indexBuffer = nullptr;
  }

  if (needs_vertex_buffer) {
    vertexCapacity = growCapacity(vertexCapacity, vertex_count);
    SDL_GPUBufferCreateInfo vertex_buffer_create_info{};
    vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertex_buffer_create_info.size =
        static_cast<uint32_t>(sizeof(PositionColorVertex) * vertexCapacity);
    vertexBuffer = SDL_CreateGPUBuffer(device, &vertex_buffer_create_info);
    if (vertexBuffer == nullptr) {
      throw std::runtime_error("GraphicsPipeline: CreateGPUBuffer vertex failed");
    }
  }

  if (needs_index_buffer) {
    indexCapacity = growCapacity(indexCapacity, index_count);
    SDL_GPUBufferCreateInfo index_buffer_create_info{};
    index_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    index_buffer_create_info.size =
        static_cast<uint32_t>(sizeof(uint16_t) * indexCapacity);
    indexBuffer = SDL_CreateGPUBuffer(device, &index_buffer_create_info);
    if (indexBuffer == nullptr) {
      throw std::runtime_error("GraphicsPipeline: CreateGPUBuffer index failed");
    }
  }
}

void GraphicsPipeline::upload(const RenderPacket& packet) {
  indexCount = static_cast<uint32_t>(packet.indices.size());
  if (packet.empty()) {
    return;
  }

  ensureBuffers(packet.vertices.size(), packet.indices.size());

  const std::size_t vertex_bytes =
      packet.vertices.size() * sizeof(PositionColorVertex);
  const std::size_t index_bytes = packet.indices.size() * sizeof(uint16_t);

  SDL_GPUTransferBufferCreateInfo vertex_transfer_info{};
  vertex_transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  vertex_transfer_info.size = static_cast<uint32_t>(vertex_bytes);
  SDL_GPUTransferBuffer* vertex_transfer =
      SDL_CreateGPUTransferBuffer(device, &vertex_transfer_info);
  if (vertex_transfer == nullptr) {
    throw std::runtime_error(
        "GraphicsPipeline: CreateGPUTransferBuffer vertex failed");
  }

  SDL_GPUTransferBufferCreateInfo index_transfer_info{};
  index_transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  index_transfer_info.size = static_cast<uint32_t>(index_bytes);
  SDL_GPUTransferBuffer* index_transfer =
      SDL_CreateGPUTransferBuffer(device, &index_transfer_info);
  if (index_transfer == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, vertex_transfer);
    throw std::runtime_error(
        "GraphicsPipeline: CreateGPUTransferBuffer index failed");
  }

  void* vertex_data = SDL_MapGPUTransferBuffer(device, vertex_transfer, false);
  if (vertex_data == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, vertex_transfer);
    SDL_ReleaseGPUTransferBuffer(device, index_transfer);
    throw std::runtime_error(
        "GraphicsPipeline: MapGPUTransferBuffer vertex failed");
  }
  std::memcpy(vertex_data, packet.vertices.data(), vertex_bytes);
  SDL_UnmapGPUTransferBuffer(device, vertex_transfer);

  void* index_data = SDL_MapGPUTransferBuffer(device, index_transfer, false);
  if (index_data == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, vertex_transfer);
    SDL_ReleaseGPUTransferBuffer(device, index_transfer);
    throw std::runtime_error(
        "GraphicsPipeline: MapGPUTransferBuffer index failed");
  }
  std::memcpy(index_data, packet.indices.data(), index_bytes);
  SDL_UnmapGPUTransferBuffer(device, index_transfer);

  SDL_GPUCommandBuffer* upload_command_buffer =
      SDL_AcquireGPUCommandBuffer(device);
  if (upload_command_buffer == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, vertex_transfer);
    SDL_ReleaseGPUTransferBuffer(device, index_transfer);
    throw std::runtime_error(
        "GraphicsPipeline: AcquireGPUCommandBuffer upload failed");
  }

  SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_command_buffer);
  SDL_GPUTransferBufferLocation vertex_location{.transfer_buffer =
                                                    vertex_transfer,
                                                .offset = 0};
  SDL_GPUBufferRegion vertex_region{.buffer = vertexBuffer,
                                    .offset = 0,
                                    .size =
                                        static_cast<uint32_t>(vertex_bytes)};
  SDL_UploadToGPUBuffer(copy_pass, &vertex_location, &vertex_region, false);

  SDL_GPUTransferBufferLocation index_location{.transfer_buffer =
                                                   index_transfer,
                                               .offset = 0};
  SDL_GPUBufferRegion index_region{.buffer = indexBuffer,
                                   .offset = 0,
                                   .size = static_cast<uint32_t>(index_bytes)};
  SDL_UploadToGPUBuffer(copy_pass, &index_location, &index_region, false);
  SDL_EndGPUCopyPass(copy_pass);

  SDL_GPUFence* fence =
      SDL_SubmitGPUCommandBufferAndAcquireFence(upload_command_buffer);
  if (fence == nullptr) {
    SDL_ReleaseGPUTransferBuffer(device, vertex_transfer);
    SDL_ReleaseGPUTransferBuffer(device, index_transfer);
    throw std::runtime_error(
        "GraphicsPipeline: SubmitGPUCommandBufferAndAcquireFence upload failed");
  }

  SDL_WaitForGPUFences(device, true, &fence, 1);
  SDL_ReleaseGPUFence(device, fence);
  SDL_ReleaseGPUTransferBuffer(device, vertex_transfer);
  SDL_ReleaseGPUTransferBuffer(device, index_transfer);
}

void GraphicsPipeline::draw(SDL_GPURenderPass* render_pass) const {
  if (indexCount == 0) {
    return;
  }

  SDL_BindGPUGraphicsPipeline(render_pass, graphicsPipeline);
  SDL_GPUBufferBinding vertex_buffer_binding{vertexBuffer, 0};
  SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_buffer_binding, 1);
  SDL_GPUBufferBinding index_buffer_binding{indexBuffer, 0};
  SDL_BindGPUIndexBuffer(render_pass, &index_buffer_binding,
                         SDL_GPU_INDEXELEMENTSIZE_16BIT);
  SDL_DrawGPUIndexedPrimitives(render_pass, indexCount, 1, 0, 0, 0);
}

GraphicsPipeline::~GraphicsPipeline() {
  releaseBuffers();
  SDL_ReleaseGPUGraphicsPipeline(device, graphicsPipeline);
}
