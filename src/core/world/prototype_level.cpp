#include "core/world/prototype_level.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr WorldColor floor_color{86, 91, 101, 255};
constexpr WorldColor boundary_color{55, 78, 122, 255};
constexpr WorldColor red{205, 63, 73, 255};
constexpr WorldColor green{66, 176, 111, 255};
constexpr WorldColor gold{225, 167, 62, 255};
constexpr WorldColor violet{139, 91, 196, 255};
constexpr WorldColor step_color{70, 184, 190, 255};
constexpr WorldColor passage_color{190, 118, 197, 255};
constexpr WorldColor target_color{222, 122, 58, 255};

bool overlaps(float first_min, float first_max, float second_min,
              float second_max) noexcept {
  return first_min < second_max && first_max > second_min;
}
}  // namespace

PrototypeLevel::PrototypeLevel()
    : solids_{
          {{0.0F, -0.25F, -2.0F}, {10.0F, 0.25F, 12.0F}, floor_color,
           PrototypeSolidKind::Floor, PrototypeSurface::Floor},
          {{-10.25F, 2.5F, -2.0F}, {0.25F, 2.5F, 12.0F}, boundary_color,
           PrototypeSolidKind::Boundary, PrototypeSurface::Boundary},
          {{10.25F, 2.5F, -2.0F}, {0.25F, 2.5F, 12.0F}, boundary_color,
           PrototypeSolidKind::Boundary, PrototypeSurface::Boundary},
          {{0.0F, 2.5F, -14.25F}, {10.5F, 2.5F, 0.25F}, boundary_color,
           PrototypeSolidKind::Boundary, PrototypeSurface::Boundary},
          {{0.0F, 2.5F, 10.25F}, {10.5F, 2.5F, 0.25F}, boundary_color,
           PrototypeSolidKind::Boundary, PrototypeSurface::Boundary},
          {{0.0F, 1.2F, 0.1F}, {1.2F, 1.2F, 0.9F}, red,
           PrototypeSolidKind::Obstacle, PrototypeSurface::Obstacle},
          {{0.0F, 1.5F, -5.5F}, {1.5F, 1.5F, 1.0F}, green,
           PrototypeSolidKind::Obstacle, PrototypeSurface::Obstacle},
          {{-4.5F, 2.0F, -3.5F}, {1.0F, 2.0F, 1.0F}, gold,
           PrototypeSolidKind::Obstacle, PrototypeSurface::Obstacle},
          {{4.5F, 1.0F, -8.5F}, {1.0F, 1.0F, 1.0F}, violet,
           PrototypeSolidKind::Obstacle, PrototypeSurface::Obstacle},
          {{-6.5F, 0.15F, 4.0F}, {1.5F, 0.15F, 1.5F}, step_color,
           PrototypeSolidKind::WalkableStep, PrototypeSurface::Obstacle},
          {{6.0F, 1.55F, 1.5F}, {1.75F, 0.15F, 3.0F}, passage_color,
           PrototypeSolidKind::LowClearance, PrototypeSurface::Obstacle},
          {{-9.0F, 1.75F, -7.0F}, {0.75F, 0.75F, 0.12F}, target_color,
           PrototypeSolidKind::ShootingTarget,
           PrototypeSurface::ShootingTarget},
          {{0.0F, 4.0F, -12.5F}, {0.75F, 0.75F, 0.12F}, target_color,
           PrototypeSolidKind::ShootingTarget,
           PrototypeSurface::ShootingTarget},
          {{9.0F, 1.75F, -7.0F}, {0.75F, 0.75F, 0.12F}, target_color,
           PrototypeSolidKind::ShootingTarget,
           PrototypeSurface::ShootingTarget},
      },
      player_spawn_{{0.0F, 0.05F, 7.0F}, -90.0F},
      environment_light_{
          {{{{0.0F, 2.4F, 6.0F}, {0.30F, 0.50F, 0.90F}, 0.65F, 4.0F},
            {{0.0F, 4.0F, -9.0F}, {1.00F, 0.48F, 0.20F}, 0.95F, 10.0F}}},
          0.03F},
      target_descriptions_{{{11}, {12}, {13}}},
      target_starting_health_(100) {}

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

bool prototypeLevelIsValid(const PrototypeLevel& level) noexcept {
  if (level.solids().empty() ||
      !prototypeEnvironmentLightIsValid(level.environmentLight()) ||
      !prototypeTargetDescriptionsAreValid(
          level.solids(), level.targetDescriptions(),
          level.targetStartingHealth())) {
    return false;
  }
  return std::all_of(level.solids().begin(), level.solids().end(),
                     prototypeSolidIsValid);
}

bool prototypeTargetDescriptionsAreValid(
    const std::vector<PrototypeSolid>& solids,
    std::span<const PrototypeTargetDescription> target_descriptions,
    int target_starting_health) noexcept {
  if (target_descriptions.size() != prototype_target_count ||
      target_starting_health <= 0 ||
      solids.size() > prototype_solid_mask_bit_count) {
    return false;
  }
  std::array<bool, prototype_solid_mask_bit_count> described{};
  for (const PrototypeTargetDescription& target : target_descriptions) {
    if (target.solid_index >= solids.size() ||
        target.solid_index >= prototype_solid_mask_bit_count ||
        described[target.solid_index] ||
        solids[target.solid_index].kind != PrototypeSolidKind::ShootingTarget) {
      return false;
    }
    described[target.solid_index] = true;
  }
  return true;
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
    if (overlaps(spawn.x - player_radius, spawn.x + player_radius,
                 solid_min_x, solid_max_x) &&
        overlaps(player_min_y, player_max_y, solid_min_y, solid_max_y) &&
        overlaps(spawn.z - player_radius, spawn.z + player_radius,
                 solid_min_z, solid_max_z)) {
      return false;
    }
  }
  return true;
}
