#ifndef EDITOR_EDITOR_UI_HPP
#define EDITOR_EDITOR_UI_HPP

#include <array>

class EditorDocument;

class EditorUi {
 public:
  void draw(EditorDocument& document);
  void finishFrame();

 private:
  void drawMenu(EditorDocument& document);
  void drawDocumentSummary(const EditorDocument& document);
  void drawProperties(const EditorDocument& document);
  void drawValidation(const EditorDocument& document);
  void drawPathModals(EditorDocument& document);
  void drawPendingModal(EditorDocument& document);
  void openPathModal(bool save_as, const EditorDocument& document);

  std::array<char, 1024> path_buffer_{};
  bool open_path_popup_{};
  bool save_as_popup_{};
};

#endif
