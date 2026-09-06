#ifndef CORE_WORLD_LEVEL_DOCUMENT_HPP
#define CORE_WORLD_LEVEL_DOCUMENT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

inline constexpr std::uint32_t level_format_version = 6;
inline constexpr std::size_t level_maximum_door_count = 32;
inline constexpr std::size_t prototype_surface_count = 3;
inline constexpr std::size_t prototype_point_light_count = 2;
inline constexpr std::size_t prototype_terrain_sample_count = 97;
inline constexpr std::size_t prototype_terrain_cell_count =
    prototype_terrain_sample_count - 1;
inline constexpr std::size_t level_maximum_solid_count = 240;
inline constexpr std::size_t level_maximum_entry_count = 16;
inline constexpr std::size_t level_maximum_entry_id_length = 64;
inline constexpr float prototype_terrain_sample_spacing = 0.5F;
inline constexpr float prototype_terrain_maximum_slope_degrees = 50.0F;
inline constexpr float prototype_maximum_ambient_intensity = 0.20F;
inline constexpr float prototype_spawn_validation_radius = 0.35F;
inline constexpr float prototype_spawn_validation_height = 1.80F;
// Character collision skin, shared by authored door clearance and physics.
inline constexpr float prototype_player_contact_padding = 0.02F;

struct WorldPosition {
  bool operator==(const WorldPosition&) const = default;
  float x{};
  float y{};
  float z{};
};

struct WorldExtent {
  bool operator==(const WorldExtent&) const = default;
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
};

enum class PrototypeSurface : std::uint32_t {
  Floor = 0,
  Boundary = 1,
  Obstacle = 2,
};

struct PrototypeSolid {
  bool operator==(const PrototypeSolid&) const = default;
  WorldPosition center{};
  WorldExtent half_extent{};
  WorldColor color{};
  PrototypeSolidKind kind{PrototypeSolidKind::Obstacle};
  std::string material{"prototype-obstacle"};
};

struct PrototypeTerrain {
  bool operator==(const PrototypeTerrain&) const = default;
  WorldPosition origin{};
  float sample_spacing{};
  std::array<float,
             prototype_terrain_sample_count * prototype_terrain_sample_count>
      heights{};
  std::string material{"prototype-floor"};
};

struct PrototypePlayerSpawn {
  bool operator==(const PrototypePlayerSpawn&) const = default;
  WorldPosition foot_position{};
  float yaw_degrees{};
};

struct LevelEntry {
  bool operator==(const LevelEntry&) const = default;
  std::string id{};
  PrototypePlayerSpawn pose{};
};

struct PrototypePointLight {
  bool operator==(const PrototypePointLight&) const = default;
  WorldPosition position{};
  std::array<float, 3> color{};
  float intensity{};
  float radius{};
};

struct PrototypeEnvironmentLight {
  bool operator==(const PrototypeEnvironmentLight&) const = default;
  std::array<PrototypePointLight, prototype_point_light_count> point_lights{};
  float ambient_intensity{};
};

inline constexpr std::size_t level_maximum_prop_count = 128;
inline constexpr std::size_t level_maximum_prop_box_count = 8;

struct PropCollisionBox {
  bool operator==(const PropCollisionBox&) const = default;
  WorldPosition center{};
  WorldExtent half_extent{};
};

struct PrototypeStaticProp {
  bool operator==(const PrototypeStaticProp&) const = default;
  std::string id{};
  std::string model{};
  WorldPosition translation{};
  float yaw_degrees{};
  float uniform_scale{1.0F};
  std::vector<PropCollisionBox> collision_boxes{};
};

struct PrototypeLightSwitch {
  bool operator==(const PrototypeLightSwitch&) const = default;
  WorldPosition position{};
  float yaw_degrees{};
  std::uint32_t point_light_index{};
  bool initially_on{true};
};

enum class DoorLockSide { None, PositiveZ, NegativeZ };

struct DoorDefinition {
  bool operator==(const DoorDefinition&) const = default;
  std::string id{};
  WorldPosition hinge_position{};
  float closed_yaw_degrees{};
  float width{0.9F};
  float height{2.0F};
  float thickness{0.06F};
  float open_angle_degrees{90.0F};
  float speed_degrees_per_second{90.0F};
  DoorLockSide lock_side{DoorLockSide::PositiveZ};
  bool initially_open{};
  bool initially_locked{};
};

struct LevelDocument {
  bool operator==(const LevelDocument&) const = default;
  std::uint32_t version{level_format_version};
  std::optional<PrototypeTerrain> terrain{};
  std::vector<PrototypeSolid> solids{};
  std::vector<LevelEntry> entries{};
  std::string default_entry{};
  PrototypeEnvironmentLight environment_light{};
  std::vector<PrototypeStaticProp> props{};
  std::optional<PrototypeLightSwitch> light_switch{};
  std::vector<DoorDefinition> doors{};
};

enum class LevelDiagnosticCategory {
  Parse,
  Validation,
  Filesystem,
};

struct TerrainDiagnosticLocation {
  bool operator==(const TerrainDiagnosticLocation&) const = default;
  std::size_t x{};
  std::size_t z{};
  // Absent for a sample diagnostic; 0 = p00,p01,p11; 1 = p00,p11,p10.
  std::optional<unsigned> triangle{};
};

struct LevelDiagnostic {
  LevelDiagnosticCategory category{LevelDiagnosticCategory::Validation};
  std::filesystem::path source_path{};
  std::string document_path{};
  std::string message{};
  std::optional<TerrainDiagnosticLocation> terrain_location{};
};

struct LevelDocumentLoadResult {
  std::optional<LevelDocument> document{};
  std::vector<LevelDiagnostic> diagnostics{};
  std::uint32_t source_version{};

  [[nodiscard]] explicit operator bool() const noexcept {
    return document.has_value() && diagnostics.empty();
  }
};

[[nodiscard]] bool levelEntryIdIsValid(std::string_view id) noexcept;
[[nodiscard]] const LevelEntry* findLevelEntry(const LevelDocument& document,
                                               std::string_view id) noexcept;

struct LevelDocumentSaveResult {
  std::vector<LevelDiagnostic> diagnostics{};

  [[nodiscard]] explicit operator bool() const noexcept {
    return diagnostics.empty();
  }
};

[[nodiscard]] std::vector<LevelDiagnostic> validateLevelDocument(
    const LevelDocument& document,
    const std::filesystem::path& source_path = {});
[[nodiscard]] LevelDocumentLoadResult loadLevelDocument(
    const std::filesystem::path& path);
[[nodiscard]] LevelDocumentSaveResult saveLevelDocument(
    const std::filesystem::path& path, const LevelDocument& document);
[[nodiscard]] std::string formatLevelDiagnostics(
    const std::vector<LevelDiagnostic>& diagnostics);

#endif
