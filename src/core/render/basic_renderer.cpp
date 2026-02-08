#include "basic_renderer.hpp"

SDL_GPUShader* BasicRenderer::CreateShader(SDL_GPUShaderStage shader_stage,
                                           const std::string& source_path) {
  // TODO: move to resource manager

  size_t source_size;
  void* shader_source = SDL_LoadFile(source_path.c_str(), &source_size);
  if (shader_source == nullptr) {
    SDL_Log("Failed to load shader: %s", source_path.c_str());
  }

  SDL_GPUShaderCreateInfo shader_create_info = {
      .code_size = source_size,
      .code = static_cast<const unsigned char*>(shader_source),
      .entrypoint = "main",
      .format = SDL_GPU_SHADERFORMAT_SPIRV,
      .stage = shader_stage};

  SDL_GPUShader* shader = SDL_CreateGPUShader(gpuDevice, &shader_create_info);
  SDL_free(shader_source);

  if (shader == nullptr) {
    SDL_Log("Failed to create shader: %s", source_path.c_str());
    return nullptr;
  }

  return shader;
}

BasicRenderer::BasicRenderer(SDL_GPUDevice* gpu_device, SDL_Window* window,
                             const std::string& vertex_shader_source_path,
                             const std::string& fragment_shader_source_path) {
  gpuDevice = gpu_device;

  SDL_GPUShader* vertex_shader =
      CreateShader(SDL_GPU_SHADERSTAGE_VERTEX, vertex_shader_source_path);

  SDL_GPUShader* fragment_shader =
      CreateShader(SDL_GPU_SHADERSTAGE_FRAGMENT, fragment_shader_source_path);

  SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {
      .vertex_shader = vertex_shader, .fragment_shader = fragment_shader};

  pipeline_create_info.target_info = {
      .color_target_descriptions =
          (SDL_GPUColorTargetDescription[]){
              {.format = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window)}},
      .num_color_targets = 1};

  pipeline_create_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  pipeline_create_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

  pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_create_info);
  if (pipeline == nullptr) {
    SDL_Log("Failed to create graphics pipeline");
  }

  SDL_ReleaseGPUShader(gpuDevice, vertex_shader);
  SDL_ReleaseGPUShader(gpuDevice, fragment_shader);
}

BasicRenderer::~BasicRenderer() {
  SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
}

void BasicRenderer::Draw(SDL_Window* window) {
  SDL_GPUCommandBuffer* command_buffer =
      SDL_AcquireGPUCommandBuffer(gpuDevice);
  if (command_buffer == nullptr) {
    SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
    return;
  }

  SDL_GPUTexture* swapchain_texture;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, window,
                                             &swapchain_texture, NULL, NULL)) {
    SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
    return;
  }

  if (swapchain_texture != NULL) {
    SDL_GPUColorTargetInfo colorTargetInfo = {0};
    colorTargetInfo.texture = swapchain_texture;
    colorTargetInfo.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass* render_pass =
        SDL_BeginGPURenderPass(command_buffer, &colorTargetInfo, 1, NULL);
    SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

    SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(render_pass);
  }

  SDL_SubmitGPUCommandBuffer(command_buffer);
}
