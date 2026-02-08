#include "application.hpp"
#include "render/basic_renderer.hpp"

#include <SDL3/SDL.h>


Application::Application() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("ERROR: Failed to initialize SDL: %s", SDL_GetError());
  } else {
    SDL_Log("SDL initialized successfully");
  }
}

Application::~Application() {}

void Application::GameLoop() {
  BasicRenderer renderer(gpuDevice, window,
                         "resources/shaders/triangle_vertex.spv",
                         "resources/shaders/triangle_fragment.spv");

  bool run = true;

  while (run) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        run = false;
        break;
      }
      renderer.Draw(window);
    }
  }
}

void Application::InitializeGPU() {
  gpuDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);

  if (gpuDevice == nullptr) {
    SDL_Log("ERROR: CreateGPUDevice failed");
    return;
  }

  window = SDL_CreateWindow("window", 1024, 768, 0);
  if (window == nullptr) {
    SDL_Log("ERROR: CreateWindow failed: %s", SDL_GetError());
    return;
  }

  if (!SDL_ClaimWindowForGPUDevice(gpuDevice, window)) {
    SDL_Log("ERROR: ClaimWindowForGPUDevice failed");
    return;
  }
}

void Application::ReleaseGPU() {
  SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
  SDL_DestroyWindow(window);
  SDL_DestroyGPUDevice(gpuDevice);
}
