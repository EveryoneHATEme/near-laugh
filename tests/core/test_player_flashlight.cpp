#include <gtest/gtest.h>

#include "core/gameplay/player_flashlight.hpp"

namespace {
PlayerViewPose forwardView() {
  return {{1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, -1.0F}};
}
}  // namespace

TEST(PlayerFlashlight, StartsOffAndTogglesOnlyOnArmedActivePresses) {
  PlayerFlashlight flashlight;
  EXPECT_FALSE(flashlight.enabled());
  EXPECT_TRUE(flashlight.armed());
  EXPECT_TRUE(spotLightFrameIsValid(flashlight.spotLight(forwardView())));
  EXPECT_FLOAT_EQ(
      flashlight.spotLight(forwardView()).outer_cosine_and_enabled[1], 0.0F);

  flashlight.samplePrimaryAction(true, true);
  EXPECT_TRUE(flashlight.enabled());
  EXPECT_FALSE(flashlight.armed());
  EXPECT_TRUE(spotLightFrameIsValid(flashlight.spotLight(forwardView())));
  flashlight.samplePrimaryAction(true, true);
  EXPECT_TRUE(flashlight.enabled());

  flashlight.samplePrimaryAction(false, true);
  EXPECT_TRUE(flashlight.armed());
  flashlight.samplePrimaryAction(true, true);
  EXPECT_FALSE(flashlight.enabled());
}

TEST(PlayerFlashlight, InactiveRecapturePressIsSuppressedUntilRelease) {
  PlayerFlashlight flashlight;
  flashlight.samplePrimaryAction(true, false);
  EXPECT_FALSE(flashlight.enabled());
  EXPECT_FALSE(flashlight.armed());

  flashlight.samplePrimaryAction(true, true);
  EXPECT_FALSE(flashlight.enabled());
  flashlight.samplePrimaryAction(false, true);
  EXPECT_TRUE(flashlight.armed());
  flashlight.samplePrimaryAction(true, true);
  EXPECT_TRUE(flashlight.enabled());
}

TEST(PlayerFlashlight, ProducesGenericSpotLightFromSuppliedViewPose) {
  PlayerFlashlight flashlight;
  flashlight.samplePrimaryAction(true, true);
  const PlayerViewPose view{{4.0F, 5.0F, 6.0F}, {1.0F, 0.0F, 0.0F}};
  const SpotLightFrame light = flashlight.spotLight(view);
  EXPECT_TRUE(spotLightFrameIsValid(light));
  EXPECT_EQ(light.position_and_range[0], view.position.x);
  EXPECT_EQ(light.position_and_range[1], view.position.y);
  EXPECT_EQ(light.position_and_range[2], view.position.z);
  EXPECT_EQ(light.direction_and_inner_cosine[0], view.direction.x);
  EXPECT_EQ(light.direction_and_inner_cosine[1], view.direction.y);
  EXPECT_EQ(light.direction_and_inner_cosine[2], view.direction.z);
  EXPECT_GT(light.direction_and_inner_cosine[3],
            light.outer_cosine_and_enabled[0]);
}
