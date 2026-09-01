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
#include "prototype_level_fixture.hpp"

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
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  EXPECT_TRUE(prototypeLevelIsValid(level));
  EXPECT_EQ(countKind(level, PrototypeSolidKind::Floor), 0U);
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

TEST(PrototypeLevel, HasValidBoundedSculptableTerrain) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  const PrototypeTerrain& terrain = level.terrain();
  EXPECT_TRUE(prototypeTerrainIsValid(terrain));
  EXPECT_EQ(terrain.sample_spacing, prototype_terrain_sample_spacing);
  EXPECT_TRUE(
      prototypeTerrainContains(terrain, terrain.origin.x, terrain.origin.z));
  EXPECT_TRUE(prototypeTerrainContains(
      terrain,
      terrain.origin.x + static_cast<float>(prototype_terrain_cell_count) *
                             terrain.sample_spacing,
      terrain.origin.z + static_cast<float>(prototype_terrain_cell_count) *
                             terrain.sample_spacing));
  EXPECT_FALSE(prototypeTerrainContains(terrain, terrain.origin.x - 0.01F,
                                        terrain.origin.z));
  EXPECT_FALSE(prototypeTerrainContains(terrain, terrain.origin.x,
                                        terrain.origin.z - 0.01F));

  const WorldPosition first = prototypeTerrainSamplePosition(terrain, 0, 0);
  const WorldPosition last = prototypeTerrainSamplePosition(
      terrain, prototype_terrain_cell_count, prototype_terrain_cell_count);
  EXPECT_FLOAT_EQ(first.x, terrain.origin.x);
  EXPECT_FLOAT_EQ(first.z, terrain.origin.z);
  EXPECT_FLOAT_EQ(last.x - first.x, 48.0F);
  EXPECT_FLOAT_EQ(last.z - first.z, 48.0F);
  EXPECT_LT(prototypeTerrainMinimumHeight(terrain), -0.1F);
  EXPECT_GT(prototypeTerrainHeightAt(terrain, -15.0F, -8.0F), 0.1F);
  EXPECT_LT(prototypeTerrainHeightAt(terrain, 14.0F, -11.0F), -0.1F);
}

TEST(PrototypeLevel, RejectsInvalidTerrainFixtures) {
  const PrototypeTerrain valid = loadPackagedPrototypeLevel().terrain();
  auto invalid = valid;
  invalid.sample_spacing = 0.0F;
  EXPECT_FALSE(prototypeTerrainIsValid(invalid));

  invalid = valid;
  invalid.heights[0] = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(prototypeTerrainIsValid(invalid));

  invalid = valid;
  invalid.heights[1] = 100.0F;
  EXPECT_FALSE(prototypeTerrainIsValid(invalid));
}

TEST(PrototypeLevel, HasThreeDistinctInertTexturedPlates) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
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
  const PrototypeLevel level = loadPackagedPrototypeLevel();
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
  PrototypeSolid solid = loadPackagedPrototypeLevel().solids().front();
  solid.surface = static_cast<PrototypeSurface>(prototype_surface_count);
  EXPECT_FALSE(prototypeSurfaceIsValid(solid.surface));
  EXPECT_FALSE(prototypeSolidIsValid(solid));

  solid = loadPackagedPrototypeLevel().solids().front();
  solid.kind = static_cast<PrototypeSolidKind>(100);
  EXPECT_FALSE(prototypeSolidIsValid(solid));

  solid = loadPackagedPrototypeLevel().solids().front();
  solid.surface = PrototypeSurface::Obstacle;
  EXPECT_FALSE(prototypeSolidIsValid(solid));
}

TEST(PrototypeLevel, HasOneValidFilesystemFreeStaticChairDescription) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  const PrototypeStaticProp& chair = level.staticProp();
  static_assert(std::is_same_v<
                decltype(std::declval<const PrototypeLevel&>().staticProp()),
                const PrototypeStaticProp&>);
  EXPECT_TRUE(prototypeStaticPropIsValid(chair));
  EXPECT_TRUE(std::isfinite(chair.translation.x));
  EXPECT_TRUE(std::isfinite(chair.translation.y));
  EXPECT_TRUE(std::isfinite(chair.translation.z));
  EXPECT_TRUE(std::isfinite(chair.yaw_degrees));
  EXPECT_GT(chair.uniform_scale, 0.0F);
  EXPECT_EQ(chair.surface, PrototypeSurface::Obstacle);
  const WorldPosition center = prototypeStaticPropProxyWorldCenter(chair);
  const WorldExtent half_extent =
      prototypeStaticPropProxyWorldHalfExtent(chair);
  EXPECT_TRUE(std::isfinite(center.x));
  EXPECT_TRUE(std::isfinite(center.y));
  EXPECT_TRUE(std::isfinite(center.z));
  EXPECT_GT(half_extent.x, 0.0F);
  EXPECT_GT(half_extent.y, 0.0F);
  EXPECT_GT(half_extent.z, 0.0F);
}

