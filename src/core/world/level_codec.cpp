#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <locale>
#include <nlohmann/json.hpp>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

#include "core/world/door.hpp"
#include "core/world/level_document.hpp"
#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
#include "core/world/scene_assets.hpp"

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
    if (value.get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max())
      fail(path, "is outside the supported integer range");
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
  fail(path, "unsupported solid kind '" + text + "'");
}

PrototypeSurface parseSurface(const Json& value, const std::string& path) {
  const std::string text = parseString(value, path);
  if (text == "floor") return PrototypeSurface::Floor;
  if (text == "boundary") return PrototypeSurface::Boundary;
  if (text == "obstacle") return PrototypeSurface::Obstacle;
  fail(path, "unsupported surface role '" + text + "'");
}

std::string legacyMaterial(PrototypeSurface surface) {
  switch (surface) {
    case PrototypeSurface::Floor:
      return "prototype-floor";
    case PrototypeSurface::Boundary:
      return "prototype-boundary";
    case PrototypeSurface::Obstacle:
      return "prototype-obstacle";
  }
  throw std::logic_error("invalid legacy surface");
}

PrototypeTerrain parseTerrain(const Json& value, std::uint32_t version) {
  constexpr std::size_t height_count =
      prototype_terrain_sample_count * prototype_terrain_sample_count;
  if (version >= 6)
    requireObjectFields(value, "terrain",
                        {"origin", "sample_spacing", "heights", "material"});
  else
    requireObjectFields(value, "terrain",
                        {"origin", "sample_spacing", "heights"});
  const Json& heights = value.at("heights");
  if (!heights.is_array() || heights.size() != height_count) {
    fail("terrain.heights", "must contain exactly " +
                                std::to_string(height_count) +
                                " row-major samples");
  }
  PrototypeTerrain terrain{};
  if (version >= 6)
    terrain.material = parseString(value.at("material"), "terrain.material");
  terrain.origin = parsePosition(value.at("origin"), "terrain.origin");
  terrain.sample_spacing =
      parseFloat(value.at("sample_spacing"), "terrain.sample_spacing");
  for (std::size_t index = 0; index < height_count; ++index) {
    terrain.heights[index] = parseFloat(
        heights[index], "terrain.heights[" + std::to_string(index) + "]");
  }
  return terrain;
}

PrototypeSolid parseSolid(const Json& value, std::size_t index,
                          std::uint32_t version) {
  const std::string path = "solids[" + std::to_string(index) + "]";
  if (version >= 6)
    requireObjectFields(value, path,
                        {"center", "half_extent", "color", "kind", "material"});
  else
    requireObjectFields(value, path,
                        {"center", "half_extent", "color", "kind", "surface"});
  return {parsePosition(value.at("center"), path + ".center"),
          parseExtent(value.at("half_extent"), path + ".half_extent"),
          parseColor(value.at("color"), path + ".color"),
          parseSolidKind(value.at("kind"), path + ".kind"),
          version >= 6 ? parseString(value.at("material"), path + ".material")
                       : legacyMaterial(parseSurface(value.at("surface"),
                                                     path + ".surface"))};
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
  if (parseSurface(value.at("surface"), "static_prop.surface") !=
      PrototypeSurface::Obstacle)
    fail("static_prop.surface", "legacy prop must use obstacle surface");
  return {"prototype-chair",
          "prototype-chair",
          parsePosition(value.at("translation"), "static_prop.translation"),
          parseFloat(value.at("yaw_degrees"), "static_prop.yaw_degrees"),
          parseFloat(value.at("uniform_scale"), "static_prop.uniform_scale"),
          {{parsePosition(proxy.at("center"), "static_prop.box_proxy.center"),
            parseExtent(proxy.at("half_extent"),
                        "static_prop.box_proxy.half_extent")}}};
}

