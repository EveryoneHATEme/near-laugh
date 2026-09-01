#include "core/world/prototype_level.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr WorldColor boundary_color{55, 78, 122, 255};
constexpr WorldColor red{205, 63, 73, 255};
constexpr WorldColor green{66, 176, 111, 255};
constexpr WorldColor gold{225, 167, 62, 255};
constexpr WorldColor violet{139, 91, 196, 255};
constexpr WorldColor step_color{70, 184, 190, 255};
constexpr WorldColor passage_color{190, 118, 197, 255};
constexpr WorldColor target_color{222, 122, 58, 255};

std::size_t terrainSampleIndex(std::size_t sample_x,
                               std::size_t sample_z) noexcept {
  return sample_z * prototype_terrain_sample_count + sample_x;
}

float authoredTerrainHeight(float x, float z) noexcept {
  const float hill_x = (x + 15.0F) / 5.0F;
  const float hill_z = (z + 8.0F) / 6.0F;
  const float depression_x = (x - 14.0F) / 4.5F;
  const float depression_z = (z + 11.0F) / 5.0F;
  return 0.75F * std::exp(-(hill_x * hill_x + hill_z * hill_z)) -
         0.55F * std::exp(-(depression_x * depression_x +
                             depression_z * depression_z));
}

PrototypeTerrain makePrototypeTerrain() {
  PrototypeTerrain terrain{{-24.0F, 0.0F, -26.0F},
                           prototype_terrain_sample_spacing, {}};
  for (std::size_t sample_z = 0; sample_z < prototype_terrain_sample_count;
       ++sample_z) {
    for (std::size_t sample_x = 0; sample_x < prototype_terrain_sample_count;
         ++sample_x) {
      const float x = terrain.origin.x +
                      static_cast<float>(sample_x) * terrain.sample_spacing;
      const float z = terrain.origin.z +
                      static_cast<float>(sample_z) * terrain.sample_spacing;
      terrain.heights[terrainSampleIndex(sample_x, sample_z)] =
          authoredTerrainHeight(x, z);
    }
  }
  return terrain;
}

float terrainHeight(const PrototypeTerrain& terrain, std::size_t sample_x,
                    std::size_t sample_z) noexcept {
  return terrain.origin.y + terrain.heights[terrainSampleIndex(sample_x, sample_z)];
}

bool terrainTriangleHasSupportedSlope(const PrototypeTerrain& terrain,
                                      std::size_t sample_x,
                                      std::size_t sample_z, bool first) noexcept {
  const WorldPosition p00 =
      prototypeTerrainSamplePosition(terrain, sample_x, sample_z);
  const WorldPosition p01 =
      prototypeTerrainSamplePosition(terrain, sample_x, sample_z + 1);
  const WorldPosition p11 =
      prototypeTerrainSamplePosition(terrain, sample_x + 1, sample_z + 1);
  const WorldPosition p10 =
      prototypeTerrainSamplePosition(terrain, sample_x + 1, sample_z);
  const WorldPosition& a = p00;
  const WorldPosition& b = first ? p01 : p11;
  const WorldPosition& c = first ? p11 : p10;
  const float ab_x = b.x - a.x;
  const float ab_y = b.y - a.y;
  const float ab_z = b.z - a.z;
  const float ac_x = c.x - a.x;
  const float ac_y = c.y - a.y;
  const float ac_z = c.z - a.z;
  const float normal_x = ab_y * ac_z - ab_z * ac_y;
  const float normal_y = ab_z * ac_x - ab_x * ac_z;
  const float normal_z = ab_x * ac_y - ab_y * ac_x;
  const float normal_length =
      std::sqrt(normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
  if (!(normal_length > 0.0F) || !std::isfinite(normal_length)) {
    return false;
  }
  const float minimum_normal_y =
      std::cos(prototype_terrain_maximum_slope_degrees *
               std::numbers::pi_v<float> / 180.0F);
  return normal_y / normal_length >= minimum_normal_y;
}

bool overlaps(float first_min, float first_max, float second_min,
              float second_max) noexcept {
  return first_min < second_max && first_max > second_min;
}
}  // namespace

