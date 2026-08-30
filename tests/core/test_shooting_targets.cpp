#include <gtest/gtest.h>

#include <stdexcept>

#include "core/gameplay/shooting_targets.hpp"

namespace {
std::uint32_t maskFor(const ShootingTargetState& target) {
  return std::uint32_t{1} << target.solid_index;
}
}  // namespace

TEST(ShootingTargets, CreatesExactlyThreeIndependentLiveTargets) {
  const PrototypeLevel level;
  const ShootingTargets targets(level);
  ASSERT_EQ(targets.size(), prototype_target_count);
  for (std::size_t index = 0; index < targets.size(); ++index) {
    EXPECT_EQ(targets.target(index).solid_index,
              level.targetDescriptions()[index].solid_index);
    EXPECT_EQ(targets.target(index).health, level.targetStartingHealth());
    EXPECT_FALSE(targets.target(index).destroyed());
  }
}

TEST(ShootingTargets, IgnoresEnvironmentAndDamagesOnlyMappedLiveTarget) {
  const PrototypeLevel level;
  ShootingTargets targets(level);
  EXPECT_FALSE(targets.applyHit(0));
  EXPECT_TRUE(targets.applyHit(targets.target(1).solid_index));
  EXPECT_EQ(targets.target(0).health, level.targetStartingHealth());
  EXPECT_EQ(targets.target(1).health,
            level.targetStartingHealth() - prototype_target_damage);
  EXPECT_EQ(targets.target(2).health, level.targetStartingHealth());
  EXPECT_THROW(static_cast<void>(targets.applyHit(
                   targets.target(0).solid_index, -1)),
               std::invalid_argument);
}

TEST(ShootingTargets, FinalHitClampsHealthAndDestroyedHitsDoNothing) {
  ShootingTargets targets(PrototypeLevel{});
  const std::size_t solid_index = targets.target(0).solid_index;
  for (int hit = 0; hit < 4; ++hit) {
    EXPECT_TRUE(targets.applyHit(solid_index));
  }
  EXPECT_EQ(targets.target(0).health, 0);
  EXPECT_TRUE(targets.target(0).destroyed());
  EXPECT_FALSE(targets.applyHit(solid_index));
  EXPECT_EQ(targets.target(0).health, 0);
  EXPECT_EQ(targets.target(1).health, 100);
}

TEST(ShootingTargets, RefreshesAndExpiresFixedStepHighlight) {
  ShootingTargets targets(PrototypeLevel{});
  const std::size_t solid_index = targets.target(1).solid_index;
  ASSERT_TRUE(targets.applyHit(solid_index));
  targets.fixedStep(prototype_target_highlight_seconds * 0.75F);
  EXPECT_NE(targets.presentation().highlighted_solid_mask &
                maskFor(targets.target(1)),
            0U);
  ASSERT_TRUE(targets.applyHit(solid_index));
  targets.fixedStep(prototype_target_highlight_seconds * 0.75F);
  EXPECT_NE(targets.presentation().highlighted_solid_mask &
                maskFor(targets.target(1)),
            0U);
  targets.fixedStep(prototype_target_highlight_seconds * 0.25F);
  EXPECT_EQ(targets.presentation().highlighted_solid_mask &
                maskFor(targets.target(1)),
            0U);
}

TEST(ShootingTargets, HighlightPrecedesPersistentDestroyedDimming) {
  ShootingTargets targets(PrototypeLevel{});
  const std::size_t solid_index = targets.target(2).solid_index;
  ASSERT_TRUE(targets.applyHit(solid_index, 1000));
  const std::uint32_t mask = maskFor(targets.target(2));
  EXPECT_NE(targets.presentation().highlighted_solid_mask & mask, 0U);
  EXPECT_NE(targets.presentation().dimmed_solid_mask & mask, 0U);
  targets.fixedStep(prototype_target_highlight_seconds);
  EXPECT_EQ(targets.presentation().highlighted_solid_mask & mask, 0U);
  EXPECT_NE(targets.presentation().dimmed_solid_mask & mask, 0U);
}
