#ifndef EDITOR_AUDIO_AUDITION_HPP
#define EDITOR_AUDIO_AUDITION_HPP
#include "core/audio/cue_coordinator.hpp"
#include "core/text/caption_font.hpp"
#include "editor/editor_document.hpp"

class EditorAudioAudition {
 public:
  bool start(const EditorDocument& document, EditorObjectId source,
             const std::filesystem::path& root, const CaptionFont& font,
             double now, AudioOutput output = AudioOutput::Device);
  void update(const EditorDocument& document, WorldPosition listener,
              WorldPosition direction, double now);
  void stop();
  void mute();
  void pause(double now);
  [[nodiscard]] CueCoordinator* cues() const noexcept { return cues_.get(); }
  [[nodiscard]] std::string_view source() const noexcept { return source_; }
  [[nodiscard]] std::string_view error() const noexcept { return error_; }

 private:
  std::unique_ptr<CueCoordinator> cues_;
  std::string source_, error_;
  std::uint64_t generation_{}, revision_{};
};
#endif