PrototypeLevel::PrototypeLevel()
    : terrain_(makePrototypeTerrain()),
      solids_{
          {{-24.25F, 1.5F, -2.0F},
           {0.25F, 3.5F, 24.0F},
           boundary_color,
           PrototypeSolidKind::Boundary,
           PrototypeSurface::Boundary},
          {{24.25F, 1.5F, -2.0F},
           {0.25F, 3.5F, 24.0F},
           boundary_color,
           PrototypeSolidKind::Boundary,
           PrototypeSurface::Boundary},
          {{0.0F, 1.5F, -26.25F},
           {24.5F, 3.5F, 0.25F},
           boundary_color,
           PrototypeSolidKind::Boundary,
           PrototypeSurface::Boundary},
          {{0.0F, 1.5F, 22.25F},
           {24.5F, 3.5F, 0.25F},
           boundary_color,
           PrototypeSolidKind::Boundary,
           PrototypeSurface::Boundary},
          {{0.0F, 1.2F, 0.1F},
           {1.2F, 1.2F, 0.9F},
           red,
           PrototypeSolidKind::Obstacle,
           PrototypeSurface::Obstacle},
          {{0.0F, 1.5F, -5.5F},
           {1.5F, 1.5F, 1.0F},
           green,
           PrototypeSolidKind::Obstacle,
           PrototypeSurface::Obstacle},
          {{-4.5F, 2.0F, -3.5F},
           {1.0F, 2.0F, 1.0F},
           gold,
           PrototypeSolidKind::Obstacle,
           PrototypeSurface::Obstacle},
          {{4.5F, 1.0F, -8.5F},
           {1.0F, 1.0F, 1.0F},
           violet,
           PrototypeSolidKind::Obstacle,
           PrototypeSurface::Obstacle},
          {{-6.5F, 0.15F, 4.0F},
           {1.5F, 0.15F, 1.5F},
           step_color,
           PrototypeSolidKind::WalkableStep,
           PrototypeSurface::Obstacle},
          {{6.0F, 1.55F, 1.5F},
           {1.75F, 0.15F, 3.0F},
           passage_color,
           PrototypeSolidKind::LowClearance,
           PrototypeSurface::Obstacle},
          {{-9.0F, 1.75F, -7.0F},
           {0.75F, 0.75F, 0.12F},
           target_color,
           PrototypeSolidKind::ShootingTarget,
           PrototypeSurface::ShootingTarget},
          {{0.0F, 4.0F, -12.5F},
           {0.75F, 0.75F, 0.12F},
           target_color,
           PrototypeSolidKind::ShootingTarget,
           PrototypeSurface::ShootingTarget},
          {{9.0F, 1.75F, -7.0F},
           {0.75F, 0.75F, 0.12F},
           target_color,
           PrototypeSolidKind::ShootingTarget,
           PrototypeSurface::ShootingTarget},
      },
      player_spawn_{{0.0F, prototypeTerrainHeightAt(terrain_, 0.0F, 7.0F),
                     7.0F},
                    -90.0F},
      environment_light_{
          {{{{0.0F, 2.4F, 6.0F}, {0.30F, 0.50F, 0.90F}, 0.65F, 4.0F},
            {{0.0F, 4.0F, -9.0F}, {1.00F, 0.48F, 0.20F}, 0.95F, 10.0F}}},
          0.12F} {}

bool prototypeEnvironmentLightIsValid(
    const PrototypeEnvironmentLight& light) noexcept {
  const bool point_lights_are_valid = std::all_of(
      light.point_lights.begin(), light.point_lights.end(),
      [](const PrototypePointLight& point_light) {
        return std::isfinite(point_light.position.x) &&
               std::isfinite(point_light.position.y) &&
               std::isfinite(point_light.position.z) &&
               std::all_of(point_light.color.begin(), point_light.color.end(),
                           [](float component) {
                             return std::isfinite(component) &&
                                    component >= 0.0F;
                           }) &&
               std::isfinite(point_light.intensity) &&
               point_light.intensity > 0.0F &&
               std::isfinite(point_light.radius) && point_light.radius > 0.0F;
      });
  return point_lights_are_valid && std::isfinite(light.ambient_intensity) &&
         light.ambient_intensity >= 0.0F &&
         light.ambient_intensity <= prototype_maximum_ambient_intensity;
}