TEST(PrototypeLevel, RejectsInvalidStaticChairFixtures) {
  const PrototypeStaticProp valid = loadPackagedPrototypeLevel().staticProp();

  auto invalid = valid;
  invalid.translation.x = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(prototypeStaticPropIsValid(invalid));

  invalid = valid;
  invalid.yaw_degrees = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(prototypeStaticPropIsValid(invalid));

  invalid = valid;
  invalid.uniform_scale = 0.0F;
  EXPECT_FALSE(prototypeStaticPropIsValid(invalid));

  invalid = valid;
  invalid.uniform_scale = -1.0F;
  EXPECT_FALSE(prototypeStaticPropIsValid(invalid));

  invalid = valid;
  invalid.box_proxy_center.y = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(prototypeStaticPropIsValid(invalid));

  invalid = valid;
  invalid.box_proxy_half_extent.z = 0.0F;
  EXPECT_FALSE(prototypeStaticPropIsValid(invalid));

  invalid = valid;
  invalid.surface = PrototypeSurface::Floor;
  EXPECT_FALSE(prototypeStaticPropIsValid(invalid));
}

TEST(PrototypeLevel, HasValidImmutableEnvironmentLight) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  static_assert(std::is_same_v<decltype(std::declval<const PrototypeLevel&>()
                                            .environmentLight()),
                               const PrototypeEnvironmentLight&>);
  const PrototypeEnvironmentLight& light = level.environmentLight();
  static_assert(prototype_point_light_count == 2U);
  ASSERT_EQ(light.point_lights.size(), 2U);
  EXPECT_TRUE(prototypeEnvironmentLightIsValid(light));
  EXPECT_FLOAT_EQ(light.ambient_intensity, 0.12F);
  EXPECT_FLOAT_EQ(prototype_maximum_ambient_intensity, 0.20F);
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

  const PrototypeLevel independently_constructed_level =
      loadPackagedPrototypeLevel();
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
  const PrototypeEnvironmentLight valid =
      loadPackagedPrototypeLevel().environmentLight();

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
  invalid.ambient_intensity = prototype_maximum_ambient_intensity;
  EXPECT_TRUE(prototypeEnvironmentLightIsValid(invalid));

  invalid = valid;
  invalid.ambient_intensity = prototype_maximum_ambient_intensity + 0.01F;
  EXPECT_FALSE(prototypeEnvironmentLightIsValid(invalid));
}

TEST(PrototypeLevel, SpawnFacesSceneAndClearsEverySolid) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  EXPECT_FLOAT_EQ(level.playerSpawn().yaw_degrees, -90.0F);
  EXPECT_TRUE(prototypeSpawnIsClear(level, player_capsule_radius,
                                    player_standing_height));
  EXPECT_GT(level.playerSpawn().foot_position.z, 4.0F);
  EXPECT_LT(level.playerSpawn().foot_position.z, 10.0F);
  EXPECT_GT(level.playerSpawn().foot_position.x, -10.0F);
  EXPECT_LT(level.playerSpawn().foot_position.x, 10.0F);
  EXPECT_FLOAT_EQ(level.playerSpawn().foot_position.y,
                  prototypeTerrainHeightAt(
                      level.terrain(), level.playerSpawn().foot_position.x,
                      level.playerSpawn().foot_position.z));
}

TEST(PrototypeLevel, RenderingAndPhysicsDeriveFromMatchingSolids) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  const PhysicsWorld physics(level);
  EXPECT_TRUE(physics.hasTerrainCollision());
  ASSERT_EQ(physics.staticBodyCount(), level.solids().size() + 1U);
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
    EXPECT_FLOAT_EQ(collision.yaw_degrees, 0.0F);
  }

  const PhysicsStaticSolid prop_collision =
      physics.staticBody(level.solids().size());
  const WorldPosition prop_center =
      prototypeStaticPropProxyWorldCenter(level.staticProp());
  const WorldExtent prop_half_extent =
      prototypeStaticPropProxyWorldHalfExtent(level.staticProp());
  EXPECT_FLOAT_EQ(prop_collision.center.x, prop_center.x);
  EXPECT_FLOAT_EQ(prop_collision.center.y, prop_center.y);
  EXPECT_FLOAT_EQ(prop_collision.center.z, prop_center.z);
  EXPECT_FLOAT_EQ(prop_collision.half_extent.x, prop_half_extent.x);
  EXPECT_FLOAT_EQ(prop_collision.half_extent.y, prop_half_extent.y);
  EXPECT_FLOAT_EQ(prop_collision.half_extent.z, prop_half_extent.z);
  EXPECT_FLOAT_EQ(prop_collision.yaw_degrees, level.staticProp().yaw_degrees);

  for (const PrototypeSolid& solid : level.solids()) {
    if (solid.kind != PrototypeSolidKind::Boundary) {
      continue;
    }
    EXPECT_LE(solid.center.y - solid.half_extent.y,
              prototypeTerrainMinimumHeight(level.terrain()));
  }
}
