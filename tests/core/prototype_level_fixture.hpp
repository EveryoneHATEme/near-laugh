#ifndef TESTS_CORE_PROTOTYPE_LEVEL_FIXTURE_HPP
#define TESTS_CORE_PROTOTYPE_LEVEL_FIXTURE_HPP

#include <cmath>
#include <filesystem>

#include "core/world/prototype_level.hpp"

inline std::filesystem::path packagedPrototypeLevelPath() {
  return std::filesystem::absolute("resources/levels/prototype.level.json")
      .lexically_normal();
}

inline LevelDocument prototypeLevelDocument() {
  constexpr WorldColor boundary_color{55, 78, 122, 255};
  constexpr WorldColor red{205, 63, 73, 255};
  constexpr WorldColor green{66, 176, 111, 255};
  constexpr WorldColor gold{225, 167, 62, 255};
  constexpr WorldColor violet{139, 91, 196, 255};
  constexpr WorldColor step_color{70, 184, 190, 255};
  constexpr WorldColor passage_color{190, 118, 197, 255};

  LevelDocument document{};
  document.terrain.origin = {-24.0F, 0.0F, -26.0F};
  document.terrain.sample_spacing = prototype_terrain_sample_spacing;
  for (std::size_t sample_z = 0; sample_z < prototype_terrain_sample_count;
       ++sample_z) {
    for (std::size_t sample_x = 0; sample_x < prototype_terrain_sample_count;
         ++sample_x) {
      const float x =
          document.terrain.origin.x +
          static_cast<float>(sample_x) * document.terrain.sample_spacing;
      const float z =
          document.terrain.origin.z +
          static_cast<float>(sample_z) * document.terrain.sample_spacing;
      const float hill_x = (x + 15.0F) / 5.0F;
      const float hill_z = (z + 8.0F) / 6.0F;
      const float depression_x = (x - 14.0F) / 4.5F;
      const float depression_z = (z + 11.0F) / 5.0F;
      document.terrain
          .heights[sample_z * prototype_terrain_sample_count + sample_x] =
          0.75F * std::exp(-(hill_x * hill_x + hill_z * hill_z)) -
          0.55F * std::exp(-(depression_x * depression_x +
                             depression_z * depression_z));
    }
  }

  document.solids = {
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
  };

  document.player_spawn = {
      {0.0F, prototypeTerrainHeightAt(document.terrain, 0.0F, 7.0F), 7.0F},
      -90.0F};
  document.environment_light = {
      {{{{0.0F, 2.4F, 6.0F}, {0.30F, 0.50F, 0.90F}, 0.65F, 4.0F},
        {{0.0F, 4.0F, -9.0F}, {1.00F, 0.48F, 0.20F}, 0.95F, 10.0F}}},
      0.12F};
  document.static_prop = {
      {3.0F, prototypeTerrainHeightAt(document.terrain, 3.0F, -2.0F), -2.0F},
      -25.0F,
      1.0F,
      PrototypeSurface::Obstacle,
      {0.0F, 0.91F, 0.0F},
      {0.55F, 0.91F, 0.48F}};
  return document;
}

inline PrototypeLevel loadPackagedPrototypeLevel() {
  return loadPrototypeLevel(packagedPrototypeLevelPath());
}

#endif
