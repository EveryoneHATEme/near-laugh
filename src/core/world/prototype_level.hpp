#ifndef CORE_WORLD_PROTOTYPE_LEVEL_HPP
#define CORE_WORLD_PROTOTYPE_LEVEL_HPP

#include <vector>

#include "core/world/level_document.hpp"

class PrototypeLevel {
 public:
  PrototypeLevel(const PrototypeLevel&) = default;
  PrototypeLevel(PrototypeLevel&&) noexcept = default;
  PrototypeLevel& operator=(const PrototypeLevel&) = delete;
  PrototypeLevel& operator=(PrototypeLevel&&) = delete;

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
  [[nodiscard]] const PrototypeStaticProp& staticProp() const noexcept {
    return static_prop_;
  }

 private:
  explicit PrototypeLevel(LevelDocument document);

  friend PrototypeLevel makePrototypeLevel(const LevelDocument& document);

  PrototypeTerrain terrain_;
  std::vector<PrototypeSolid> solids_;
  PrototypePlayerSpawn player_spawn_;
  PrototypeEnvironmentLight environment_light_;
  PrototypeStaticProp static_prop_;
};

[[nodiscard]] PrototypeLevel makePrototypeLevel(const LevelDocument& document);
[[nodiscard]] PrototypeLevel loadPrototypeLevel(
    const std::filesystem::path& path);

[[nodiscard]] bool prototypeEnvironmentLightIsValid(
    const PrototypeEnvironmentLight& light) noexcept;
[[nodiscard]] bool prototypeSurfaceIsValid(PrototypeSurface surface) noexcept;
[[nodiscard]] bool prototypeSolidIsValid(const PrototypeSolid& solid) noexcept;
[[nodiscard]] bool prototypeStaticPropIsValid(
    const PrototypeStaticProp& prop) noexcept;
[[nodiscard]] WorldPosition prototypeStaticPropProxyWorldCenter(
    const PrototypeStaticProp& prop) noexcept;
[[nodiscard]] WorldExtent prototypeStaticPropProxyWorldHalfExtent(
    const PrototypeStaticProp& prop) noexcept;
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
