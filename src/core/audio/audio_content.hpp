#ifndef CORE_AUDIO_AUDIO_CONTENT_HPP
#define CORE_AUDIO_AUDIO_CONTENT_HPP

#include <span>

#include "core/text/utf8.hpp"
#include "core/world/level_document.hpp"

struct AudioClip {
  std::string id;
  unsigned channels{};
  std::vector<float> samples;
  [[nodiscard]] double duration() const noexcept {
    return static_cast<double>(samples.size()) / channels / 48000;
  }
};
struct CaptionSegment {
  double start{};
  double end{};
  std::string label;
  std::string text;
};
struct CaptionTrack {
  std::string id;
  std::vector<CaptionSegment> segments;
};
struct AudioContent {
  std::vector<AudioClip> clips;
  std::vector<CaptionTrack> captions;
  [[nodiscard]] const AudioClip& clip(std::string_view id) const;
  [[nodiscard]] const CaptionTrack* caption(std::string_view id) const noexcept;
};

// No devices, windows or GPU objects are constructed by content preflight.
[[nodiscard]] AudioContent prepareAudioContent(
    const std::filesystem::path& root, const LevelAudio& audio);
class CaptionFont;
void validateAudioCaptions(const AudioContent& content, const LevelAudio& audio,
                           const CaptionFont& font);

// Extra selected-content constraints for the concrete actor marker links.
void validateCharacterAudio(const AudioContent& content,
                            const LevelAudio& audio,
                            const LevelCharacters& characters);

#endif
