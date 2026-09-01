#ifndef CORE_WORLD_PROTOTYPE_LEVEL_HPP
#define CORE_WORLD_PROTOTYPE_LEVEL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

inline constexpr std::size_t prototype_plate_count = 3;
inline constexpr std::size_t prototype_surface_count = 4;
inline constexpr std::size_t prototype_point_light_count = 2;
inline constexpr std::size_t prototype_terrain_sample_count = 97;
inline constexpr std::size_t prototype_terrain_cell_count =
    prototype_terrain_sample_count - 1;
inline constexpr float prototype_terrain_sample_spacing = 0.5F;
inline constexpr float prototype_terrain_maximum_slope_degrees = 50.0F;
inline constexpr float prototype_maximum_ambient_intensity = 0.20F;

struct WorldPosition {
  float x{};
  float y{};
  float z{};
};

struct WorldExtent {
  float x{};
  float y{};
  float z{};
};

using WorldColor = std::array<std::uint8_t, 4>;

enum class PrototypeSolidKind {
  Floor,
  Boundary,
  Obstacle,
  WalkableStep,
  LowClearance,
  ShootingTarget,
};

enum class PrototypeSurface : std::uint32_t {
  Floor = 0,
  Boundary = 1,
  Obstacle = 2,
  ShootingTarget = 3,
};

struct PrototypeSolid {
  WorldPosition center{};
  WorldExtent half_extent{};
  WorldColor color{};
  PrototypeSolidKind kind{PrototypeSolidKind::Obstacle};
  PrototypeSurface surface{PrototypeSurface::Obstacle};
};

struct PrototypeTerrain {
  WorldPosition origin{};
  float sample_spacing{};
  std::array<float, prototype_terrain_sample_count * prototype_terrain_sample_count>
      heights{};
};

struct PrototypePlayerSpawn {
  WorldPosition foot_position{};
  float yaw_degrees{};
};

struct PrototypePointLight {
  WorldPosition position{};
  std::array<float, 3> color{};
  float intensity{};
  float radius{};
};

struct PrototypeEnvironmentLight {
  std::array<PrototypePointLight, prototype_point_light_count> point_lights{};
  float ambient_intensity{};
};

class PrototypeLevel {
 public:
  PrototypeLevel();

  [[nodiscard]] const std::vector<PrototypeSolid>& solids() const noexcept {
    return solids_;
  }
  [[nodiscard]] const PrototypeTerrain& terrain() const noexcept {
    return terrain_;
  }
  [[nodiscard]] const PrototypePlayerSpawn& playerSpawn() const noexcept {
    return player_spawn_;
  }
  [[nodiscard]] const PrototypeEnvironmentLight& environmentLight()
      const noexcept {
    return environment_light_;
  }

 private:
  PrototypeTerrain terrain_;
  std::vector<PrototypeSolid> solids_;
  PrototypePlayerSpawn player_spawn_;
  PrototypeEnvironmentLight environment_light_;
};

[[nodiscard]] bool prototypeEnvironmentLightIsValid(
    const PrototypeEnvironmentLight& light) noexcept;
[[nodiscard]] bool prototypeSurfaceIsValid(PrototypeSurface surface) noexcept;
[[nodiscard]] bool prototypeSolidIsValid(const PrototypeSolid& solid) noexcept;
[[nodiscard]] bool prototypeTerrainIsValid(
    const PrototypeTerrain& terrain) noexcept;
[[nodiscard]] bool prototypeTerrainContains(const PrototypeTerrain& terrain,
                                            float x, float z) noexcept;
[[nodiscard]] float prototypeTerrainHeightAt(const PrototypeTerrain& terrain,
                                             float x, float z) noexcept;
[[nodiscard]] WorldPosition prototypeTerrainSamplePosition(
    const PrototypeTerrain& terrain, std::size_t sample_x,
    std::size_t sample_z) noexcept;
[[nodiscard]] float prototypeTerrainMinimumHeight(
    const PrototypeTerrain& terrain) noexcept;
[[nodiscard]] bool prototypeLevelIsValid(const PrototypeLevel& level) noexcept;
[[nodiscard]] bool prototypeSpawnIsClear(const PrototypeLevel& level,
                                         float player_radius,
                                         float player_height) noexcept;

#endif
