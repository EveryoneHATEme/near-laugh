#ifndef CORE_RENDER_GRAPHICS_PIPELINE_H
#define CORE_RENDER_GRAPHICS_PIPELINE_H

#include <SDL3/SDL.h>

#include <filesystem>
#include <vector>

enum class ShaderType { VERTEX_SHADER, FRAGMENT_SHADER };

class GraphicsPipeline {
 private:
  SDL_GPUDevice* device;
  SDL_GPUGraphicsPipeline* graphicsPipeline;
  SDL_GPUBuffer* vertexBuffer;

 private:
  SDL_GPUShader* createShader(const std::vector<uint8_t>& shader_source,
                              ShaderType shader_type) const;

 public:
  // should be extended in future (no fragment shader, with compute shader, with
  // geometry shader)
  GraphicsPipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat swapchain_format,
                   const std::filesystem::path& vertex_shader_path,
                   const std::filesystem::path& fragment_shader_path);
  ~GraphicsPipeline();

  void draw(SDL_GPURenderPass* render_pass) const;
};

#endif
