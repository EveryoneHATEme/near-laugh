#ifndef CORE_RENDER_RENDERER_H
#define CORE_RENDER_RENDERER_H

#include <SDL3/SDL.h>

#include <memory>
#include <vector>

const size_t FRAMES_IN_FLIGHT = 4;

struct GPUDeviceDeleter {
  void operator()(SDL_GPUDevice* device) const noexcept {
    if (device != nullptr) {
      SDL_DestroyGPUDevice(device);
    }
  }
};
using GPUDevicePtr = std::unique_ptr<SDL_GPUDevice, GPUDeviceDeleter>;

struct FrameContext {
  SDL_GPUCommandBuffer* commandBuffer = nullptr;
  SDL_GPUTexture* swapchainTexture = nullptr;
};

class Renderer {
 private:
  SDL_Window* window{nullptr};
  GPUDevicePtr device{nullptr};

  size_t currentFrame{};
  std::vector<FrameContext> frames{};

 public:
  Renderer(SDL_Window* window);
  ~Renderer();

  SDL_GPURenderPass* beginRenderPass(FrameContext& frame, float r, float g,
                                     float b, float a);
  void endRenderPass(SDL_GPURenderPass* render_pass);

  FrameContext& beginFrame();
  void endFrame(FrameContext& frame);

  SDL_GPUTextureFormat getSwapchainFormat() const {
    return SDL_GetGPUSwapchainTextureFormat(device.get(), window);
  }
  SDL_GPUDevice* getDevice() const {
    return device.get();
  }
};

#endif
