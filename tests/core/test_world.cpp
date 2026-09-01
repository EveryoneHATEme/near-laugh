#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/physics/physics_world.hpp"
#include "core/world/prototype_level.hpp"

namespace {
std::size_t countKind(const PrototypeLevel& level, PrototypeSolidKind kind) {
  return static_cast<std::size_t>(std::count_if(
      level.solids().begin(), level.solids().end(),
      [kind](const PrototypeSolid& solid) { return solid.kind == kind; }));
}

PrototypeSurface expectedSurface(PrototypeSolidKind kind) {
  switch (kind) {
    case PrototypeSolidKind::Floor:
      return PrototypeSurface::Floor;
    case PrototypeSolidKind::Boundary:
      return PrototypeSurface::Boundary;
    case PrototypeSolidKind::Obstacle:
    case PrototypeSolidKind::WalkableStep:
    case PrototypeSolidKind::LowClearance:
      return PrototypeSurface::Obstacle;
    case PrototypeSolidKind::ShootingTarget:
      return PrototypeSurface::ShootingTarget;
  }
  return static_cast<PrototypeSurface>(prototype_surface_count);
}

float distance(const WorldPosition& first, const WorldPosition& second) {
  const float x = first.x - second.x;
  const float y = first.y - second.y;
  const float z = first.z - second.z;
  return std::sqrt(x * x + y * y + z * z);
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
  EXPECT_EQ(countKind(level, PrototypeSolidKind::ShootingTarget),
            prototype_plate_count);

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
  const float passage_clearance = passage->center.y - passage->half_extent.y;
  EXPECT_GT(passage_clearance, player_crouched_height);
  EXPECT_LT(passage_clearance, player_standing_height);
}

TEST(PrototypeLevel, HasThreeDistinctInertTexturedPlates) {
  const PrototypeLevel level;
  std::size_t plate_count = 0;
  float previous_x = -1000.0F;
  for (const PrototypeSolid& solid : level.solids()) {
    if (solid.kind != PrototypeSolidKind::ShootingTarget) {
      continue;
    }
    ++plate_count;
    EXPECT_EQ(solid.kind, PrototypeSolidKind::ShootingTarget);
    EXPECT_EQ(solid.surface, PrototypeSurface::ShootingTarget);
    EXPECT_GT(solid.center.x, previous_x);
    previous_x = solid.center.x;
  }
  EXPECT_EQ(plate_count, prototype_plate_count);
  EXPECT_TRUE(prototypeLevelIsValid(level));
}

TEST(PrototypeLevel, AssignsOneFixedSurfaceRoleToEveryBuiltInSolid) {
  const PrototypeLevel level;
  static_assert(static_cast<std::uint32_t>(PrototypeSurface::Floor) == 0U);
  static_assert(static_cast<std::uint32_t>(PrototypeSurface::Boundary) == 1U);
  static_assert(static_cast<std::uint32_t>(PrototypeSurface::Obstacle) == 2U);
  static_assert(static_cast<std::uint32_t>(PrototypeSurface::ShootingTarget) ==
                3U);
  static_assert(prototype_surface_count == 4U);

  for (const PrototypeSolid& solid : level.solids()) {
    EXPECT_TRUE(prototypeSurfaceIsValid(solid.surface));
    EXPECT_EQ(solid.surface, expectedSurface(solid.kind));
  }
}

TEST(PrototypeLevel, RejectsInvalidSurfaceRoles) {
  PrototypeSolid solid = PrototypeLevel{}.solids().front();
  solid.surface = static_cast<PrototypeSurface>(prototype_surface_count);
  EXPECT_FALSE(prototypeSurfaceIsValid(solid.surface));
  EXPECT_FALSE(prototypeSolidIsValid(solid));
}

TEST(PrototypeLevel, HasValidImmutableEnvironmentLight) {
  const PrototypeLevel level;
  static_assert(std::is_same_v<decltype(std::declval<const PrototypeLevel&>()
                                            .environmentLight()),
                               const PrototypeEnvironmentLight&>);
  const PrototypeEnvironmentLight& light = level.environmentLight();
  static_assert(prototype_point_light_count == 2U);
  ASSERT_EQ(light.point_lights.size(), 2U);
  EXPECT_TRUE(prototypeEnvironmentLightIsValid(light));
  EXPECT_GT(light.ambient_intensity, 0.0F);
  EXPECT_LE(light.ambient_intensity, prototype_maximum_ambient_intensity);

  for (const PrototypePointLight& point_light : light.point_lights) {
    EXPECT_GT(point_light.intensity, 0.0F);
    EXPECT_GT(point_light.radius, 0.0F);
  }

  const PrototypePointLight& spawn_light = light.point_lights[0];
  const PrototypePointLight& destination_light = light.point_lights[1];
  EXPECT_GT(spawn_light.color[2], spawn_light.color[0]);
  EXPECT_GT(destination_light.color[0], destination_light.color[2]);
  EXPECT_GT(destination_light.intensity, spawn_light.intensity);
  EXPECT_GT(distance(spawn_light.position, destination_light.position),
            spawn_light.radius + destination_light.radius);

  constexpr WorldPosition dark_route_sample{0.0F, 1.6F, 1.5F};
  EXPECT_GE(distance(dark_route_sample, spawn_light.position),
            spawn_light.radius);
  EXPECT_GE(distance(dark_route_sample, destination_light.position),
            destination_light.radius);

  const PrototypeLevel independently_constructed_level;
  for (std::size_t index = 0; index < prototype_point_light_count; ++index) {
    const WorldPosition& first = light.point_lights[index].position;
    const WorldPosition& second =
        independently_constructed_level.environmentLight()
            .point_lights[index]
            .position;
    EXPECT_FLOAT_EQ(first.x, second.x);
    EXPECT_FLOAT_EQ(first.y, second.y);
    EXPECT_FLOAT_EQ(first.z, second.z);
  }
  EXPECT_NE(level.playerSpawn().foot_position.z, destination_light.position.z);
}

TEST(PrototypeLevel, RejectsInvalidEnvironmentLightFixtures) {
  const PrototypeEnvironmentLight valid = PrototypeLevel{}.environmentLight();

  auto invalid = valid;
  invalid.point_lights[0].position.x = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.point_lights[0].color[1] = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.point_lights[0].color[0] = -0.01F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.point_lights[0].intensity = -0.01F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.point_lights[0].intensity = 0.0F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.point_lights[1].intensity = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.point_lights[0].radius = 0.0F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.point_lights[1].radius = -0.01F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.ambient_intensity = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.ambient_intensity = prototype_maximum_ambient_intensity + 0.01F;
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
