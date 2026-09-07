#ifndef CORE_AUDIO_APARTMENT_AUDIO_FIXTURE_HPP
#define CORE_AUDIO_APARTMENT_AUDIO_FIXTURE_HPP

#include "core/audio/cue_coordinator.hpp"

// A compiled P04 acceptance sequence, selected only by the internal demo entry.
class ApartmentAudioFixture {
 public:
  explicit ApartmentAudioFixture(CueCoordinator& cues);
  void restart();
  void update();
  void controls(bool restart, bool mute, bool pause, bool active, double now);
  [[nodiscard]] bool paused() const noexcept { return paused_; }
  [[nodiscard]] bool finished() const noexcept { return stage_ == Stage::Done; }

 private:
  enum class Stage { Ring, Steps, Phone, Pause, Invitation, Done };
  void start(std::string_view id);
  CueCoordinator& cues_;
  Stage stage_{Stage::Done};
  WorldPosition steps_origin_{};
  double pause_start_{};
  bool restart_down_{}, mute_down_{}, pause_down_{}, paused_{};
};

#endif
