#ifndef CORE_RENDER_BASIC_RENDERER_H
#define CORE_RENDER_BASIC_RENDERER_H

#include <SDL3/SDL.h>

#include <string>

class BasicRenderer {
 private:
  SDL_GPUGraphicsPipeline* pipeline;
  SDL_GPUDevice* gpuDevice;

 private:
  SDL_GPUShader* CreateShader(SDL_GPUShaderStage shader_stage,
                              const std::string& source_path);

 public:
  BasicRenderer(SDL_GPUDevice* gpu_device, SDL_Window* window,
                const std::string& vertex_shader_source_path,
                const std::string& fragment_shader_source_path);
  ~BasicRenderer();

  void Draw(SDL_Window* window);
};

#endif
