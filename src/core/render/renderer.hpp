#ifndef CORE_RENDER_RENDERER_H
#define CORE_RENDER_RENDERER_H

#include <filesystem>
#include <memory>

#include "core/frame.hpp"

class Window;
class ValidationDiagnostics;
class PrototypeLevel;

struct RendererResources {
  std::filesystem::path vertex_shader{};
  std::filesystem::path fragment_shader{};
};

class Renderer {
 public:
  Renderer(const Window& window, FramebufferExtent initial_extent,
           const PrototypeLevel& level, RendererResources resources,
           ValidationDiagnostics& diagnostics);
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&) = delete;
  Renderer& operator=(Renderer&&) = delete;

  [[nodiscard]] FrameOutcome renderFrame(const FrameRequest& request);
  void requestSwapchainRecreation() noexcept;
  [[nodiscard]] bool validationEnabled() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
