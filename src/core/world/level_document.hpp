#ifndef CORE_WORLD_LEVEL_DOCUMENT_HPP
#define CORE_WORLD_LEVEL_DOCUMENT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

inline constexpr std::uint32_t level_format_version = 2;
inline constexpr std::size_t prototype_surface_count = 3;
inline constexpr std::size_t prototype_point_light_count = 2;
inline constexpr std::size_t prototype_terrain_sample_count = 97;
inline constexpr std::size_t prototype_terrain_cell_count =
    prototype_terrain_sample_count - 1;
inline constexpr std::size_t level_maximum_solid_count = 240;
inline constexpr float prototype_terrain_sample_spacing = 0.5F;
inline constexpr float prototype_terrain_maximum_slope_degrees = 50.0F;
inline constexpr float prototype_maximum_ambient_intensity = 0.20F;
inline constexpr float prototype_spawn_validation_radius = 0.35F;
inline constexpr float prototype_spawn_validation_height = 1.80F;

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
  PrototypeSurface surface{PrototypeSurface::Obstacle};
};

struct PrototypeTerrain {
  bool operator==(const PrototypeTerrain&) const = default;
  WorldPosition origin{};
  float sample_spacing{};
  std::array<float,
             prototype_terrain_sample_count * prototype_terrain_sample_count>
      heights{};
};

struct PrototypePlayerSpawn {
  bool operator==(const PrototypePlayerSpawn&) const = default;
  WorldPosition foot_position{};
  float yaw_degrees{};
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

struct PrototypeStaticProp {
  bool operator==(const PrototypeStaticProp&) const = default;
  WorldPosition translation{};
  float yaw_degrees{};
  float uniform_scale{1.0F};
  PrototypeSurface surface{PrototypeSurface::Obstacle};
  WorldPosition box_proxy_center{};
  WorldExtent box_proxy_half_extent{};
};

struct LevelDocument {
  bool operator==(const LevelDocument&) const = default;
  std::uint32_t version{level_format_version};
  PrototypeTerrain terrain{};
  std::vector<PrototypeSolid> solids{};
  PrototypePlayerSpawn player_spawn{};
  PrototypeEnvironmentLight environment_light{};
  PrototypeStaticProp static_prop{};
};

enum class LevelDiagnosticCategory {
  Parse,
  Validation,
  Filesystem,
};

struct LevelDiagnostic {
  LevelDiagnosticCategory category{LevelDiagnosticCategory::Validation};
  std::filesystem::path source_path{};
  std::string document_path{};
  std::string message{};
};

struct LevelDocumentLoadResult {
  std::optional<LevelDocument> document{};
  std::vector<LevelDiagnostic> diagnostics{};

  [[nodiscard]] explicit operator bool() const noexcept {
    return document.has_value() && diagnostics.empty();
  }
};

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
