#ifndef EDITOR_EDITOR_UI_HPP
#define EDITOR_EDITOR_UI_HPP

#include <array>
#include <utility>

#include "core/frame.hpp"
#include "editor/editor_playtest.hpp"
#include "editor/editor_property_edit.hpp"

class EditorDocument;
struct EditorAuditionView {
  bool active{}, muted{}, paused{};
  CaptionPresentation captions{};
  std::string_view source{}, warning{}, listener_room{}, source_room{};
  float gain{};
};
enum class EditorAuditionAction { None, Start, Stop, Mute, Pause };
bool drawEditorAudioProperties(EditorObjectValue& value,
                               const LevelDocument& level);

class EditorUi {
 public:
  void draw(EditorDocument& document, bool child_active = false,
            std::string_view process_status = {});
  EditorAuditionAction drawAudition(const EditorAuditionView& view,
                                    bool can_start);
  bool takePlayAttempt() { return std::exchange(play_attempt_, false); }
  [[nodiscard]] std::optional<EditorLaunchRequest> takeLaunchRequest() {
    return playtest_.consume();
  }
  [[nodiscard]] std::optional<WorldPosition> updateViewport(
      EditorDocument& document, const CameraFrame& camera, bool navigating);
  void finishFrame();
  [[nodiscard]] bool sculpting() const noexcept { return sculpting_; }

 private:
  void drawMenu(EditorDocument& document);
  void drawDocumentSummary(const EditorDocument& document);
  void drawObjects(EditorDocument& document);
  void drawAudioObjects(EditorDocument& document);
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
};

#endif
