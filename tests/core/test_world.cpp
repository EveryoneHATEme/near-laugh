#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

#include "core/physics/physics_world.hpp"
#include "core/world/prototype_level.hpp"

namespace {
std::size_t countKind(const PrototypeLevel& level, PrototypeSolidKind kind) {
  return static_cast<std::size_t>(std::count_if(
      level.solids().begin(), level.solids().end(),
      [kind](const PrototypeSolid& solid) { return solid.kind == kind; }));
}
}  // namespace

TEST(PrototypeLevel, HasValidContainedMovementTestGeometry) {
  const PrototypeLevel level;
  EXPECT_TRUE(prototypeLevelIsValid(level));
  EXPECT_EQ(countKind(level, PrototypeSolidKind::Floor), 1U);
  EXPECT_GE(countKind(level, PrototypeSolidKind::Boundary), 4U);
  EXPECT_GE(countKind(level, PrototypeSolidKind::Obstacle), 2U);
  EXPECT_GE(countKind(level, PrototypeSolidKind::WalkableStep), 1U);
  EXPECT_GE(countKind(level, PrototypeSolidKind::LowClearance), 1U);

  const auto step = std::find_if(
      level.solids().begin(), level.solids().end(), [](const auto& solid) {
        return solid.kind == PrototypeSolidKind::WalkableStep;
      });
  ASSERT_NE(step, level.solids().end());
  EXPECT_LE(step->half_extent.y * 2.0F, player_maximum_step_height);

  const auto passage = std::find_if(
      level.solids().begin(), level.solids().end(), [](const auto& solid) {
        return solid.kind == PrototypeSolidKind::LowClearance;
      });
  ASSERT_NE(passage, level.solids().end());
  const float passage_clearance =
      passage->center.y - passage->half_extent.y;
  EXPECT_GT(passage_clearance, player_crouched_height);
  EXPECT_LT(passage_clearance, player_standing_height);
}

TEST(PrototypeLevel, HasValidImmutableEnvironmentLight) {
  const PrototypeLevel level;
  const PrototypeEnvironmentLight& light = level.environmentLight();
  EXPECT_TRUE(prototypeEnvironmentLightIsValid(light));
  EXPECT_GT(light.direction_to_light[1], 0.0F);
  EXPECT_GT(light.directional_intensity, 0.0F);
  EXPECT_GT(light.ambient_intensity, 0.0F);
}

TEST(PrototypeLevel, RejectsInvalidEnvironmentLightFixtures) {
  const PrototypeEnvironmentLight valid = PrototypeLevel{}.environmentLight();

  auto invalid = valid;
  invalid.direction_to_light = {0.0F, 0.0F, 0.0F};
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.direction_to_light[0] = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.directional_intensity = -0.01F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.directional_intensity = 1.01F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.ambient_intensity = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.ambient_intensity = 1.01F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));
}

TEST(PrototypeLevel, SpawnFacesSceneAndClearsEverySolid) {
  const PrototypeLevel level;
  EXPECT_FLOAT_EQ(level.playerSpawn().yaw_degrees, -90.0F);
  EXPECT_TRUE(prototypeSpawnIsClear(level, player_capsule_radius,
                                    player_standing_height));
  EXPECT_GT(level.playerSpawn().foot_position.z, 4.0F);
  EXPECT_LT(level.playerSpawn().foot_position.z, 10.0F);
  EXPECT_GT(level.playerSpawn().foot_position.x, -10.0F);
  EXPECT_LT(level.playerSpawn().foot_position.x, 10.0F);
}

TEST(PrototypeLevel, RenderingAndPhysicsDeriveFromMatchingSolids) {
  const PrototypeLevel level;
  const PhysicsWorld physics(level);
  ASSERT_EQ(physics.staticBodyCount(), level.solids().size());
  for (std::size_t index = 0; index < level.solids().size(); ++index) {
    const PrototypeSolid& authored = level.solids()[index];
    const PhysicsStaticSolid collision = physics.staticBody(index);
    EXPECT_FLOAT_EQ(collision.center.x, authored.center.x);
    EXPECT_FLOAT_EQ(collision.center.y, authored.center.y);
    EXPECT_FLOAT_EQ(collision.center.z, authored.center.z);
    EXPECT_FLOAT_EQ(collision.half_extent.x, authored.half_extent.x);
    EXPECT_FLOAT_EQ(collision.half_extent.y, authored.half_extent.y);
    EXPECT_FLOAT_EQ(collision.half_extent.z, authored.half_extent.z);
    EXPECT_EQ(collision.kind, authored.kind);
  }
}
