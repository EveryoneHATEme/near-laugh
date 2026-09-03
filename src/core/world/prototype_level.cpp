#include "core/world/prototype_level.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
std::size_t terrainSampleIndex(std::size_t sample_x,
                               std::size_t sample_z) noexcept {
  return sample_z * prototype_terrain_sample_count + sample_x;
}

float terrainHeight(const PrototypeTerrain& terrain, std::size_t sample_x,
                    std::size_t sample_z) noexcept {
  return terrain.origin.y +
         terrain.heights[terrainSampleIndex(sample_x, sample_z)];
}

bool terrainTriangleHasSupportedSlope(const PrototypeTerrain& terrain,
                                      std::size_t sample_x,
                                      std::size_t sample_z,
                                      bool first) noexcept {
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
  const float normal_length = std::sqrt(
      normal_x * normal_x + normal_y * normal_y + normal_z * normal_z);
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

bool solidKindIsValid(PrototypeSolidKind kind) noexcept {
  switch (kind) {
    case PrototypeSolidKind::Floor:
    case PrototypeSolidKind::Boundary:
    case PrototypeSolidKind::Obstacle:
    case PrototypeSolidKind::WalkableStep:
    case PrototypeSolidKind::LowClearance:
      return true;
  }
  return false;
}

PrototypeSurface expectedSurface(PrototypeSolidKind kind) noexcept {
  switch (kind) {
    case PrototypeSolidKind::Floor:
      return PrototypeSurface::Floor;
    case PrototypeSolidKind::Boundary:
      return PrototypeSurface::Boundary;
    case PrototypeSolidKind::Obstacle:
    case PrototypeSolidKind::WalkableStep:
    case PrototypeSolidKind::LowClearance:
      return PrototypeSurface::Obstacle;
  }
  return static_cast<PrototypeSurface>(prototype_surface_count);
}

void addValidation(std::vector<LevelDiagnostic>& diagnostics,
                   const std::filesystem::path& source_path,
                   std::string document_path, std::string message) {
  diagnostics.push_back({LevelDiagnosticCategory::Validation, source_path,
                         std::move(document_path), std::move(message)});
}
}  // namespace

PrototypeLevel::PrototypeLevel(LevelDocument document)
    : terrain_(std::move(document.terrain)),
      solids_(std::move(document.solids)),
      player_spawn_(std::move(document.player_spawn)),
      environment_light_(std::move(document.environment_light)),
      static_prop_(std::move(document.static_prop)) {}

PrototypeLevel makePrototypeLevel(const LevelDocument& document) {
  const std::vector<LevelDiagnostic> diagnostics =
      validateLevelDocument(document);
  if (!diagnostics.empty()) {
    throw std::invalid_argument(formatLevelDiagnostics(diagnostics));
  }
  return PrototypeLevel(document);
}

PrototypeLevel loadPrototypeLevel(const std::filesystem::path& path) {
  LevelDocumentLoadResult result = loadLevelDocument(path);
  if (!result) {
    throw std::runtime_error(formatLevelDiagnostics(result.diagnostics));
  }
  const std::vector<LevelDiagnostic> diagnostics = validateLevelDocument(
      *result.document, std::filesystem::absolute(path).lexically_normal());
  if (!diagnostics.empty()) {
    throw std::runtime_error(formatLevelDiagnostics(diagnostics));
  }
  return makePrototypeLevel(*result.document);
}

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
         solidKindIsValid(solid.kind) &&
         prototypeSurfaceIsValid(solid.surface) &&
         solid.surface == expectedSurface(solid.kind);
}

WorldPosition prototypeStaticPropProxyWorldCenter(
    const PrototypeStaticProp& prop) noexcept {
  const float yaw = prop.yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  const float cosine = std::cos(yaw);
  const float sine = std::sin(yaw);
  const float local_x = prop.box_proxy_center.x * prop.uniform_scale;
  const float local_y = prop.box_proxy_center.y * prop.uniform_scale;
  const float local_z = prop.box_proxy_center.z * prop.uniform_scale;
  return {prop.translation.x + cosine * local_x + sine * local_z,
          prop.translation.y + local_y,
          prop.translation.z - sine * local_x + cosine * local_z};
}