bool prototypeSurfaceIsValid(PrototypeSurface surface) noexcept {
  switch (surface) {
    case PrototypeSurface::Floor:
    case PrototypeSurface::Boundary:
    case PrototypeSurface::Obstacle:
    case PrototypeSurface::ShootingTarget:
      return true;
  }
  return false;
}

bool prototypeSolidIsValid(const PrototypeSolid& solid) noexcept {
  return std::isfinite(solid.center.x) && std::isfinite(solid.center.y) &&
         std::isfinite(solid.center.z) && std::isfinite(solid.half_extent.x) &&
         std::isfinite(solid.half_extent.y) &&
         std::isfinite(solid.half_extent.z) && solid.half_extent.x > 0.0F &&
         solid.half_extent.y > 0.0F && solid.half_extent.z > 0.0F &&
         prototypeSurfaceIsValid(solid.surface);
}

WorldPosition prototypeTerrainSamplePosition(const PrototypeTerrain& terrain,
                                             std::size_t sample_x,
                                             std::size_t sample_z) noexcept {
  if (sample_x >= prototype_terrain_sample_count ||
      sample_z >= prototype_terrain_sample_count) {
    return {std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::quiet_NaN(),
            std::numeric_limits<float>::quiet_NaN()};
  }
  return {terrain.origin.x + static_cast<float>(sample_x) * terrain.sample_spacing,
          terrainHeight(terrain, sample_x, sample_z),
          terrain.origin.z + static_cast<float>(sample_z) * terrain.sample_spacing};
}

bool prototypeTerrainContains(const PrototypeTerrain& terrain, float x,
                              float z) noexcept {
  if (!std::isfinite(x) || !std::isfinite(z) ||
      !(terrain.sample_spacing > 0.0F) ||
      !std::isfinite(terrain.sample_spacing)) {
    return false;
  }
  const float maximum_x =
      terrain.origin.x + static_cast<float>(prototype_terrain_cell_count) *
                             terrain.sample_spacing;
  const float maximum_z =
      terrain.origin.z + static_cast<float>(prototype_terrain_cell_count) *
                             terrain.sample_spacing;
  return x >= terrain.origin.x && x <= maximum_x && z >= terrain.origin.z &&
         z <= maximum_z;
}

float prototypeTerrainHeightAt(const PrototypeTerrain& terrain, float x,
                               float z) noexcept {
  if (!prototypeTerrainContains(terrain, x, z)) {
    return std::numeric_limits<float>::quiet_NaN();
  }
  const float local_x = (x - terrain.origin.x) / terrain.sample_spacing;
  const float local_z = (z - terrain.origin.z) / terrain.sample_spacing;
  const std::size_t sample_x = std::min(
      static_cast<std::size_t>(std::floor(local_x)), prototype_terrain_cell_count - 1);
  const std::size_t sample_z = std::min(
      static_cast<std::size_t>(std::floor(local_z)), prototype_terrain_cell_count - 1);
  const float fraction_x = std::min(local_x - static_cast<float>(sample_x), 1.0F);
  const float fraction_z = std::min(local_z - static_cast<float>(sample_z), 1.0F);
  const float h00 = terrainHeight(terrain, sample_x, sample_z);
  const float h01 = terrainHeight(terrain, sample_x, sample_z + 1);
  const float h11 = terrainHeight(terrain, sample_x + 1, sample_z + 1);
  const float h10 = terrainHeight(terrain, sample_x + 1, sample_z);
  if (fraction_x <= fraction_z) {
    return h00 * (1.0F - fraction_z) + h01 * (fraction_z - fraction_x) +
           h11 * fraction_x;
  }
  return h00 * (1.0F - fraction_x) + h11 * fraction_z +
         h10 * (fraction_x - fraction_z);
}

