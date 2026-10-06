#ifndef EDITOR_EDITOR_UI_HPP
#define EDITOR_EDITOR_UI_HPP

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "core/frame.hpp"
#include "core/text/caption_font.hpp"
#include "editor/editor_character_preview.hpp"
#include "editor/editor_playtest.hpp"
#include "editor/editor_property_edit.hpp"

class EditorDocument;
struct EditorAuditionView {
  bool active{}, muted{}, paused{};
  CaptionPresentation captions{};
  std::string_view source{}, warning{}, listener_room{}, source_room{};
  float gain{};
};
struct EditorReadablePreviewView {
  bool selected{}, available{}, stale{};
  std::size_t page{};  // Zero-based page actually represented by the preview.
  std::string title, text, layout_diagnostics;
  std::vector<std::string> validation_diagnostics;
};
enum class EditorAuditionAction { None, Start, Stop, Mute, Pause };
// Call right after a draft text input; true when its edit is complete.
[[nodiscard]] bool editorTextEditFinished();
bool drawEditorAudioProperties(EditorObjectValue& value,
                               const LevelDocument& level);

class EditorUi {
 public:
  void draw(EditorDocument& document, bool child_active = false,
            std::string_view process_status = {});
  void setReadableFont(std::shared_ptr<const CaptionFont> font) {
    readable_font_ = std::move(font);
    readable_preview_.reset();
    readable_preview_revision_.reset();
  }
  EditorAuditionAction drawAudition(const EditorAuditionView& view,
                                    bool can_start);
  std::optional<EditorCharacterPreviewRequest> drawCharacterPreview(
      const EditorDocument& document, EditorCharacterPreview& preview,
      bool can_start);
  bool takePlayAttempt() { return std::exchange(play_attempt_, false); }
  [[nodiscard]] std::optional<EditorLaunchRequest> takeLaunchRequest() {
    return playtest_.consume();
  }
  [[nodiscard]] std::optional<WorldPosition> updateViewport(
      EditorDocument& document, const CameraFrame& camera, bool navigating);
  void finishFrame();
  // Fixed-scene GPU smoke excludes changing panel text from pixel comparisons.
  void collapsePanelsForCapture(bool collapsed);
  [[nodiscard]] bool sculpting() const noexcept { return sculpting_; }
  [[nodiscard]] EditorReadablePreviewView readablePreview(
      const EditorDocument& document) const;

 private:
  void drawMenu(EditorDocument& document);
  void drawDocumentSummary(EditorDocument& document);
  void drawObjects(EditorDocument& document);
  void drawAudioObjects(EditorDocument& document);
  void drawCharacterObjects(EditorDocument& document);
  void drawCharacterProperties(EditorDocument& document);
  void drawHouseholdObjects(EditorDocument& document);
  void drawNarrativeObjects(EditorDocument& document);
  void drawNarrativeProperties(EditorDocument& document);
  void drawHouseholdProperties(EditorDocument& document);
  void drawReadablePreview(const EditorDocument& document);
  bool commitSelectionDraft(EditorDocument& document);
  void selectObject(EditorDocument& document, EditorObjectId id);
  void drawProperties(EditorDocument& document);
  void drawTerrainBrush(EditorDocument& document);
  void drawValidation(const EditorDocument& document);
  void drawPathModals(EditorDocument& document);
  void drawPendingModal(EditorDocument& document);
  void drawPlay(EditorDocument& document, bool child_active,
                std::string_view process_status);
  void openPathModal(bool save_as, const EditorDocument& document);

  std::array<char, 1024> path_buffer_{};
  bool open_path_popup_{};
  bool save_as_popup_{};
  bool placing_{};
  bool sculpting_{};
  EditorTerrainBrush brush_draft_{};
  EditorPropertyEdit property_edit_{};
  EditorPlacementMode placement_mode_{EditorPlacementMode::SceneSurfaces};
  EditorPlacementOffsets placement_offsets_{};
  std::optional<EditorSurfaceHit> placement_hit_{};
  EditorObjectId placement_object_{};
  std::uint64_t document_generation_{};
  bool save_pending_action_{};
  EditorPlaytest playtest_{};
  bool play_attempt_{};
  EditorCharacterPreviewRequest preview_request_{};
  std::uint64_t preview_generation_{}, preview_revision_{},
      preview_selection_revision_{};
  EditorObjectId preview_selection_{};
  std::shared_ptr<const CaptionFont> readable_font_;
  EditorObjectId readable_object_{};
  std::uint64_t readable_generation_{};
  std::size_t readable_page_{}, readable_preview_page_{},
      readable_last_good_page_{};
  std::optional<std::uint64_t> readable_preview_revision_;
  std::optional<CaptionLayout> readable_preview_;
  std::string readable_preview_error_;
  std::string readable_preview_title_, readable_preview_text_;
};

#endif
