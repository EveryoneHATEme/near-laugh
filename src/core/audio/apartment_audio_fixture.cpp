#include "core/audio/apartment_audio_fixture.hpp"

#include <algorithm>
#include <stdexcept>

#include "core/world/audio.hpp"

ApartmentAudioFixture::ApartmentAudioFixture(CueCoordinator& cues)
    : cues_(cues) {
  for (const auto id : {"radio", "phone-ring", "footsteps",
                        "phone-conversation", "invitation"}) {
    const auto& sources = cues_.definitions().sources;
    const auto source =
        std::find_if(sources.begin(), sources.end(),
                     [&](const auto& value) { return value.id == id; });
    if (source == sources.end())
      throw std::runtime_error("Audio fixture requires source '" +
                               std::string(id) + "'");
    const auto* cue = findAudioCue(cues_.definitions(), source->cue);
    if (cue->clip != id || !cue->caption || *cue->caption != id ||
        cue->loop != (id == std::string_view("radio")) ||
        (cue->kind == AudioCueKind::Ambience) !=
            (id == std::string_view("radio")))
      throw std::runtime_error("Audio fixture source '" + std::string(id) +
                               "' has an incompatible cue");
    if (id == std::string_view("footsteps")) steps_origin_ = source->position;
  }
}
void ApartmentAudioFixture::start(std::string_view id) {
  if (cues_.start(id) != CueStart::Started)
    throw std::runtime_error("Audio fixture could not explicitly start '" +
                             std::string(id) + "'");
}
void ApartmentAudioFixture::restart() {
  cues_.cancelAll();
  cues_.moveSource("footsteps", steps_origin_);
  start("radio");
  start("phone-ring");
  stage_ = Stage::Ring;
}
void ApartmentAudioFixture::update() {
  if (cues_.suspended()) return;
  switch (stage_) {
    case Stage::Ring:
      if (cues_.status("phone-ring") == CueStatus::Completed) {
        start("footsteps");
        stage_ = Stage::Steps;
      }
      break;
    case Stage::Steps: {
      auto position = steps_origin_;
      position.z +=
          static_cast<float>(std::min(cues_.offset("footsteps"), 6.0)) * .8F;
      cues_.moveSource("footsteps", position);
      if (cues_.status("footsteps") == CueStatus::Completed) {
        start("phone-conversation");
        stage_ = Stage::Phone;
      }
      break;
    }
    case Stage::Phone:
      if (cues_.status("phone-conversation") == CueStatus::Completed) {
        pause_start_ = cues_.activeTime();
        stage_ = Stage::Pause;
      }
      break;
    case Stage::Pause:
      if (cues_.activeTime() - pause_start_ >= 2) {
        start("invitation");
        stage_ = Stage::Invitation;
      }
      break;
    case Stage::Invitation:
      if (cues_.status("invitation") == CueStatus::Completed)
        stage_ = Stage::Done;
      break;
    case Stage::Done:
      break;
  }
}
void ApartmentAudioFixture::controls(bool restart_key, bool mute_key,
                                     bool pause_key, bool active, double now) {
  if (active) {
    if (pause_key && !pause_down_) {
      paused_ = !paused_;
      cues_.suspend(paused_, now);
    }
    if (mute_key && !mute_down_) cues_.mute(!cues_.muted());
    if (restart_key && !restart_down_) restart();
  }
  restart_down_ = restart_key;
  mute_down_ = mute_key;
  pause_down_ = pause_key;
}
