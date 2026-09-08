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
  [[nodiscard]] const std::optional<PrototypeTerrain>& terrain()
      const noexcept {
    return terrain_;
  }
  [[nodiscard]] const std::vector<LevelEntry>& entries() const noexcept {
    return entries_;
  }
  [[nodiscard]] const std::string& defaultEntryId() const noexcept {
    return default_entry_;
  }
  [[nodiscard]] const LevelEntry* entry(std::string_view id) const noexcept;
  [[nodiscard]] const PrototypePlayerSpawn& playerSpawn() const noexcept {
    return entry(default_entry_)->pose;
  }
  [[nodiscard]] const PrototypeEnvironmentLight& environmentLight()
      const noexcept {
    return environment_light_;
  }
  [[nodiscard]] const std::vector<PrototypeStaticProp>& props() const noexcept {
    return props_;
  }
  [[nodiscard]] const std::vector<PrototypeLightSwitch>& lightSwitches()
      const noexcept {
    return light_switches_;
  }
  [[nodiscard]] const std::vector<DoorDefinition>& doors() const noexcept {
    return doors_;
  }
  [[nodiscard]] const LevelAudio& audio() const noexcept { return audio_; }
  [[nodiscard]] const LevelCharacters& characters() const noexcept {
    return characters_;
  }

 private:
  explicit PrototypeLevel(LevelDocument document);

  friend PrototypeLevel makePrototypeLevel(const LevelDocument& document);

  std::optional<PrototypeTerrain> terrain_;
  std::vector<PrototypeSolid> solids_;
  std::vector<LevelEntry> entries_;
  std::string default_entry_;
  PrototypeEnvironmentLight environment_light_;
  std::vector<PrototypeStaticProp> props_;
  std::vector<PrototypeLightSwitch> light_switches_;
  std::vector<DoorDefinition> doors_;
  LevelAudio audio_;
  LevelCharacters characters_;
};

[[nodiscard]] PrototypeLevel makePrototypeLevel(const LevelDocument& document);
[[nodiscard]] PrototypeLevel loadPrototypeLevel(
    const std::filesystem::path& path);

[[nodiscard]] bool prototypeEnvironmentLightIsValid(
    const PrototypeEnvironmentLight& light);
[[nodiscard]] std::string pointLightFieldError(const PrototypePointLight& light,
                                               std::string* field = nullptr);
[[nodiscard]] bool prototypeSurfaceIsValid(PrototypeSurface surface) noexcept;
[[nodiscard]] bool prototypeSolidIsValid(const PrototypeSolid& solid) noexcept;
[[nodiscard]] bool prototypeStaticPropIsValid(
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
// Ground feet remain the authored/model anchor. This raises the catalog
// capsule just enough to clear nearby terrain, including triangle seams.
[[nodiscard]] float prototypeActorCapsuleOffset(const PrototypeTerrain* terrain,
                                                WorldPosition feet,
                                                float radius);
[[nodiscard]] bool prototypeLevelIsValid(const PrototypeLevel& level);
[[nodiscard]] bool prototypeSpawnIsClear(const PrototypeLevel& level,
                                         float player_radius,
                                         float player_height) noexcept;

#endif
