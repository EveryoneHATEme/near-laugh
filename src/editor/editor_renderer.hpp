#ifndef EDITOR_EDITOR_RENDERER_HPP
#define EDITOR_EDITOR_RENDERER_HPP

#include <array>
#include <filesystem>
#include <memory>
#include <span>

#include "core/frame.hpp"
#include "core/render/character_presentation.hpp"

struct LevelDocument;
struct EditorOverlayLine;
class ValidationDiagnostics;
class Window;
struct FrameCapture;

struct EditorRendererResources {
  std::filesystem::path vertex_shader{};
  std::filesystem::path fragment_shader{};
  std::filesystem::path resource_root{};
  FrameCapture* capture{};
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
  void replaceDocument(const LevelDocument& level);
  // A candidate owns the full static/animated scene until successful install.
  // Failure retains the previous scene and compatible pose selection.
  void replaceDocument(const LevelDocument& level,
                       std::span<const CharacterRenderInstance> characters);
  void replaceTerrain(const LevelDocument& level);
  void validateSceneAssets(const LevelDocument& level) const;
  [[nodiscard]] std::size_t terrainReplacementCount() const noexcept;
  void drawOverlays(std::span<const EditorOverlayLine> lines);
  void clearDocument();
  [[nodiscard]] FrameOutcome renderFrame(const FrameRequest& request);
  void requestSwapchainRecreation() noexcept;
  [[nodiscard]] bool validationEnabled() const noexcept;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
