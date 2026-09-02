#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <locale>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

#include "core/world/level_document.hpp"

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

namespace {
using Json =
    nlohmann::basic_json<nlohmann::ordered_map, std::vector, std::string, bool,
                         std::int64_t, std::uint64_t, float>;

class ParseFailure final : public std::runtime_error {
 public:
  ParseFailure(std::string document_path, std::string message)
      : std::runtime_error(std::move(message)),
        document_path_(std::move(document_path)) {}

  [[nodiscard]] const std::string& documentPath() const noexcept {
    return document_path_;
  }

 private:
  std::string document_path_;
};

std::filesystem::path normalizedAbsolute(const std::filesystem::path& path) {
  return std::filesystem::absolute(path).lexically_normal();
}

[[noreturn]] void fail(std::string path, std::string message) {
  throw ParseFailure(std::move(path), std::move(message));
}

void requireObjectFields(
    const Json& value, std::string_view path,
    std::initializer_list<std::string_view> required_fields) {
  if (!value.is_object()) {
    fail(std::string(path), "must be an object");
  }
  for (auto member = value.begin(); member != value.end(); ++member) {
    const bool known =
        std::any_of(required_fields.begin(), required_fields.end(),
                    [&member](std::string_view required) {
                      return member.key() == required;
                    });
    if (!known) {
      const std::string prefix =
          path.empty() ? std::string{} : std::string(path) + ".";
      fail(prefix + member.key(), "unknown field");
    }
  }
  for (const std::string_view required : required_fields) {
    if (!value.contains(required)) {
      const std::string prefix =
          path.empty() ? std::string{} : std::string(path) + ".";
      fail(prefix + std::string(required), "required field is missing");
    }
  }
}

float parseFloat(const Json& value, const std::string& path) {
  if (!value.is_number()) {
    fail(path, "must be a number");
  }
  try {
    const float result = value.get<float>();
    if (!std::isfinite(result)) {
      fail(path, "must be a finite float");
    }
    return result;
  } catch (const nlohmann::json::exception&) {
    fail(path, "is outside the supported float range");
  }
}

std::uint32_t parseUnsigned(const Json& value, const std::string& path) {
  if (!value.is_number_unsigned()) {
    fail(path, "must be an unsigned integer");
  }
  try {
    return value.get<std::uint32_t>();
  } catch (const nlohmann::json::exception&) {
    fail(path, "is outside the supported integer range");
  }
}

WorldPosition parsePosition(const Json& value, const std::string& path) {
  requireObjectFields(value, path, {"x", "y", "z"});
  return {parseFloat(value.at("x"), path + ".x"),
          parseFloat(value.at("y"), path + ".y"),
          parseFloat(value.at("z"), path + ".z")};
}

WorldExtent parseExtent(const Json& value, const std::string& path) {
  const WorldPosition parsed = parsePosition(value, path);
  return {parsed.x, parsed.y, parsed.z};
}

template <std::size_t Size>
std::array<float, Size> parseFloatArray(const Json& value,
                                        const std::string& path) {
  if (!value.is_array() || value.size() != Size) {
    fail(path, "must contain exactly " + std::to_string(Size) + " numbers");
  }
  std::array<float, Size> result{};
  for (std::size_t index = 0; index < Size; ++index) {
    result[index] =
        parseFloat(value[index], path + "[" + std::to_string(index) + "]");
  }
  return result;
}

WorldColor parseColor(const Json& value, const std::string& path) {
  if (!value.is_array() || value.size() != 4) {
    fail(path, "must contain exactly four byte components");
  }
  WorldColor color{};
  for (std::size_t index = 0; index < color.size(); ++index) {
    const std::uint32_t component =
        parseUnsigned(value[index], path + "[" + std::to_string(index) + "]");
    if (component > 255) {
      fail(path + "[" + std::to_string(index) + "]",
           "must be between 0 and 255");
    }
    color[index] = static_cast<std::uint8_t>(component);
  }
  return color;
}

std::string parseString(const Json& value, const std::string& path) {
  if (!value.is_string()) {
    fail(path, "must be a string");
  }
  return value.get<std::string>();
}

PrototypeSolidKind parseSolidKind(const Json& value, const std::string& path) {
  const std::string text = parseString(value, path);
  if (text == "floor") return PrototypeSolidKind::Floor;
  if (text == "boundary") return PrototypeSolidKind::Boundary;
  if (text == "obstacle") return PrototypeSolidKind::Obstacle;
  if (text == "walkable_step") return PrototypeSolidKind::WalkableStep;
  if (text == "low_clearance") return PrototypeSolidKind::LowClearance;
  if (text == "shooting_target") return PrototypeSolidKind::ShootingTarget;
  fail(path, "unsupported solid kind '" + text + "'");
}

PrototypeSurface parseSurface(const Json& value, const std::string& path) {
  const std::string text = parseString(value, path);
  if (text == "floor") return PrototypeSurface::Floor;
  if (text == "boundary") return PrototypeSurface::Boundary;
  if (text == "obstacle") return PrototypeSurface::Obstacle;
  if (text == "shooting_target") return PrototypeSurface::ShootingTarget;
  fail(path, "unsupported surface role '" + text + "'");
}

PrototypeTerrain parseTerrain(const Json& value) {
  constexpr std::size_t height_count =
      prototype_terrain_sample_count * prototype_terrain_sample_count;
  requireObjectFields(value, "terrain",
                      {"origin", "sample_spacing", "heights"});
  const Json& heights = value.at("heights");
  if (!heights.is_array() || heights.size() != height_count) {
    fail("terrain.heights", "must contain exactly " +
                                std::to_string(height_count) +
                                " row-major samples");
  }
  PrototypeTerrain terrain{};
  terrain.origin = parsePosition(value.at("origin"), "terrain.origin");
  terrain.sample_spacing =
      parseFloat(value.at("sample_spacing"), "terrain.sample_spacing");
  for (std::size_t index = 0; index < height_count; ++index) {
    terrain.heights[index] = parseFloat(
        heights[index], "terrain.heights[" + std::to_string(index) + "]");
  }
  return terrain;
}

PrototypeSolid parseSolid(const Json& value, std::size_t index) {
  const std::string path = "solids[" + std::to_string(index) + "]";
  requireObjectFields(value, path,
                      {"center", "half_extent", "color", "kind", "surface"});
  return {parsePosition(value.at("center"), path + ".center"),
          parseExtent(value.at("half_extent"), path + ".half_extent"),
          parseColor(value.at("color"), path + ".color"),
          parseSolidKind(value.at("kind"), path + ".kind"),
          parseSurface(value.at("surface"), path + ".surface")};
}

PrototypePlayerSpawn parseSpawn(const Json& value) {
  requireObjectFields(value, "player_spawn", {"foot_position", "yaw_degrees"});
  return {
      parsePosition(value.at("foot_position"), "player_spawn.foot_position"),
      parseFloat(value.at("yaw_degrees"), "player_spawn.yaw_degrees")};
}

PrototypePointLight parsePointLight(const Json& value, std::size_t index) {
  const std::string path =
      "environment_light.point_lights[" + std::to_string(index) + "]";
  requireObjectFields(value, path,
                      {"position", "color", "intensity", "radius"});
  return {parsePosition(value.at("position"), path + ".position"),
          parseFloatArray<3>(value.at("color"), path + ".color"),
          parseFloat(value.at("intensity"), path + ".intensity"),
          parseFloat(value.at("radius"), path + ".radius")};
}

PrototypeEnvironmentLight parseEnvironmentLight(const Json& value) {
  requireObjectFields(value, "environment_light",
                      {"point_lights", "ambient_intensity"});
  const Json& points = value.at("point_lights");
  if (!points.is_array() || points.size() != prototype_point_light_count) {
    fail("environment_light.point_lights",
         "must contain exactly two point lights");
  }
  PrototypeEnvironmentLight result{};
  for (std::size_t index = 0; index < result.point_lights.size(); ++index) {
    result.point_lights[index] = parsePointLight(points[index], index);
  }
  result.ambient_intensity = parseFloat(value.at("ambient_intensity"),
                                        "environment_light.ambient_intensity");
  return result;
}

PrototypeStaticProp parseStaticProp(const Json& value) {
  requireObjectFields(
      value, "static_prop",
      {"translation", "yaw_degrees", "uniform_scale", "surface", "box_proxy"});
  const Json& proxy = value.at("box_proxy");
  requireObjectFields(proxy, "static_prop.box_proxy",
                      {"center", "half_extent"});
  return {parsePosition(value.at("translation"), "static_prop.translation"),
          parseFloat(value.at("yaw_degrees"), "static_prop.yaw_degrees"),
          parseFloat(value.at("uniform_scale"), "static_prop.uniform_scale"),
          parseSurface(value.at("surface"), "static_prop.surface"),
          parsePosition(proxy.at("center"), "static_prop.box_proxy.center"),
          parseExtent(proxy.at("half_extent"),
                      "static_prop.box_proxy.half_extent")};
}

LevelDocument parseDocument(const Json& root) {
  requireObjectFields(root, "",
                      {"version", "terrain", "solids", "player_spawn",
                       "environment_light", "static_prop"});
  const std::uint32_t version = parseUnsigned(root.at("version"), "version");
  if (version != level_format_version) {
    fail("version",
         "unsupported level format version " + std::to_string(version));
  }
  const Json& solids_json = root.at("solids");
  if (!solids_json.is_array()) {
    fail("solids", "must be an array");
  }
  if (solids_json.size() > level_maximum_solid_count) {
    fail("solids", "exceeds the 240-solid limit");
  }
  std::vector<PrototypeSolid> solids;
  solids.reserve(solids_json.size());
  for (std::size_t index = 0; index < solids_json.size(); ++index) {
    solids.push_back(parseSolid(solids_json[index], index));
  }
  return {version,
          parseTerrain(root.at("terrain")),
          std::move(solids),
          parseSpawn(root.at("player_spawn")),
          parseEnvironmentLight(root.at("environment_light")),
          parseStaticProp(root.at("static_prop"))};
}

Json positionJson(const WorldPosition& value) {
  Json result = Json::object();
  result["x"] = value.x;
  result["y"] = value.y;
  result["z"] = value.z;
  return result;
}

Json extentJson(const WorldExtent& value) {
  return positionJson({value.x, value.y, value.z});
}

std::string_view solidKindString(PrototypeSolidKind kind) {
  switch (kind) {
    case PrototypeSolidKind::Floor:
      return "floor";
    case PrototypeSolidKind::Boundary:
      return "boundary";
    case PrototypeSolidKind::Obstacle:
      return "obstacle";
    case PrototypeSolidKind::WalkableStep:
      return "walkable_step";
    case PrototypeSolidKind::LowClearance:
      return "low_clearance";
    case PrototypeSolidKind::ShootingTarget:
      return "shooting_target";
  }
  throw std::logic_error("validated level has unsupported solid kind");
}

std::string_view surfaceString(PrototypeSurface surface) {
  switch (surface) {
    case PrototypeSurface::Floor:
      return "floor";
    case PrototypeSurface::Boundary:
      return "boundary";
    case PrototypeSurface::Obstacle:
      return "obstacle";
    case PrototypeSurface::ShootingTarget:
      return "shooting_target";
  }
  throw std::logic_error("validated level has unsupported surface role");
}

std::string serializeDocument(const LevelDocument& document) {
  Json root = Json::object();
  root["version"] = document.version;

  Json terrain = Json::object();
  terrain["origin"] = positionJson(document.terrain.origin);
  terrain["sample_spacing"] = document.terrain.sample_spacing;
  terrain["heights"] = Json::array();
  for (const float height : document.terrain.heights) {
    terrain["heights"].push_back(height);
  }
  root["terrain"] = std::move(terrain);

  Json solids = Json::array();
  for (const PrototypeSolid& solid : document.solids) {
    Json value = Json::object();
    value["center"] = positionJson(solid.center);
    value["half_extent"] = extentJson(solid.half_extent);
    value["color"] = solid.color;
    value["kind"] = solidKindString(solid.kind);
    value["surface"] = surfaceString(solid.surface);
    solids.push_back(std::move(value));
  }
  root["solids"] = std::move(solids);

  Json spawn = Json::object();
  spawn["foot_position"] = positionJson(document.player_spawn.foot_position);
  spawn["yaw_degrees"] = document.player_spawn.yaw_degrees;
  root["player_spawn"] = std::move(spawn);

  Json lighting = Json::object();
  lighting["point_lights"] = Json::array();
  for (const PrototypePointLight& point :
       document.environment_light.point_lights) {
    Json value = Json::object();
    value["position"] = positionJson(point.position);
    value["color"] = point.color;
    value["intensity"] = point.intensity;
    value["radius"] = point.radius;
    lighting["point_lights"].push_back(std::move(value));
  }
  lighting["ambient_intensity"] = document.environment_light.ambient_intensity;
  root["environment_light"] = std::move(lighting);

  Json prop = Json::object();
  prop["translation"] = positionJson(document.static_prop.translation);
  prop["yaw_degrees"] = document.static_prop.yaw_degrees;
  prop["uniform_scale"] = document.static_prop.uniform_scale;
  prop["surface"] = surfaceString(document.static_prop.surface);
  Json proxy = Json::object();
  proxy["center"] = positionJson(document.static_prop.box_proxy_center);
  proxy["half_extent"] = extentJson(document.static_prop.box_proxy_half_extent);
  prop["box_proxy"] = std::move(proxy);
  root["static_prop"] = std::move(prop);

  return root.dump(2, ' ', false, Json::error_handler_t::strict) + '\n';
}

std::filesystem::path temporaryPathFor(
    const std::filesystem::path& destination) {
  static std::atomic<std::uint64_t> sequence{0};
  for (int attempt = 0; attempt < 100; ++attempt) {
    const std::filesystem::path candidate =
        destination.parent_path() /
        (destination.filename().string() + ".tmp-" +
         std::to_string(sequence.fetch_add(1, std::memory_order_relaxed)));
    if (!std::filesystem::exists(candidate)) {
      return candidate;
    }
  }
  throw std::runtime_error("could not allocate a sibling temporary path");
}

std::error_code replaceFile(const std::filesystem::path& temporary,
                            const std::filesystem::path& destination) {
#if defined(_WIN32)
  if (MoveFileExW(temporary.c_str(), destination.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
    return {};
  }
  return {static_cast<int>(GetLastError()), std::system_category()};
#else
  std::error_code error;
  std::filesystem::rename(temporary, destination, error);
  return error;
#endif
}

LevelDiagnostic filesystemDiagnostic(const std::filesystem::path& path,
                                     std::string message) {
  return {LevelDiagnosticCategory::Filesystem, path, {}, std::move(message)};
}
}  // namespace

LevelDocumentLoadResult loadLevelDocument(const std::filesystem::path& path) {
  const std::filesystem::path resolved = normalizedAbsolute(path);
  std::ifstream input(resolved, std::ios::binary);
  input.imbue(std::locale::classic());
  if (!input) {
    return {std::nullopt,
            {filesystemDiagnostic(resolved, "could not open level document")}};
  }
  const std::string bytes((std::istreambuf_iterator<char>(input)),
                          std::istreambuf_iterator<char>());
  if (input.bad()) {
    return {std::nullopt,
            {filesystemDiagnostic(resolved, "could not read level document")}};
  }
  try {
    LevelDocument document = parseDocument(Json::parse(bytes));
    return {std::move(document), {}};
  } catch (const nlohmann::json::parse_error& error) {
    return {std::nullopt,
            {{LevelDiagnosticCategory::Parse, resolved,
              "byte " + std::to_string(error.byte),
              std::string("malformed JSON: ") + error.what()}}};
  } catch (const ParseFailure& error) {
    return {std::nullopt,
            {{LevelDiagnosticCategory::Parse, resolved, error.documentPath(),
              error.what()}}};
  } catch (const nlohmann::json::exception& error) {
    return {std::nullopt,
            {{LevelDiagnosticCategory::Parse, resolved, {}, error.what()}}};
  }
}

LevelDocumentSaveResult saveLevelDocument(const std::filesystem::path& path,
                                          const LevelDocument& document) {
  const std::filesystem::path resolved = normalizedAbsolute(path);
  std::vector<LevelDiagnostic> diagnostics =
      validateLevelDocument(document, resolved);
  if (!diagnostics.empty()) {
    return {std::move(diagnostics)};
  }

  std::filesystem::path temporary;
  try {
    temporary = temporaryPathFor(resolved);
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.imbue(std::locale::classic());
    if (!output) {
      return {{filesystemDiagnostic(
          resolved, "could not create sibling temporary level document")}};
    }
    const std::string serialized = serializeDocument(document);
    output.write(serialized.data(),
                 static_cast<std::streamsize>(serialized.size()));
    output.flush();
    if (!output) {
      output.close();
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return {{filesystemDiagnostic(
          resolved, "could not write complete level document")}};
    }
    output.close();
    if (!output) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return {
          {filesystemDiagnostic(resolved, "could not close level document")}};
    }
    const std::error_code replacement_error = replaceFile(temporary, resolved);
    if (replacement_error) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      return {{filesystemDiagnostic(
          resolved, "could not atomically replace destination: " +
                        replacement_error.message())}};
    }
    return {};
  } catch (const std::exception& error) {
    if (!temporary.empty()) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
    }
    return {{filesystemDiagnostic(resolved, error.what())}};
  }
}

std::string formatLevelDiagnostics(
    const std::vector<LevelDiagnostic>& diagnostics) {
  std::ostringstream output;
  for (std::size_t index = 0; index < diagnostics.size(); ++index) {
    const LevelDiagnostic& diagnostic = diagnostics[index];
    if (index != 0) output << '\n';
    if (!diagnostic.source_path.empty()) {
      output << diagnostic.source_path.string();
    } else {
      output << "level document";
    }
    if (!diagnostic.document_path.empty()) {
      output << ": " << diagnostic.document_path;
    }
    output << ": " << diagnostic.message;
  }
  return output.str();
}
