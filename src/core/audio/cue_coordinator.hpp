#ifndef CORE_AUDIO_CUE_COORDINATOR_HPP
#define CORE_AUDIO_CUE_COORDINATOR_HPP

#include "core/audio/audio_playback.hpp"
#include "core/text/presentation.hpp"

enum class CueStart { Started, AlreadyActive, Busy };
enum class CueStatus { Idle, Playing, Completed, Cancelled };

class CueCoordinator final {
 public:
  CueCoordinator(LevelAudio definitions, std::vector<DoorDefinition> doors,
                 AudioContent content, AudioOutput output, double now);
  [[nodiscard]] CueStart start(std::string_view source);
  bool cancel(std::string_view source);
  void cancelAll();
  void autoplay();
  void update(double now);
  void suspend(bool suspended, double now);
  void mute(bool muted);
  void listener(WorldPosition position, WorldPosition direction);
  void moveSource(std::string_view source, WorldPosition position);
  void acceptedDoors(std::span<const float> angles);
  [[nodiscard]] CueStatus status(std::string_view source) const;
  [[nodiscard]] double offset(std::string_view source) const;
  [[nodiscard]] std::uint64_t instance(std::string_view source) const;
  [[nodiscard]] float effectiveGain(std::string_view source) const;
  [[nodiscard]] CaptionPresentation captions() const;
  [[nodiscard]] double activeTime() const noexcept { return active_time_; }
  [[nodiscard]] bool suspended() const noexcept { return suspended_; }
  [[nodiscard]] bool muted() const noexcept { return muted_; }
  [[nodiscard]] AudioPlayback& playback() noexcept { return playback_; }
  [[nodiscard]] const LevelAudio& definitions() const noexcept {
    return definitions_;
  }

 private:
  struct Instance {
    CueStatus status{CueStatus::Idle};
    double start{};
    std::uint64_t serial{};
    WorldPosition position{};
    float effective_gain{};
    unsigned synchronization_failures{};
    std::optional<double> correction_time{};
  };
  [[nodiscard]] std::size_t index(std::string_view source) const;
  [[nodiscard]] const AudioCueDefinition& cue(std::size_t source) const;
  void updateGains();
  void synchronize(std::size_t source, bool force);

  LevelAudio definitions_;
  std::vector<DoorDefinition> doors_;
  AudioPlayback playback_;
  std::vector<Instance> instances_;
  std::vector<float> accepted_angles_;
  std::optional<std::size_t> foreground_{};
  WorldPosition listener_{};
  double last_time_{}, active_time_{};
  std::uint64_t next_serial_{};
  bool suspended_{}, muted_{};
};

#endif
