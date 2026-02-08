#ifndef CORE_APPLICATION_HPP
#define CORE_APPLICATION_HPP

#include <SDL3/SDL.h>

class Application {
 private:
  SDL_Window* window;
  SDL_GPUDevice* gpuDevice;

 public:
  Application();
  ~Application();

  void GameLoop();

  void InitializeGPU();
  void ReleaseGPU();
};

#endif
