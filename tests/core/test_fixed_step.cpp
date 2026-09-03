#include <gtest/gtest.h>

#include <chrono>

#include "core/player/player_controller.hpp"
#include "core/simulation/fixed_step.hpp"
#include "core/world/prototype_level.hpp"
#include "prototype_level_fixture.hpp"

TEST(FixedStepAccumulator, RetainsSubStepRemainder) {
  FixedStepAccumulator accumulator;
  const FixedStepBatch batch =
      accumulator.advance(FixedStepAccumulator::step_seconds * 0.25);
  EXPECT_EQ(batch.complete_steps, 0);
  EXPECT_NEAR(batch.interpolation_alpha, 0.25F, 0.00001F);
}

TEST(FixedStepAccumulator, ExecutesExactAndMultipleCompleteSteps) {
  FixedStepAccumulator accumulator;
  FixedStepBatch batch =
      accumulator.advance(FixedStepAccumulator::step_seconds);
  EXPECT_EQ(batch.complete_steps, 1);
  EXPECT_NEAR(batch.interpolation_alpha, 0.0F, 0.00001F);

  batch = accumulator.advance(FixedStepAccumulator::step_seconds * 2.5);
  EXPECT_EQ(batch.complete_steps, 2);
  EXPECT_NEAR(batch.interpolation_alpha, 0.5F, 0.00001F);
}

TEST(FixedStepAccumulator, CapsStallContributionAtSixSteps) {
  FixedStepAccumulator accumulator;
  static_cast<void>(
      accumulator.advance(FixedStepAccumulator::step_seconds * 0.5));
  const FixedStepBatch batch = accumulator.advance(10.0);
  EXPECT_EQ(batch.complete_steps,
            FixedStepAccumulator::maximum_steps_per_sample);
  EXPECT_NEAR(batch.interpolation_alpha, 0.5F, 0.00001F);
}

TEST(FixedStepAccumulator, ResetClearsRemainderAndClockOrigin) {
  using namespace std::chrono_literals;
  FixedStepAccumulator accumulator;
  const FixedStepAccumulator::Clock::time_point start{};
  EXPECT_EQ(accumulator.sample(start).complete_steps, 0);
  static_cast<void>(accumulator.advance(0.01));
  accumulator.reset();
  EXPECT_DOUBLE_EQ(accumulator.remainderSeconds(), 0.0);
  const FixedStepBatch restored = accumulator.sample(start + 10s);
  EXPECT_EQ(restored.complete_steps, 0);
  EXPECT_FLOAT_EQ(restored.interpolation_alpha, 0.0F);
}

TEST(PlayerInterpolation, ZeroStepKeepsPositionWhileLookIsLatest) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  const PlayerCameraPosition before = player.interpolatedCameraPosition(0.5F);
  PlayerActionSnapshot look;
  look.look_delta_x = 25.0;
  player.sampleInput(look, true);
  const PlayerCameraPosition after = player.interpolatedCameraPosition(0.5F);
  EXPECT_FLOAT_EQ(after.x, before.x);
  EXPECT_FLOAT_EQ(after.y, before.y);
  EXPECT_FLOAT_EQ(after.z, before.z);
  EXPECT_FLOAT_EQ(player.yawDegrees(), -87.5F);
}

TEST(PlayerInterpolation, MultiStepIterationRetainsLatestTwoValidPoses) {
  constexpr float delta = 1.0F / 60.0F;
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  for (int step = 0; step < 120; ++step) {
    player.fixedStep(delta);
  }
  player.collapsePresentationState();
  PlayerActionSnapshot right;
  right.move_right = true;
  player.sampleInput(right, true);
  player.fixedStep(delta);
  player.fixedStep(delta);
  player.fixedStep(delta);

  const PlayerCameraPosition previous = player.interpolatedCameraPosition(0.0F);
  const PlayerCameraPosition halfway = player.interpolatedCameraPosition(0.5F);
  const PlayerCameraPosition current = player.interpolatedCameraPosition(1.0F);
  EXPECT_LT(previous.x, current.x);
  EXPECT_NEAR(halfway.x, (previous.x + current.x) * 0.5F, 0.0001F);
  EXPECT_NEAR(previous.z, current.z, 0.0001F);
}

TEST(PlayerInterpolation, InterpolatesStanceEyeHeightAndCanCollapse) {
  constexpr float delta = 1.0F / 60.0F;
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  for (int step = 0; step < 120; ++step) {
    player.fixedStep(delta);
  }
  player.collapsePresentationState();
  PlayerActionSnapshot crouch;
  crouch.crouch = true;
  player.sampleInput(crouch, true);
  player.fixedStep(delta);
  const PlayerCameraPosition standing = player.interpolatedCameraPosition(0.0F);
  const PlayerCameraPosition halfway = player.interpolatedCameraPosition(0.5F);
  const PlayerCameraPosition crouched = player.interpolatedCameraPosition(1.0F);
  EXPECT_GT(standing.y, crouched.y);
  EXPECT_NEAR(halfway.y, (standing.y + crouched.y) * 0.5F, 0.0001F);

  player.collapsePresentationState();
  const PlayerCameraPosition collapsed_previous =
      player.interpolatedCameraPosition(0.0F);
  const PlayerCameraPosition collapsed_current =
      player.interpolatedCameraPosition(1.0F);
  EXPECT_FLOAT_EQ(collapsed_previous.x, collapsed_current.x);
  EXPECT_FLOAT_EQ(collapsed_previous.y, collapsed_current.y);
  EXPECT_FLOAT_EQ(collapsed_previous.z, collapsed_current.z);
}
