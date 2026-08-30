#include <gtest/gtest.h>

#include "core/gameplay/prototype_rifle.hpp"

namespace {
constexpr float step_seconds = 1.0F / 60.0F;
constexpr PhysicsVector origin{1.0F, 2.0F, 3.0F};
constexpr PhysicsVector forward{0.0F, 0.0F, -1.0F};

std::optional<PhysicsStaticRay> rifleStep(PrototypeRifle& rifle) {
  rifle.advanceFixedStep(step_seconds);
  return rifle.tryFire(origin, forward);
}
}  // namespace

TEST(PrototypeRifle, LatchesPressUntilAFixedStepAndUsesPreKickDirection) {
  PrototypeRifle rifle;
  rifle.sampleTrigger(true, true);
  EXPECT_TRUE(rifle.triggerPending());
  EXPECT_FLOAT_EQ(rifle.recoilPitchDegrees(), 0.0F);

  const auto shot = rifleStep(rifle);
  ASSERT_TRUE(shot);
  EXPECT_FLOAT_EQ(shot->origin.x, origin.x);
  EXPECT_FLOAT_EQ(shot->direction.y, forward.y);
  EXPECT_FLOAT_EQ(shot->maximum_distance, prototype_rifle_maximum_range);
  EXPECT_FLOAT_EQ(rifle.recoilPitchDegrees(),
                  prototype_rifle_recoil_kick_degrees);
}

TEST(PrototypeRifle, FiresAutomaticallyAtFixedCadenceAndPreservesCooldown) {
  PrototypeRifle rifle;
  rifle.sampleTrigger(true, true);
  ASSERT_TRUE(rifleStep(rifle));
  for (int step = 0; step < 5; ++step) {
    EXPECT_FALSE(rifleStep(rifle));
  }

  rifle.sampleTrigger(false, true);
  rifle.sampleTrigger(true, true);
  EXPECT_TRUE(rifle.triggerPending());
  EXPECT_TRUE(rifleStep(rifle));
  EXPECT_FALSE(rifle.triggerPending());
}

TEST(PrototypeRifle, ReleaseStopsAutomaticFireAndNewPressFires) {
  PrototypeRifle rifle;
  rifle.sampleTrigger(true, true);
  ASSERT_TRUE(rifleStep(rifle));
  rifle.sampleTrigger(false, true);
  for (int step = 0; step < 12; ++step) {
    EXPECT_FALSE(rifleStep(rifle));
  }
  rifle.sampleTrigger(true, true);
  EXPECT_TRUE(rifleStep(rifle));
}

TEST(PrototypeRifle, InactiveRecaptureClickIsSuppressedUntilRelease) {
  PrototypeRifle rifle;
  rifle.sampleTrigger(true, false);
  EXPECT_FALSE(rifleStep(rifle));
  rifle.sampleTrigger(true, true);
  EXPECT_FALSE(rifleStep(rifle));
  rifle.sampleTrigger(false, true);
  rifle.sampleTrigger(true, true);
  EXPECT_TRUE(rifleStep(rifle));
}

TEST(PrototypeRifle, RecoilAccumulatesClampsAndRecovers) {
  PrototypeRifle rifle;
  rifle.sampleTrigger(true, true);
  for (int shot = 0; shot < 12; ++shot) {
    while (!rifleStep(rifle)) {
    }
    EXPECT_LE(rifle.recoilPitchDegrees(),
              prototype_rifle_maximum_recoil_degrees);
  }
  EXPECT_FLOAT_EQ(rifle.recoilPitchDegrees(),
                  prototype_rifle_maximum_recoil_degrees);

  rifle.sampleTrigger(false, true);
  for (int step = 0; step < 120; ++step) {
    EXPECT_FALSE(rifleStep(rifle));
  }
  EXPECT_FLOAT_EQ(rifle.recoilPitchDegrees(), 0.0F);
}
