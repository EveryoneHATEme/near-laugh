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

bool overlaps(float first_min, float first_max, float second_min,
              float second_max) noexcept {
  return first_min < second_max && first_max > second_min;
}
}  // namespace

PrototypeLevel::PrototypeLevel()
    : solids_{
          {{0.0F, -0.25F, -2.0F}, {10.0F, 0.25F, 12.0F}, floor_color,
           PrototypeSolidKind::Floor},
          {{-10.25F, 2.5F, -2.0F}, {0.25F, 2.5F, 12.0F}, boundary_color,
           PrototypeSolidKind::Boundary},
          {{10.25F, 2.5F, -2.0F}, {0.25F, 2.5F, 12.0F}, boundary_color,
           PrototypeSolidKind::Boundary},
          {{0.0F, 2.5F, -14.25F}, {10.5F, 2.5F, 0.25F}, boundary_color,
           PrototypeSolidKind::Boundary},
          {{0.0F, 2.5F, 10.25F}, {10.5F, 2.5F, 0.25F}, boundary_color,
           PrototypeSolidKind::Boundary},
          {{0.0F, 1.2F, 0.1F}, {1.2F, 1.2F, 0.9F}, red,
           PrototypeSolidKind::Obstacle},
          {{0.0F, 1.5F, -5.5F}, {1.5F, 1.5F, 1.0F}, green,
           PrototypeSolidKind::Obstacle},
          {{-4.5F, 2.0F, -3.5F}, {1.0F, 2.0F, 1.0F}, gold,
           PrototypeSolidKind::Obstacle},
          {{4.5F, 1.0F, -8.5F}, {1.0F, 1.0F, 1.0F}, violet,
           PrototypeSolidKind::Obstacle},
          {{-6.5F, 0.15F, 4.0F}, {1.5F, 0.15F, 1.5F}, step_color,
           PrototypeSolidKind::WalkableStep},
          {{6.0F, 1.55F, 1.5F}, {1.75F, 0.15F, 3.0F}, passage_color,
           PrototypeSolidKind::LowClearance},
      },
      player_spawn_{{0.0F, 0.05F, 7.0F}, -90.0F} {}

bool prototypeLevelIsValid(const PrototypeLevel& level) noexcept {
  if (level.solids().empty()) {
    return false;
  }
  return std::all_of(level.solids().begin(), level.solids().end(),
                     [](const PrototypeSolid& solid) {
                       return std::isfinite(solid.center.x) &&
                              std::isfinite(solid.center.y) &&
                              std::isfinite(solid.center.z) &&
                              std::isfinite(solid.half_extent.x) &&
                              std::isfinite(solid.half_extent.y) &&
                              std::isfinite(solid.half_extent.z) &&
                              solid.half_extent.x > 0.0F &&
                              solid.half_extent.y > 0.0F &&
                              solid.half_extent.z > 0.0F;
                     });
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
