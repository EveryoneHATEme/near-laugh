#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "core/audio/apartment_audio_fixture.hpp"
#include "core/text/caption_font.hpp"
#include "core/world/prototype_level.hpp"

TEST(AudioFixture,
     ActualContentKeepsFullCallBeforeInvitationInAudibleMutedAndSilentRuns) {
  const auto level =
      loadPrototypeLevel("resources/levels/audio-captions.level.json");
  const CaptionFont font("resources");
  std::vector<std::string> reference;
  for (int mode = 0; mode < 3; ++mode) {
    auto content = prepareAudioContent("resources", level.audio());
    validateAudioCaptions(content, level.audio(), font);
    CueCoordinator cues(level.audio(), level.doors(), std::move(content),
                        mode == 2 ? AudioOutput::Silent : AudioOutput::Offline,
                        0);
    cues.mute(mode == 1);
    cues.listener({-3, 4.5F, 3.7F}, {1, 0, 0});
    ApartmentAudioFixture fixture(cues);
    fixture.restart();
    std::vector<std::string> captions;
    double energy = 0, call_finished = -1, invitation_started = -1;
    std::array<float, 4800> pcm{};
    for (int tick = 1; tick <= 800; ++tick) {
      cues.playback().render(pcm);
      for (float sample : pcm) energy += std::abs(sample);
      cues.update(tick * .05);
      fixture.update();
      const auto text = cues.captions().foreground.text;
      if (!text.empty() && (captions.empty() || captions.back() != text))
        captions.emplace_back(text);
      unsigned foreground = 0;
      for (const auto id :
           {"phone-ring", "footsteps", "phone-conversation", "invitation"})
        foreground += cues.status(id) == CueStatus::Playing;
      EXPECT_LE(foreground, 1U);
      if (call_finished < 0 &&
          cues.status("phone-conversation") == CueStatus::Completed)
        call_finished = cues.activeTime();
      if (invitation_started < 0 &&
          cues.status("invitation") == CueStatus::Playing)
        invitation_started = cues.activeTime();
    }
    EXPECT_TRUE(fixture.finished());
    EXPECT_GE(invitation_started - call_finished, 2);
    EXPECT_FALSE(cues.playback().silent() && mode != 2);
    if (mode == 0) {
      EXPECT_GT(energy, 1);
      reference = captions;
    } else {
      EXPECT_EQ(energy, 0);
      EXPECT_EQ(captions, reference);
    }
    ASSERT_EQ(captions.size(), 7U);
    EXPECT_EQ(captions[5], "Разговор окончен.");
    EXPECT_EQ(captions.back(),
              "Это я. Я уже дома. Открой дверь, пойдём на кухню.");
    const auto serial = cues.instance("phone-ring");
    fixture.restart();
    EXPECT_GT(cues.instance("phone-ring"), serial);
    EXPECT_EQ(cues.offset("phone-ring"), 0);
    EXPECT_FALSE(fixture.finished());
  }
}

TEST(AudioFixture,
     ControlsConsumeInactiveHoldsAndPreserveOffsetsAcrossLongSuspension) {
  const auto level =
      loadPrototypeLevel("resources/levels/audio-captions.level.json");
  CueCoordinator cues(level.audio(), level.doors(),
                      prepareAudioContent("resources", level.audio()),
                      AudioOutput::Silent, 0);
  ApartmentAudioFixture fixture(cues);
  fixture.restart();
  const auto instance = cues.instance("phone-ring");
  fixture.controls(true, true, true, false, 0);
  fixture.controls(true, true, true, true, 1);
  EXPECT_EQ(cues.instance("phone-ring"), instance);
  EXPECT_FALSE(cues.muted());
  EXPECT_FALSE(cues.suspended());
  fixture.controls(false, false, false, true, 1);
  cues.update(2);
  fixture.controls(false, false, true, true, 2);
  cues.update(1002);
  fixture.update();
  EXPECT_EQ(cues.offset("phone-ring"), 2);
  fixture.controls(false, false, false, true, 1002);
  fixture.controls(false, false, true, true, 2002);
  cues.update(2003);
  fixture.update();
  EXPECT_EQ(cues.offset("phone-ring"), 3);
  EXPECT_EQ(cues.instance("phone-ring"), instance);
  cues.cancelAll();
  EXPECT_TRUE(cues.captions().foreground.text.empty());
  cues.suspend(true, 2003);
  cues.suspend(false, 3003);
  fixture.update();
  EXPECT_EQ(cues.status("phone-ring"), CueStatus::Cancelled);
}

TEST(AudioFixture, RequiredSourcesAndAcceptedDoorPosesAreObservable) {
  const auto level =
      loadPrototypeLevel("resources/levels/audio-captions.level.json");
  CueCoordinator cues(level.audio(), level.doors(),
                      prepareAudioContent("resources", level.audio()),
                      AudioOutput::Silent, 0);
  cues.listener({-3, 4.5F, 3.7F}, {1, 0, 0});
  ASSERT_EQ(cues.start("invitation"), CueStart::Started);
  const float closed = cues.effectiveGain("invitation");
  cues.acceptedDoors(std::array{-45.0F});
  const float blocked = cues.effectiveGain("invitation");
  cues.acceptedDoors(std::array{-90.0F});
  const float open = cues.effectiveGain("invitation");
  EXPECT_GT(blocked, closed);
  EXPECT_GT(open, blocked);
  EXPECT_NEAR(closed / open, .2F, 1e-5F);
  EXPECT_NEAR(blocked / open, .6F, 1e-5F);
  auto missing = level.audio();
  missing.sources.pop_back();
  CueCoordinator invalid(missing, level.doors(),
                         prepareAudioContent("resources", missing),
                         AudioOutput::Silent, 0);
  EXPECT_THROW(static_cast<void>(ApartmentAudioFixture(invalid)),
               std::runtime_error);
}
