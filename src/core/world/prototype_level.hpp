#ifndef CORE_WORLD_PROTOTYPE_LEVEL_HPP
#define CORE_WORLD_PROTOTYPE_LEVEL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

inline constexpr std::size_t prototype_target_count = 3;
inline constexpr std::size_t prototype_solid_mask_bit_count = 32;
inline constexpr std::size_t prototype_surface_count = 4;
inline constexpr std::size_t prototype_point_light_count = 2;
inline constexpr float prototype_maximum_ambient_intensity = 0.08F;

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

struct PrototypeTargetDescription {
  std::size_t solid_index{};
};

class PrototypeLevel {
 public:
  PrototypeLevel();

  [[nodiscard]] const std::vector<PrototypeSolid>& solids() const noexcept {
    return solids_;
  }
  [[nodiscard]] const PrototypePlayerSpawn& playerSpawn() const noexcept {
    return player_spawn_;
  }
  [[nodiscard]] const PrototypeEnvironmentLight& environmentLight()
      const noexcept {
    return environment_light_;
  }
  [[nodiscard]] const std::array<PrototypeTargetDescription,
                                 prototype_target_count>&
  targetDescriptions() const noexcept {
    return target_descriptions_;
  }
  [[nodiscard]] int targetStartingHealth() const noexcept {
    return target_starting_health_;
  }

 private:
  std::vector<PrototypeSolid> solids_;
  PrototypePlayerSpawn player_spawn_;
  PrototypeEnvironmentLight environment_light_;
  std::array<PrototypeTargetDescription, prototype_target_count>
      target_descriptions_{};
  int target_starting_health_{};
};

[[nodiscard]] bool prototypeEnvironmentLightIsValid(
    const PrototypeEnvironmentLight& light) noexcept;
[[nodiscard]] bool prototypeSurfaceIsValid(PrototypeSurface surface) noexcept;
[[nodiscard]] bool prototypeSolidIsValid(const PrototypeSolid& solid) noexcept;
[[nodiscard]] bool prototypeLevelIsValid(const PrototypeLevel& level) noexcept;
[[nodiscard]] bool prototypeTargetDescriptionsAreValid(
    const std::vector<PrototypeSolid>& solids,
    std::span<const PrototypeTargetDescription> target_descriptions,
    int target_starting_health) noexcept;
[[nodiscard]] bool prototypeSpawnIsClear(const PrototypeLevel& level,
                                         float player_radius,
                                         float player_height) noexcept;

#endif