float prototypeTerrainMinimumHeight(const PrototypeTerrain& terrain) noexcept {
  float minimum = std::numeric_limits<float>::infinity();
  for (const float height : terrain.heights) {
    minimum = std::min(minimum, terrain.origin.y + height);
  }
  return minimum;
}

bool prototypeTerrainIsValid(const PrototypeTerrain& terrain) noexcept {
  if (!std::isfinite(terrain.origin.x) || !std::isfinite(terrain.origin.y) ||
      !std::isfinite(terrain.origin.z) ||
      !(terrain.sample_spacing > 0.0F) ||
      !std::isfinite(terrain.sample_spacing) ||
      !std::all_of(terrain.heights.begin(), terrain.heights.end(),
                   [](float height) { return std::isfinite(height); })) {
    return false;
  }
  for (std::size_t sample_z = 0; sample_z < prototype_terrain_cell_count;
       ++sample_z) {
    for (std::size_t sample_x = 0; sample_x < prototype_terrain_cell_count;
         ++sample_x) {
      if (!terrainTriangleHasSupportedSlope(terrain, sample_x, sample_z, true) ||
          !terrainTriangleHasSupportedSlope(terrain, sample_x, sample_z, false)) {
        return false;
      }
    }
  }
  return true;
}

bool prototypeLevelIsValid(const PrototypeLevel& level) noexcept {
  if (!prototypeTerrainIsValid(level.terrain()) || level.solids().empty() ||
      !prototypeEnvironmentLightIsValid(level.environmentLight())) {
    return false;
  }
  const bool solids_are_valid = std::all_of(
      level.solids().begin(), level.solids().end(), prototypeSolidIsValid);
  const std::size_t plate_count = static_cast<std::size_t>(std::count_if(
      level.solids().begin(), level.solids().end(), [](const auto& solid) {
        return solid.kind == PrototypeSolidKind::ShootingTarget &&
               solid.surface == PrototypeSurface::ShootingTarget;
      }));
  const PrototypePlayerSpawn& spawn = level.playerSpawn();
  return solids_are_valid && plate_count == prototype_plate_count &&
         prototypeTerrainContains(level.terrain(), spawn.foot_position.x,
                                  spawn.foot_position.z) &&
         std::abs(spawn.foot_position.y -
                  prototypeTerrainHeightAt(level.terrain(),
                                           spawn.foot_position.x,
                                           spawn.foot_position.z)) < 0.0001F &&
         prototypeSpawnIsClear(level, 0.35F, 1.80F);
}

bool prototypeSpawnIsClear(const PrototypeLevel& level, float player_radius,
                           float player_height) noexcept {
  if (!(player_radius > 0.0F) || !(player_height > 0.0F)) {
    return false;
  }
  const WorldPosition spawn = level.playerSpawn().foot_position;
  const float player_min_y = spawn.y;
  const float player_max_y = spawn.y + player_height;
  for (const PrototypeSolid& solid : level.solids()) {
    const float solid_min_x = solid.center.x - solid.half_extent.x;
    const float solid_max_x = solid.center.x + solid.half_extent.x;
    const float solid_min_y = solid.center.y - solid.half_extent.y;
    const float solid_max_y = solid.center.y + solid.half_extent.y;
    const float solid_min_z = solid.center.z - solid.half_extent.z;
    const float solid_max_z = solid.center.z + solid.half_extent.z;
    if (overlaps(spawn.x - player_radius, spawn.x + player_radius, solid_min_x,
                 solid_max_x) &&
        overlaps(player_min_y, player_max_y, solid_min_y, solid_max_y) &&
        overlaps(spawn.z - player_radius, spawn.z + player_radius, solid_min_z,
                 solid_max_z)) {
      return false;
    }
  }
  return true;
}