WorldExtent prototypeStaticPropProxyWorldHalfExtent(
    const PrototypeStaticProp& prop) noexcept {
  return {prop.box_proxy_half_extent.x * prop.uniform_scale,
          prop.box_proxy_half_extent.y * prop.uniform_scale,
          prop.box_proxy_half_extent.z * prop.uniform_scale};
}

bool prototypeStaticPropIsValid(const PrototypeStaticProp& prop) noexcept {
  const WorldPosition center = prototypeStaticPropProxyWorldCenter(prop);
  const WorldExtent half_extent = prototypeStaticPropProxyWorldHalfExtent(prop);
  return std::isfinite(prop.translation.x) &&
         std::isfinite(prop.translation.y) &&
         std::isfinite(prop.translation.z) && std::isfinite(prop.yaw_degrees) &&
         std::isfinite(prop.uniform_scale) && prop.uniform_scale > 0.0F &&
         prop.surface == PrototypeSurface::Obstacle &&
         std::isfinite(prop.box_proxy_center.x) &&
         std::isfinite(prop.box_proxy_center.y) &&
         std::isfinite(prop.box_proxy_center.z) && std::isfinite(center.x) &&
         std::isfinite(center.y) && std::isfinite(center.z) &&
         std::isfinite(half_extent.x) && std::isfinite(half_extent.y) &&
         std::isfinite(half_extent.z) && half_extent.x > 0.0F &&
         half_extent.y > 0.0F && half_extent.z > 0.0F;
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
  return {
      terrain.origin.x + static_cast<float>(sample_x) * terrain.sample_spacing,
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
      terrain.origin.x +
      static_cast<float>(prototype_terrain_cell_count) * terrain.sample_spacing;
  const float maximum_z =
      terrain.origin.z +
      static_cast<float>(prototype_terrain_cell_count) * terrain.sample_spacing;
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
  const std::size_t sample_x =
      std::min(static_cast<std::size_t>(std::floor(local_x)),
               prototype_terrain_cell_count - 1);
  const std::size_t sample_z =
      std::min(static_cast<std::size_t>(std::floor(local_z)),
               prototype_terrain_cell_count - 1);
  const float fraction_x =
      std::min(local_x - static_cast<float>(sample_x), 1.0F);
  const float fraction_z =
      std::min(local_z - static_cast<float>(sample_z), 1.0F);
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

std::vector<LevelDiagnostic> validateLevelDocument(
    const LevelDocument& document, const std::filesystem::path& source_path) {
  std::vector<LevelDiagnostic> diagnostics;
  if (document.version != level_format_version) {
    addValidation(diagnostics, source_path, "version",
                  "unsupported level format version");
  }

  const PrototypeTerrain& terrain = document.terrain;
  if (!std::isfinite(terrain.origin.x)) {
    addValidation(diagnostics, source_path, "terrain.origin.x",
                  "must be finite");
  }
  if (!std::isfinite(terrain.origin.y)) {
    addValidation(diagnostics, source_path, "terrain.origin.y",
                  "must be finite");
  }
  if (!std::isfinite(terrain.origin.z)) {
    addValidation(diagnostics, source_path, "terrain.origin.z",
                  "must be finite");
  }
  if (!std::isfinite(terrain.sample_spacing) ||
      terrain.sample_spacing != prototype_terrain_sample_spacing) {
    addValidation(diagnostics, source_path, "terrain.sample_spacing",
                  "must equal the fixed 0.5 metre spacing");
  }
  for (std::size_t index = 0; index < terrain.heights.size(); ++index) {
    if (!std::isfinite(terrain.heights[index])) {
      addValidation(diagnostics, source_path,
                    "terrain.heights[" + std::to_string(index) + "]",
                    "must be finite");
    }
  }
  if (std::isfinite(terrain.sample_spacing) && terrain.sample_spacing > 0.0F &&
      std::all_of(terrain.heights.begin(), terrain.heights.end(),
                  [](float height) { return std::isfinite(height); })) {
    for (std::size_t sample_z = 0; sample_z < prototype_terrain_cell_count;
         ++sample_z) {
      for (std::size_t sample_x = 0; sample_x < prototype_terrain_cell_count;
           ++sample_x) {
        for (const bool first : {true, false}) {
          if (!terrainTriangleHasSupportedSlope(terrain, sample_x, sample_z,
                                                first)) {
            addValidation(diagnostics, source_path,
                          "terrain.cells[" + std::to_string(sample_z) + "][" +
                              std::to_string(sample_x) + "]",
                          first
                              ? "first triangle exceeds the supported slope"
                              : "second triangle exceeds the supported slope");
          }
        }
      }
    }
  }

  if (document.solids.empty()) {
    addValidation(diagnostics, source_path, "solids",
                  "must contain the prototype scene solids");
  }
  if (document.solids.size() > level_maximum_solid_count) {
    addValidation(diagnostics, source_path, "solids",
                  "exceeds the 240-solid limit");
  }
  for (std::size_t index = 0; index < document.solids.size(); ++index) {
    const PrototypeSolid& solid = document.solids[index];
    const std::string path = "solids[" + std::to_string(index) + "]";
    const std::array<std::pair<const char*, float>, 6> finite_values{{
        {"center.x", solid.center.x},
        {"center.y", solid.center.y},
        {"center.z", solid.center.z},
        {"half_extent.x", solid.half_extent.x},
        {"half_extent.y", solid.half_extent.y},
        {"half_extent.z", solid.half_extent.z},
    }};
    for (const auto& [field, value] : finite_values) {
      if (!std::isfinite(value)) {
        addValidation(diagnostics, source_path, path + "." + field,
                      "must be finite");
      }
    }
    if (!(solid.half_extent.x > 0.0F) || !(solid.half_extent.y > 0.0F) ||
        !(solid.half_extent.z > 0.0F)) {
      addValidation(diagnostics, source_path, path + ".half_extent",
                    "all components must be positive");
    }
    if (!solidKindIsValid(solid.kind)) {
      addValidation(diagnostics, source_path, path + ".kind",
                    "is not a supported solid kind");
    }
    if (!prototypeSurfaceIsValid(solid.surface)) {
      addValidation(diagnostics, source_path, path + ".surface",
                    "is not a supported surface role");
    } else if (solidKindIsValid(solid.kind) &&
               solid.surface != expectedSurface(solid.kind)) {
      addValidation(diagnostics, source_path, path + ".surface",
                    "does not match the fixed role for this solid kind");
    }
  }

  const PrototypePlayerSpawn& spawn = document.player_spawn;
  if (!std::isfinite(spawn.foot_position.x) ||
      !std::isfinite(spawn.foot_position.y) ||
      !std::isfinite(spawn.foot_position.z)) {
    addValidation(diagnostics, source_path, "player_spawn.foot_position",
                  "all components must be finite");
  }
  if (!std::isfinite(spawn.yaw_degrees)) {
    addValidation(diagnostics, source_path, "player_spawn.yaw_degrees",
                  "must be finite");
  }
  const bool spawn_in_terrain = prototypeTerrainContains(
      terrain, spawn.foot_position.x, spawn.foot_position.z);
  if (!spawn_in_terrain) {
    addValidation(diagnostics, source_path, "player_spawn.foot_position",
                  "must lie within the terrain");
  } else {
    const float support = prototypeTerrainHeightAt(
        terrain, spawn.foot_position.x, spawn.foot_position.z);
    if (!std::isfinite(spawn.foot_position.y) ||
        std::abs(spawn.foot_position.y - support) >= 0.0001F) {
      addValidation(diagnostics, source_path, "player_spawn.foot_position.y",
                    "must be supported by the terrain surface");
    }
  }

  const PrototypeEnvironmentLight& light = document.environment_light;
  for (std::size_t index = 0; index < light.point_lights.size(); ++index) {
    const PrototypePointLight& point = light.point_lights[index];
    const std::string path =
        "environment_light.point_lights[" + std::to_string(index) + "]";
    if (!std::isfinite(point.position.x) || !std::isfinite(point.position.y) ||
        !std::isfinite(point.position.z)) {
      addValidation(diagnostics, source_path, path + ".position",
                    "all components must be finite");
    }
    for (std::size_t component = 0; component < point.color.size();
         ++component) {
      if (!std::isfinite(point.color[component]) ||
          point.color[component] < 0.0F) {
        addValidation(diagnostics, source_path,
                      path + ".color[" + std::to_string(component) + "]",
                      "must be finite and non-negative");
      }
    }
    if (!std::isfinite(point.intensity) || !(point.intensity > 0.0F)) {
      addValidation(diagnostics, source_path, path + ".intensity",
                    "must be finite and positive");
    }
    if (!std::isfinite(point.radius) || !(point.radius > 0.0F)) {
      addValidation(diagnostics, source_path, path + ".radius",
                    "must be finite and positive");
    }
  }
  if (!std::isfinite(light.ambient_intensity) ||
      light.ambient_intensity < 0.0F ||
      light.ambient_intensity > prototype_maximum_ambient_intensity) {
    addValidation(diagnostics, source_path,
                  "environment_light.ambient_intensity",
                  "must be finite and between 0.0 and 0.2");
  }

  const PrototypeStaticProp& prop = document.static_prop;
  if (!std::isfinite(prop.translation.x) ||
      !std::isfinite(prop.translation.y) ||
      !std::isfinite(prop.translation.z)) {
    addValidation(diagnostics, source_path, "static_prop.translation",
                  "all components must be finite");
  }
  if (!std::isfinite(prop.yaw_degrees)) {
    addValidation(diagnostics, source_path, "static_prop.yaw_degrees",
                  "must be finite");
  }
  if (!std::isfinite(prop.uniform_scale) || !(prop.uniform_scale > 0.0F)) {
    addValidation(diagnostics, source_path, "static_prop.uniform_scale",
                  "must be finite and positive");
  }
  if (prop.surface != PrototypeSurface::Obstacle) {
    addValidation(diagnostics, source_path, "static_prop.surface",
                  "must use the fixed obstacle surface");
  }
  if (!std::isfinite(prop.box_proxy_center.x) ||
      !std::isfinite(prop.box_proxy_center.y) ||
      !std::isfinite(prop.box_proxy_center.z)) {
    addValidation(diagnostics, source_path, "static_prop.box_proxy.center",
                  "all components must be finite");
  }
  if (!std::isfinite(prop.box_proxy_half_extent.x) ||
      !std::isfinite(prop.box_proxy_half_extent.y) ||
      !std::isfinite(prop.box_proxy_half_extent.z) ||
      !(prop.box_proxy_half_extent.x > 0.0F) ||
      !(prop.box_proxy_half_extent.y > 0.0F) ||
      !(prop.box_proxy_half_extent.z > 0.0F)) {
    addValidation(diagnostics, source_path, "static_prop.box_proxy.half_extent",
                  "all components must be finite and positive");
  }
  const WorldPosition proxy_center = prototypeStaticPropProxyWorldCenter(prop);
  const WorldExtent proxy_extent =
      prototypeStaticPropProxyWorldHalfExtent(prop);
  if (!std::isfinite(proxy_center.x) || !std::isfinite(proxy_center.y) ||
      !std::isfinite(proxy_center.z) || !std::isfinite(proxy_extent.x) ||
      !std::isfinite(proxy_extent.y) || !std::isfinite(proxy_extent.z)) {
    addValidation(diagnostics, source_path, "static_prop.box_proxy",
                  "world transform must remain finite");
  } else if (!prototypeTerrainContains(terrain, proxy_center.x,
                                       proxy_center.z)) {
    addValidation(diagnostics, source_path, "static_prop.box_proxy.center",
                  "world-space proxy center must lie within the terrain");
  }

  if (spawn_in_terrain && std::isfinite(spawn.foot_position.y)) {
    const float player_min_y = spawn.foot_position.y;
    const float player_max_y =
        spawn.foot_position.y + prototype_spawn_validation_height;
    bool clear = true;
    for (const PrototypeSolid& solid : document.solids) {
      if (overlaps(spawn.foot_position.x - prototype_spawn_validation_radius,
                   spawn.foot_position.x + prototype_spawn_validation_radius,
                   solid.center.x - solid.half_extent.x,
                   solid.center.x + solid.half_extent.x) &&
          overlaps(player_min_y, player_max_y,
                   solid.center.y - solid.half_extent.y,
                   solid.center.y + solid.half_extent.y) &&
          overlaps(spawn.foot_position.z - prototype_spawn_validation_radius,
                   spawn.foot_position.z + prototype_spawn_validation_radius,
                   solid.center.z - solid.half_extent.z,
                   solid.center.z + solid.half_extent.z)) {
        clear = false;
        break;
      }
    }
    if (clear && std::isfinite(proxy_center.x) &&
        std::isfinite(proxy_center.y) && std::isfinite(proxy_center.z) &&
        std::isfinite(proxy_extent.x) && std::isfinite(proxy_extent.y) &&
        std::isfinite(proxy_extent.z)) {
      clear = !(
          overlaps(spawn.foot_position.x - prototype_spawn_validation_radius,
                   spawn.foot_position.x + prototype_spawn_validation_radius,
                   proxy_center.x - proxy_extent.x,
                   proxy_center.x + proxy_extent.x) &&
          overlaps(player_min_y, player_max_y, proxy_center.y - proxy_extent.y,
                   proxy_center.y + proxy_extent.y) &&
          overlaps(spawn.foot_position.z - prototype_spawn_validation_radius,
                   spawn.foot_position.z + prototype_spawn_validation_radius,
                   proxy_center.z - proxy_extent.z,
                   proxy_center.z + proxy_extent.z));
    }
    if (!clear) {
      addValidation(diagnostics, source_path, "player_spawn.foot_position",
                    "standing player clearance overlaps blocking geometry");
    }
  }

  return diagnostics;
}

bool prototypeTerrainIsValid(const PrototypeTerrain& terrain) noexcept {
  if (!std::isfinite(terrain.origin.x) || !std::isfinite(terrain.origin.y) ||
      !std::isfinite(terrain.origin.z) ||
      terrain.sample_spacing != prototype_terrain_sample_spacing ||
      !std::all_of(terrain.heights.begin(), terrain.heights.end(),
                   [](float height) { return std::isfinite(height); })) {
    return false;
  }
  for (std::size_t sample_z = 0; sample_z < prototype_terrain_cell_count;
       ++sample_z) {
    for (std::size_t sample_x = 0; sample_x < prototype_terrain_cell_count;
         ++sample_x) {
      if (!terrainTriangleHasSupportedSlope(terrain, sample_x, sample_z,
                                            true) ||
          !terrainTriangleHasSupportedSlope(terrain, sample_x, sample_z,
                                            false)) {
        return false;
      }
    }
  }
  return true;
}

bool prototypeLevelIsValid(const PrototypeLevel& level) noexcept {
  const LevelDocument document{level_format_version,     level.terrain(),
                               level.solids(),           level.playerSpawn(),
                               level.environmentLight(), level.staticProp()};
  return validateLevelDocument(document).empty();
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
  const PrototypeStaticProp& prop = level.staticProp();
  const WorldPosition prop_center = prototypeStaticPropProxyWorldCenter(prop);
  const WorldExtent prop_half_extent =
      prototypeStaticPropProxyWorldHalfExtent(prop);
  if (overlaps(spawn.x - player_radius, spawn.x + player_radius,
               prop_center.x - prop_half_extent.x,
               prop_center.x + prop_half_extent.x) &&
      overlaps(player_min_y, player_max_y, prop_center.y - prop_half_extent.y,
               prop_center.y + prop_half_extent.y) &&
      overlaps(spawn.z - player_radius, spawn.z + player_radius,
               prop_center.z - prop_half_extent.z,
               prop_center.z + prop_half_extent.z)) {
    return false;
  }
  return true;
}
