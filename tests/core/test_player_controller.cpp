#include <gtest/gtest.h>

#include <vector>

#include "src/core/game/player_controller.hpp"

TEST(PlayerControllerTest, HorizontalMovementStopsAgainstCollider) {
  PlayerController player;
  const math::Vec3 start{0.0f, 1.35f, 0.0f};
  const std::vector<math::AABB> colliders{
      {{1.0f, 0.0f, -1.0f}, {2.0f, 2.0f, 1.0f}}};

  const math::Vec3 result =
      player.moveWithCollision(start, {2.0f, 0.0f, 0.0f}, colliders);

  EXPECT_FLOAT_EQ(result.x, start.x);
  EXPECT_FLOAT_EQ(result.z, start.z);
}

TEST(PlayerControllerTest, HorizontalMovementSlidesAlongOpenAxis) {
  PlayerController player;
  const math::Vec3 start{0.0f, 1.35f, 0.0f};
  const std::vector<math::AABB> colliders{
      {{1.0f, 0.0f, -1.0f}, {2.0f, 2.0f, 1.0f}}};

  const math::Vec3 result =
      player.moveWithCollision(start, {2.0f, 0.0f, 2.0f}, colliders);

  EXPECT_FLOAT_EQ(result.x, start.x);
  EXPECT_FLOAT_EQ(result.z, 2.0f);
}
