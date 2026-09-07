#ifndef CORE_AUDIO_AUDIO_PLAYBACK_HPP
#define CORE_AUDIO_AUDIO_PLAYBACK_HPP

#include <memory>
#include <span>
#include <string_view>

#include "core/audio/audio_content.hpp"

// Device-free rendering uses exactly the same engine as desktop playback.
enum class AudioOutput { Device, Offline, Silent };

class AudioPlayback final {
 public:
  explicit AudioPlayback(AudioOutput output = AudioOutput::Device);
  AudioPlayback(AudioContent content, AudioOutput output);
  ~AudioPlayback();
  AudioPlayback(const AudioPlayback&) = delete;
  AudioPlayback& operator=(const AudioPlayback&) = delete;
  AudioPlayback(AudioPlayback&&) = delete;
  AudioPlayback& operator=(AudioPlayback&&) = delete;

  [[nodiscard]] bool silent() const noexcept;
  [[nodiscard]] std::string_view warning() const noexcept;
  // Interleaved stereo float PCM, 48000 frames per second. Offline only.
  void render(std::span<float> stereo_samples);
  [[nodiscard]] const AudioContent& content() const noexcept;
  void start(std::size_t slot, const AudioCueDefinition& cue,
             const AudioSourceDefinition& source);
  void stop(std::size_t slot);
  void source(std::size_t slot, WorldPosition position, float gain);
  void listener(WorldPosition position, WorldPosition direction);
  void mute(bool muted);
  void suspend(bool suspended);
  [[nodiscard]] bool seek(std::size_t slot, double seconds);
  [[nodiscard]] double cursor(std::size_t slot) const;
  [[nodiscard]] std::size_t voiceCount() const noexcept;
  void pollDevice();
  void silentFallback(std::string reason);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
