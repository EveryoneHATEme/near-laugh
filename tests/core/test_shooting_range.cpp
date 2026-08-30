#include <gtest/gtest.h>

#include "core/gameplay/shooting_range.hpp"
#include "core/world/prototype_level.hpp"

namespace {
constexpr float step_seconds = 1.0F / 60.0F;

struct HeadlessRange {
  PrototypeLevel level{};
  PhysicsWorld physics{level};
  PlayerController player{physics, level.playerSpawn().yaw_degrees};
  PrototypeRifle rifle{};
  ShootingTargets targets{level};

  ShootingRangeStepResult step() {
    return coordinateShootingRangeFixedStep(player, rifle, physics, targets,
                                            step_seconds);
  }
};
}  // namespace

TEST(ShootingRangeRuntime, RetainsZeroStepPressAndEmitsOneShotOnNextStep) {
  HeadlessRange range;
  range.rifle.sampleTrigger(true, true);
  EXPECT_TRUE(range.rifle.triggerPending());
  EXPECT_EQ(range.targets.presentation().highlighted_solid_mask, 0U);

  const ShootingRangeStepResult result = range.step();
  EXPECT_TRUE(result.shot_emitted);
  ASSERT_TRUE(result.static_hit);
  EXPECT_FALSE(result.target_damaged);
  EXPECT_FALSE(range.rifle.triggerPending());
}

TEST(ShootingRangeRuntime, ProcessesMultipleFixedStepsInOrder) {
  HeadlessRange range;
  range.rifle.sampleTrigger(true, true);
  int shot_count = 0;
  for (int step = 0; step < 13; ++step) {
    shot_count += range.step().shot_emitted ? 1 : 0;
  }
  EXPECT_EQ(shot_count, 3);
  EXPECT_GT(range.rifle.recoilPitchDegrees(), 0.0F);
}

TEST(ShootingRangeRuntime, InactiveControlsRecoverWithoutFiring) {
  HeadlessRange range;
  range.rifle.sampleTrigger(true, false);
  for (int step = 0; step < 12; ++step) {
    EXPECT_FALSE(range.step().shot_emitted);
  }
  range.rifle.sampleTrigger(true, true);
  EXPECT_FALSE(range.step().shot_emitted);
}

TEST(ShootingRangeRuntime, ShotCanMissEveryStaticSolid) {
  HeadlessRange range;
  FpsActionSnapshot aim_up;
  aim_up.look_delta_y = -890.0;
  range.player.sampleInput(aim_up, true);
  range.rifle.sampleTrigger(true, true);
  const ShootingRangeStepResult result = range.step();
  EXPECT_TRUE(result.shot_emitted);
  EXPECT_FALSE(result.static_hit);
  EXPECT_FALSE(result.target_damaged);
}

TEST(ShootingRangeRuntime, AppliesTargetHitAndProducesOneFinalPresentation) {
  HeadlessRange range;
  FpsActionSnapshot aim_left_target;
  aim_left_target.look_delta_x = -327.4;
  aim_left_target.look_delta_y = -2.0;
  range.player.sampleInput(aim_left_target, true);
  range.rifle.sampleTrigger(true, true);

  const ShootingRangeStepResult result = range.step();
  ASSERT_TRUE(result.shot_emitted);
  ASSERT_TRUE(result.static_hit);
  EXPECT_EQ(result.static_hit->solid_index,
            range.targets.target(0).solid_index);
  ASSERT_TRUE(result.target_damaged);
  const PrototypeScenePresentation presentation = range.targets.presentation();
  EXPECT_NE(presentation.highlighted_solid_mask &
                (std::uint32_t{1} << result.static_hit->solid_index),
            0U);
  EXPECT_EQ(presentation.dimmed_solid_mask, 0U);
}