PrototypeStaticProp parseProp(const Json& value, std::size_t index) {
  const auto path = "props[" + std::to_string(index) + "]";
  requireObjectFields(value, path,
                      {"id", "model", "translation", "yaw_degrees",
                       "uniform_scale", "collision_boxes"});
  PrototypeStaticProp prop;
  prop.id = parseString(value.at("id"), path + ".id");
  prop.model = parseString(value.at("model"), path + ".model");
  if (prop.id.size() > 64 || prop.model.size() > 64)
    fail(path, "identities must contain at most 64 characters");
  prop.translation =
      parsePosition(value.at("translation"), path + ".translation");
  prop.yaw_degrees = parseFloat(value.at("yaw_degrees"), path + ".yaw_degrees");
  prop.uniform_scale =
      parseFloat(value.at("uniform_scale"), path + ".uniform_scale");
  const auto& boxes = value.at("collision_boxes");
  if (!boxes.is_array() || boxes.size() > level_maximum_prop_box_count)
    fail(path + ".collision_boxes", "must contain at most eight boxes");
  for (std::size_t i = 0; i < boxes.size(); ++i) {
    const auto field = path + ".collision_boxes[" + std::to_string(i) + "]";
    requireObjectFields(boxes[i], field, {"center", "half_extent"});
    prop.collision_boxes.push_back(
        {parsePosition(boxes[i].at("center"), field + ".center"),
         parseExtent(boxes[i].at("half_extent"), field + ".half_extent")});
  }
  return prop;
}

std::optional<PrototypeLightSwitch> parseLightSwitch(const Json& value) {
  if (value.is_null()) return std::nullopt;
  requireObjectFields(
      value, "light_switch",
      {"position", "yaw_degrees", "point_light_index", "initially_on"});
  const auto index = parseUnsigned(value.at("point_light_index"),
                                   "light_switch.point_light_index");
  if (index >= prototype_point_light_count)
    fail("light_switch.point_light_index", "must select point light 0 or 1");
  if (!value.at("initially_on").is_boolean())
    fail("light_switch.initially_on", "must be a boolean");
  return PrototypeLightSwitch{
      parsePosition(value.at("position"), "light_switch.position"),
      parseFloat(value.at("yaw_degrees"), "light_switch.yaw_degrees"), index,
      value.at("initially_on").get<bool>()};
}

DoorDefinition parseDoor(const Json& value, std::size_t index) {
  const auto path = "doors[" + std::to_string(index) + "]";
  requireObjectFields(
      value, path,
      {"id", "hinge_position", "closed_yaw_degrees", "width", "height",
       "thickness", "open_angle_degrees", "speed_degrees_per_second",
       "lock_side", "initially_open", "initially_locked"});
  DoorDefinition door;
  door.id = parseString(value.at("id"), path + ".id");
  if (door.id.size() > 64)
    fail(path + ".id", "must contain at most 64 characters");
  door.hinge_position =
      parsePosition(value.at("hinge_position"), path + ".hinge_position");
  door.closed_yaw_degrees =
      parseFloat(value.at("closed_yaw_degrees"), path + ".closed_yaw_degrees");
  door.width = parseFloat(value.at("width"), path + ".width");
  door.height = parseFloat(value.at("height"), path + ".height");
  door.thickness = parseFloat(value.at("thickness"), path + ".thickness");
  door.open_angle_degrees =
      parseFloat(value.at("open_angle_degrees"), path + ".open_angle_degrees");
  door.speed_degrees_per_second = parseFloat(
      value.at("speed_degrees_per_second"), path + ".speed_degrees_per_second");
  const auto side = parseString(value.at("lock_side"), path + ".lock_side");
  if (side == "none")
    door.lock_side = DoorLockSide::None;
  else if (side == "positive-z")
    door.lock_side = DoorLockSide::PositiveZ;
  else if (side == "negative-z")
    door.lock_side = DoorLockSide::NegativeZ;
  else
    fail(path + ".lock_side", "must select none, positive-z, or negative-z");
  for (const char* name : {"initially_open", "initially_locked"})
    if (!value.at(name).is_boolean())
      fail(path + "." + name, "must be a boolean");
  door.initially_open = value.at("initially_open").get<bool>();
  door.initially_locked = value.at("initially_locked").get<bool>();
  for (auto p : doorCorners(door, doorInitialAngle(door)))
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
      fail(path + ".hinge_position",
           "derived preview bounds must remain finite");
  return door;
}

