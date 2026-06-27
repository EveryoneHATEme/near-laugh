#ifndef CORE_RENDER_RENDERER_H
#define CORE_RENDER_RENDERER_H

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

constexpr std::size_t FRAMES_IN_FLIGHT = 4;

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
  uint32_t width = 0;
  uint32_t height = 0;
};

class Renderer {
 private:
  SDL_Window* window{nullptr};
  GPUDevicePtr device{nullptr};
  SDL_GPUTexture* depthTexture{nullptr};
  uint32_t depthTextureWidth{0};
  uint32_t depthTextureHeight{0};
  uint32_t viewportWidth{1024};
  uint32_t viewportHeight{768};

  std::size_t currentFrame{};
  std::vector<FrameContext> frames{};

  void ensureDepthTexture(uint32_t width, uint32_t height);
  void releaseDepthTexture();

 public:
  Renderer(SDL_Window* window);
  ~Renderer();

  SDL_GPURenderPass* beginRenderPass(FrameContext& frame, float r, float g,
                                     float b, float a);
  void endRenderPass(SDL_GPURenderPass* render_pass);

  FrameContext& beginFrame();
  void endFrame(FrameContext& frame);

  float getAspectRatio() const;
  SDL_GPUTextureFormat getDepthFormat() const {
    return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
  }
  SDL_GPUTextureFormat getSwapchainFormat() const {
    return SDL_GetGPUSwapchainTextureFormat(device.get(), window);
  }
  SDL_GPUDevice* getDevice() const {
    return device.get();
  }
};

#endif
