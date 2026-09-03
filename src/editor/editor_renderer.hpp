#ifndef EDITOR_EDITOR_RENDERER_HPP
#define EDITOR_EDITOR_RENDERER_HPP

#include <array>
#include <filesystem>
#include <memory>

#include "core/frame.hpp"

class PrototypeLevel;
class ValidationDiagnostics;
class Window;

struct EditorRendererResources {
  std::filesystem::path vertex_shader{};
  std::filesystem::path fragment_shader{};
  std::array<std::filesystem::path, 3> surface_textures{};
  std::filesystem::path prototype_chair_model{};
};

class EditorRenderer {
 public:
  EditorRenderer(const Window& window, FramebufferExtent initial_extent,
                 EditorRendererResources resources,
                 ValidationDiagnostics& diagnostics);
  ~EditorRenderer();

  EditorRenderer(const EditorRenderer&) = delete;
  EditorRenderer& operator=(const EditorRenderer&) = delete;
  EditorRenderer(EditorRenderer&&) = delete;
  EditorRenderer& operator=(EditorRenderer&&) = delete;

  void beginUiFrame();
  void replaceDocument(const PrototypeLevel& level);
  void clearDocument();
  [[nodiscard]] FrameOutcome renderFrame(const FrameRequest& request);
  void requestSwapchainRecreation() noexcept;
  [[nodiscard]] bool validationEnabled() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