LevelDocument parseDocument(const Json& root) {
  if (!root.is_object()) fail("", "must be an object");
  if (!root.contains("version")) fail("version", "required field is missing");
  const std::uint32_t version = parseUnsigned(root.at("version"), "version");
  if (version != 2 && version != 3 && version != 4 && version != 5 &&
      version != level_format_version) {
    fail("version",
         "unsupported level format version " + std::to_string(version));
  }
  if (version == 2) {
    requireObjectFields(root, "",
                        {"version", "terrain", "solids", "player_spawn",
                         "environment_light", "static_prop"});
  } else if (version == 3) {
    requireObjectFields(root, "",
                        {"version", "terrain", "solids", "player_spawn",
                         "environment_light", "static_prop", "light_switch"});
  } else if (version == 4) {
    requireObjectFields(
        root, "",
        {"version", "terrain", "solids", "entries", "default_entry",
         "environment_light", "static_prop", "light_switch"});
  } else if (version == 5) {
    requireObjectFields(
        root, "",
        {"version", "terrain", "solids", "entries", "default_entry",
         "environment_light", "static_prop", "light_switch", "doors"});
  } else {
    requireObjectFields(
        root, "",
        {"version", "terrain", "solids", "entries", "default_entry",
         "environment_light", "props", "light_switch", "doors"});
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
    solids.push_back(parseSolid(solids_json[index], index, version));
  }
  LevelDocument document;
  if (version < 4 || !root.at("terrain").is_null())
    document.terrain = parseTerrain(root.at("terrain"), version);
  document.solids = std::move(solids);
  if (version < 4) {
    document.entries.push_back(
        {"default", parseSpawn(root.at("player_spawn"))});
    document.default_entry = "default";
  } else {
    const auto& entries = root.at("entries");
    if (!entries.is_array() || entries.size() > level_maximum_entry_count)
      fail("entries", "must be an array of at most 16 entries");
    for (std::size_t i = 0; i < entries.size(); ++i) {
      const auto path = "entries[" + std::to_string(i) + "]";
      const auto& value = entries[i];
      requireObjectFields(value, path, {"id", "foot_position", "yaw_degrees"});
      auto id = parseString(value.at("id"), path + ".id");
      if (id.size() > level_maximum_entry_id_length)
        fail(path + ".id", "must contain at most 64 characters");
      document.entries.push_back(
          {std::move(id),
           {parsePosition(value.at("foot_position"), path + ".foot_position"),
            parseFloat(value.at("yaw_degrees"), path + ".yaw_degrees")}});
    }
    document.default_entry =
        parseString(root.at("default_entry"), "default_entry");
    if (document.default_entry.size() > level_maximum_entry_id_length)
      fail("default_entry", "must contain at most 64 characters");
  }
  document.environment_light =
      parseEnvironmentLight(root.at("environment_light"));
  if (version < 6)
    document.props.push_back(parseStaticProp(root.at("static_prop")));
  else {
    const auto& props = root.at("props");
    if (!props.is_array() || props.size() > level_maximum_prop_count)
      fail("props", "must contain at most 128 placements");
    for (std::size_t i = 0; i < props.size(); ++i)
      document.props.push_back(parseProp(props[i], i));
  }
  if (version != 2)
    document.light_switch = parseLightSwitch(root.at("light_switch"));
  if (version >= 5) {
    const auto& doors = root.at("doors");
    if (!doors.is_array() || doors.size() > level_maximum_door_count)
      fail("doors", "must be an array of at most 32 doors");
    for (std::size_t i = 0; i < doors.size(); ++i)
      document.doors.push_back(parseDoor(doors[i], i));
  }
  const auto finite = [](WorldPosition p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
  };
  const auto requireBounds = [&](WorldPosition p, WorldExtent e,
                                 const std::string& field) {
    if (!finite({p.x - e.x, p.y - e.y, p.z - e.z}) ||
        !finite({p.x + e.x, p.y + e.y, p.z + e.z}) ||
        !finite({2 * e.x, 2 * e.y, 2 * e.z}))
      fail(field, "derived preview bounds must remain finite");
  };
  for (std::size_t i = 0; i < document.solids.size(); ++i)
    requireBounds(document.solids[i].center, document.solids[i].half_extent,
                  "solids[" + std::to_string(i) + "].bounds");
  for (const auto& prop : document.props) {
    const double yaw =
        static_cast<double>(prop.yaw_degrees) * std::numbers::pi / 180;
    const float c = static_cast<float>(std::abs(std::cos(yaw)));
    const float s = static_cast<float>(std::abs(std::sin(yaw)));
    const auto requirePropBounds = [&](const PropCollisionBox& box) {
      const auto extent = propBoxWorldHalfExtent(prop, box);
      requireBounds(
          propBoxWorldCenter(prop, box),
          {c * extent.x + s * extent.z, extent.y, s * extent.x + c * extent.z},
          "props['" + prop.id + "'].bounds");
    };
    for (const auto& box : prop.collision_boxes) requirePropBounds(box);
    if (const auto* model = findSceneModel(prop.model))
      requirePropBounds(sceneModelBounds(*model));
  }
  if (document.light_switch)
    for (const auto point : lightSwitchCorners(*document.light_switch))
      if (!finite(point))
        fail("light_switch.position",
             "derived preview bounds must remain finite");
  if (document.terrain) {
    const auto& terrain = *document.terrain;
    for (std::size_t z = 0; z < prototype_terrain_sample_count; ++z)
      for (std::size_t x = 0; x < prototype_terrain_sample_count; ++x)
        if (!finite(prototypeTerrainSamplePosition(terrain, x, z)))
          fail("terrain", "derived preview samples must remain finite");
  }
  return document;
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
  }
  throw std::logic_error("validated level has unsupported solid kind");
}

