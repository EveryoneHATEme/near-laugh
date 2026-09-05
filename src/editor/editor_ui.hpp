#ifndef EDITOR_EDITOR_UI_HPP
#define EDITOR_EDITOR_UI_HPP

#include <array>

#include "core/frame.hpp"
#include "editor/editor_property_edit.hpp"

class EditorDocument;

class EditorUi {
 public:
  void draw(EditorDocument& document);
  [[nodiscard]] std::optional<WorldPosition> updateViewport(
      EditorDocument& document, const CameraFrame& camera, bool navigating);
  void finishFrame();
  [[nodiscard]] bool sculpting() const noexcept { return sculpting_; }

 private:
  void drawMenu(EditorDocument& document);
  void drawDocumentSummary(const EditorDocument& document);
  void drawObjects(EditorDocument& document);
  void drawProperties(EditorDocument& document);
  void drawTerrainBrush(EditorDocument& document);
  void drawValidation(const EditorDocument& document);
  void drawPathModals(EditorDocument& document);
  void drawPendingModal(EditorDocument& document);
  void openPathModal(bool save_as, const EditorDocument& document);

  std::array<char, 1024> path_buffer_{};
  bool open_path_popup_{};
  bool save_as_popup_{};
  bool placing_{};
  bool sculpting_{};
  EditorTerrainBrush brush_draft_{};
  EditorPropertyEdit property_edit_{};
};

#endif
