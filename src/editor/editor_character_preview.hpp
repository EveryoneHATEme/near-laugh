#ifndef EDITOR_CHARACTER_PREVIEW_HPP
#define EDITOR_CHARACTER_PREVIEW_HPP

#include "core/animation/character_animation.hpp"
#include "editor/editor_document.hpp"

enum class EditorCharacterPreviewMode { Clip, Route };
enum class EditorRoutePreviewStage {
  Heading,
  Travel,
  Facing,
  Interaction,
  Completed
};

struct EditorCharacterPreviewRequest {
  EditorCharacterPreviewMode mode{EditorCharacterPreviewMode::Clip};
  EditorObjectId actor{};
  // Zero uses the actor's initial mark; otherwise an explicitly chosen mark.
  EditorObjectId mark{};
  std::string clip{"idle"};
};

// One silent, immutable-definition inspection session. No game or audio owner.
class EditorCharacterPreview {
 public:
  [[nodiscard]] static std::string startError(
      const EditorDocument& document,
      const EditorCharacterPreviewRequest& request);
  bool start(const EditorDocument& document,
             const EditorCharacterPreviewRequest& request,
             std::shared_ptr<const CharacterAsset> asset);
  void synchronize(const EditorDocument& document);
  void stop();
  void advance(double seconds);
  void pause();
  void restart();
  void seek(double seconds);
  void selectClip(std::string_view clip);
  [[nodiscard]] bool active() const { return playback_.has_value(); }
  [[nodiscard]] bool paused() const { return playback_ && playback_->paused(); }
  [[nodiscard]] double time() const {
    return playback_ ? playback_->time() : 0;
  }
  [[nodiscard]] double duration() const;
  [[nodiscard]] std::string_view clip() const {
    return playback_ ? playback_->clip() : "";
  }
  [[nodiscard]] const CharacterPose& pose() const { return palette_; }
  [[nodiscard]] const CharacterPlacement& placement() const {
    return placement_;
  }
  [[nodiscard]] EditorObjectId actor() const { return actor_; }
  [[nodiscard]] EditorCharacterPreviewMode mode() const { return mode_; }
  [[nodiscard]] EditorRoutePreviewStage stage() const { return stage_; }
  [[nodiscard]] std::size_t segment() const { return segment_; }
  [[nodiscard]] std::string_view targetMark() const;
  [[nodiscard]] std::string_view finalClip() const {
    return final_clip_ ? std::string_view(*final_clip_) : "None";
  }
  [[nodiscard]] std::string_view error() const { return error_; }

 private:
  void advanceRoute(double seconds);
  void refresh();
  std::shared_ptr<const CharacterAsset> asset_;
  std::optional<CharacterPlayback> playback_;
  CharacterPose palette_;
  CharacterPlacement placement_{}, initial_{};
  std::vector<CharacterMarkDefinition> marks_;
  std::optional<std::string> final_clip_;
  std::string error_;
  EditorObjectId actor_{};
  std::uint64_t generation_{}, revision_{}, selection_revision_{};
  EditorObjectId selection_{};
  EditorCharacterPreviewMode mode_{};
  EditorRoutePreviewStage stage_{EditorRoutePreviewStage::Completed};
  std::size_t segment_{};
  double speed_{}, walked_distance_{}, walk_cycle_distance_{};
};

#endif
