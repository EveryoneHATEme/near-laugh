#ifndef CORE_RENDER_GRAPHICS_PIPELINE_H
#define CORE_RENDER_GRAPHICS_PIPELINE_H

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "render_types.hpp"

enum class ShaderType { VERTEX_SHADER, FRAGMENT_SHADER };

class GraphicsPipeline {
 private:
  SDL_GPUDevice* device;
  SDL_GPUGraphicsPipeline* graphicsPipeline{nullptr};
  SDL_GPUBuffer* vertexBuffer{nullptr};
  SDL_GPUBuffer* indexBuffer{nullptr};
  std::size_t vertexCapacity{};
  std::size_t indexCapacity{};
  uint32_t indexCount{};

 private:
  SDL_GPUShader* createShader(const std::vector<uint8_t>& shader_source,
                              ShaderType shader_type) const;
  void ensureBuffers(std::size_t vertex_count, std::size_t index_count);
  void releaseBuffers();

 public:
  // should be extended in future (no fragment shader, with compute shader, with
  // geometry shader)
  GraphicsPipeline(SDL_GPUDevice* device, SDL_GPUTextureFormat swapchain_format,
                   SDL_GPUTextureFormat depth_format,
                   const std::filesystem::path& vertex_shader_path,
                   const std::filesystem::path& fragment_shader_path);
  ~GraphicsPipeline();

  void upload(const RenderPacket& packet);
  void draw(SDL_GPURenderPass* render_pass) const;
};

#endif