std::string serializeDocument(const LevelDocument& document) {
  Json root = Json::object();
  root["version"] = document.version;

  root["terrain"] = nullptr;
  if (document.terrain) {
    Json terrain = Json::object();
    terrain["origin"] = positionJson(document.terrain->origin);
    terrain["sample_spacing"] = document.terrain->sample_spacing;
    terrain["heights"] = document.terrain->heights;
    terrain["material"] = document.terrain->material;
    root["terrain"] = std::move(terrain);
  }

  Json solids = Json::array();
  for (const PrototypeSolid& solid : document.solids) {
    Json value = Json::object();
    value["center"] = positionJson(solid.center);
    value["half_extent"] = extentJson(solid.half_extent);
    value["color"] = solid.color;
    value["kind"] = solidKindString(solid.kind);
    value["material"] = solid.material;
    solids.push_back(std::move(value));
  }
  root["solids"] = std::move(solids);

  root["entries"] = Json::array();
  for (const auto& entry : document.entries) {
    Json value = Json::object();
    value["id"] = entry.id;
    value["foot_position"] = positionJson(entry.pose.foot_position);
    value["yaw_degrees"] = entry.pose.yaw_degrees;
    root["entries"].push_back(std::move(value));
  }
  root["default_entry"] = document.default_entry;

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

  root["props"] = Json::array();
  for (const auto& placement : document.props) {
    Json prop = Json::object();
    prop["id"] = placement.id;
    prop["model"] = placement.model;
    prop["translation"] = positionJson(placement.translation);
    prop["yaw_degrees"] = placement.yaw_degrees;
    prop["uniform_scale"] = placement.uniform_scale;
    prop["collision_boxes"] = Json::array();
    for (const auto& box : placement.collision_boxes) {
      Json proxy = Json::object();
      proxy["center"] = positionJson(box.center);
      proxy["half_extent"] = extentJson(box.half_extent);
      prop["collision_boxes"].push_back(std::move(proxy));
    }
    root["props"].push_back(std::move(prop));
  }
  root["light_switch"] = nullptr;
  if (document.light_switch) {
    const auto& light_switch = *document.light_switch;
    Json value = Json::object();
    value["position"] = positionJson(light_switch.position);
    value["yaw_degrees"] = light_switch.yaw_degrees;
    value["point_light_index"] = light_switch.point_light_index;
    value["initially_on"] = light_switch.initially_on;
    root["light_switch"] = std::move(value);
  }

  root["doors"] = Json::array();
  for (const auto& door : document.doors) {
    Json value = Json::object();
    value["id"] = door.id;
    value["hinge_position"] = positionJson(door.hinge_position);
    value["closed_yaw_degrees"] = door.closed_yaw_degrees;
    value["width"] = door.width;
    value["height"] = door.height;
    value["thickness"] = door.thickness;
    value["open_angle_degrees"] = door.open_angle_degrees;
    value["speed_degrees_per_second"] = door.speed_degrees_per_second;
    value["lock_side"] = door.lock_side == DoorLockSide::None ? "none"
                         : door.lock_side == DoorLockSide::PositiveZ
                             ? "positive-z"
                             : "negative-z";
    value["initially_open"] = door.initially_open;
    value["initially_locked"] = door.initially_locked;
    root["doors"].push_back(std::move(value));
  }
  return root.dump(2, ' ', false, Json::error_handler_t::strict) + '\n';
}

std::filesystem::path temporaryPathFor(
    const std::filesystem::path& destination) {
  static std::atomic<std::uint64_t> sequence{0};
  for (int attempt = 0; attempt < 100; ++attempt) {
    const std::filesystem::path candidate =
        destination.parent_path() /
        std::filesystem::path(destination.filename())
            .concat(".tmp-" + std::to_string(sequence.fetch_add(
                                  1, std::memory_order_relaxed)));
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
    const Json root = Json::parse(bytes);
    LevelDocument document = parseDocument(root);
    return {std::move(document), {}, root.at("version").get<std::uint32_t>()};
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
      const auto text = diagnostic.source_path.u8string();
      output << std::string(text.begin(), text.end());
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
