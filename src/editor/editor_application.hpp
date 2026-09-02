#ifndef EDITOR_EDITOR_APPLICATION_HPP
#define EDITOR_EDITOR_APPLICATION_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>

#include "core/platform/platform.hpp"
#include "core/platform/window.hpp"
#include "core/render/validation_diagnostics.hpp"
#include "editor/editor_camera.hpp"
#include "editor/editor_document.hpp"
#include "editor/editor_glfw_bridge.hpp"
#include "editor/editor_renderer.hpp"
#include "editor/editor_ui.hpp"

class EditorApplication {
 public:
  EditorApplication(std::filesystem::path resource_root,
                    std::optional<std::filesystem::path> initial_level);

  EditorApplication(const EditorApplication&) = delete;
  EditorApplication& operator=(const EditorApplication&) = delete;
  EditorApplication(EditorApplication&&) = delete;
  EditorApplication& operator=(EditorApplication&&) = delete;

  void run();
  void runSmoke(const std::filesystem::path& valid_level);
  [[nodiscard]] bool tick();
  [[nodiscard]] std::size_t validationErrorCount() const noexcept;

 private:
  void updateNavigation(EditorUiCaptureIntent capture);
  void synchronizeDocumentResources();

  ValidationDiagnostics validation_diagnostics_{};
  Platform platform_{};
  Window window_;
  EditorGlfwBridge glfw_imgui_bridge_;
  EditorDocument document_{};
  EditorRenderer renderer_;
  EditorUi ui_{};
  EditorCamera camera_{};
  EditorFrameClock frame_clock_{};
  std::uint64_t rendered_document_revision_{};
  bool scene_resources_installed_{};
};

#endif
