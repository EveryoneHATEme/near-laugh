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

Renderer::~Renderer() { SDL_ReleaseWindowFromGPUDevice(device.get(), window); }

SDL_GPURenderPass* Renderer::beginRenderPass(FrameContext& frame_context,
                                             float r, float g, float b,
                                             float a) {
  SDL_GPUColorTargetInfo color_target_info{};
  color_target_info.texture = frame_context.swapchainTexture;
  color_target_info.clear_color = {r, g, b, a};
  color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
  color_target_info.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(
      frame_context.commandBuffer, &color_target_info, 1, nullptr);
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
    throw std::runtime_error("Renderer: AcquireGPUSwapcahinTexture failed");
  }

  return frame_context;
}

void Renderer::endFrame(FrameContext& frame) {
  SDL_SubmitGPUCommandBuffer(frame.commandBuffer);
  frame.commandBuffer = nullptr;
  frame.swapchainTexture = nullptr;
}
