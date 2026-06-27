#include "renderer.hpp"

#include <stdexcept>

Renderer::Renderer(SDL_Window* window) : window(window) {
  if (window == nullptr) {
    throw std::runtime_error("Renderer: window is nullptr");
  }

  SDL_GPUDevice* gpu_device =
      SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
  if (gpu_device == nullptr) {
    throw std::runtime_error("Renderer: CreateGPUDevice failed");
  }
  device.reset(gpu_device);

  if (!SDL_ClaimWindowForGPUDevice(device.get(), window)) {
    throw std::runtime_error("Renderer: ClaimWindowForGPUDevice failed");
  }
  frames.resize(FRAMES_IN_FLIGHT);
}

Renderer::~Renderer() {
  releaseDepthTexture();
  SDL_ReleaseWindowFromGPUDevice(device.get(), window);
}

void Renderer::releaseDepthTexture() {
  if (depthTexture != nullptr) {
    SDL_ReleaseGPUTexture(device.get(), depthTexture);
    depthTexture = nullptr;
    depthTextureWidth = 0;
    depthTextureHeight = 0;
  }
}

void Renderer::ensureDepthTexture(uint32_t width, uint32_t height) {
  if (width == 0 || height == 0) {
    return;
  }
  if (depthTexture != nullptr && depthTextureWidth == width &&
      depthTextureHeight == height) {
    return;
  }

  releaseDepthTexture();

  SDL_GPUTextureCreateInfo texture_create_info{};
  texture_create_info.type = SDL_GPU_TEXTURETYPE_2D;
  texture_create_info.format = getDepthFormat();
  texture_create_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  texture_create_info.width = width;
  texture_create_info.height = height;
  texture_create_info.layer_count_or_depth = 1;
  texture_create_info.num_levels = 1;
  texture_create_info.sample_count = SDL_GPU_SAMPLECOUNT_1;

  depthTexture = SDL_CreateGPUTexture(device.get(), &texture_create_info);
  if (depthTexture == nullptr) {
    throw std::runtime_error("Renderer: CreateGPUTexture depth target failed");
  }

  depthTextureWidth = width;
  depthTextureHeight = height;
}

SDL_GPURenderPass* Renderer::beginRenderPass(FrameContext& frame_context,
                                             float r, float g, float b,
                                             float a) {
  SDL_GPUColorTargetInfo color_target_info{};
  color_target_info.texture = frame_context.swapchainTexture;
  color_target_info.clear_color = {r, g, b, a};
  color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
  color_target_info.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPUDepthStencilTargetInfo depth_stencil_target_info{};
  depth_stencil_target_info.texture = depthTexture;
  depth_stencil_target_info.clear_depth = 1.0f;
  depth_stencil_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
  depth_stencil_target_info.store_op = SDL_GPU_STOREOP_DONT_CARE;
  depth_stencil_target_info.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth_stencil_target_info.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

  SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(
      frame_context.commandBuffer, &color_target_info, 1,
      &depth_stencil_target_info);
  if (!render_pass) {
    throw std::runtime_error("Renderer: BeginGPURenderPass failed");
  }
  return render_pass;
}

void Renderer::endRenderPass(SDL_GPURenderPass* render_pass) {
  SDL_EndGPURenderPass(render_pass);
}

FrameContext& Renderer::beginFrame() {
  currentFrame = (currentFrame + 1) % FRAMES_IN_FLIGHT;
  FrameContext& frame_context = frames[currentFrame];

  frame_context.commandBuffer = SDL_AcquireGPUCommandBuffer(device.get());
  if (frame_context.commandBuffer == nullptr) {
    throw std::runtime_error("Renderer: AcquireGPUCommandBuffer failed");
  }

  uint32_t width, height;
  if (!SDL_AcquireGPUSwapchainTexture(frame_context.commandBuffer, window,
                                      &frame_context.swapchainTexture, &width,
                                      &height)) {
    throw std::runtime_error("Renderer: AcquireGPUSwapchainTexture failed");
  }
  frame_context.width = width;
  frame_context.height = height;

  if (frame_context.swapchainTexture != nullptr) {
    viewportWidth = width;
    viewportHeight = height;
    ensureDepthTexture(width, height);
  }

  return frame_context;
}

void Renderer::endFrame(FrameContext& frame) {
  SDL_SubmitGPUCommandBuffer(frame.commandBuffer);
  frame.commandBuffer = nullptr;
  frame.swapchainTexture = nullptr;
}

float Renderer::getAspectRatio() const {
  if (viewportHeight == 0) {
    return 1.0f;
  }
  return static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
}
