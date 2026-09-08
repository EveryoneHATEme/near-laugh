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

PrototypePointLight parsePointLight(const Json& value, std::size_t index,
                                    std::uint32_t version) {
  const std::string path =
      "environment_light.point_lights[" + std::to_string(index) + "]";
  if (version >= 8)
    requireObjectFields(value, path,
                        {"id", "position", "color", "intensity", "radius",
                         "initially_on", "casts_shadows"});
  else
    requireObjectFields(value, path,
                        {"position", "color", "intensity", "radius"});
  PrototypePointLight result{
      parsePosition(value.at("position"), path + ".position"),
      parseFloatArray<3>(value.at("color"), path + ".color"),
      parseFloat(value.at("intensity"), path + ".intensity"),
      parseFloat(value.at("radius"), path + ".radius")};
  result.id = "point-light-" + std::to_string(index);
  if (version >= 8) {
    result.id = parseString(value.at("id"), path + ".id");
    if (result.id.size() > 64)
      fail(path + ".id", "must contain at most 64 characters");
    for (const auto* field : {"initially_on", "casts_shadows"})
      if (!value.at(field).is_boolean())
        fail(path + "." + field, "must be a boolean");
    result.initially_on = value.at("initially_on").get<bool>();
    result.casts_shadows = value.at("casts_shadows").get<bool>();
  }
  return result;
}

PrototypeEnvironmentLight parseEnvironmentLight(const Json& value,
                                                std::uint32_t version) {
  requireObjectFields(value, "environment_light",
                      {"point_lights", "ambient_intensity"});
  const Json& points = value.at("point_lights");
  if (!points.is_array() ||
      (version < 8 ? points.size() != 2
                   : points.size() > level_maximum_point_light_count)) {
    fail("environment_light.point_lights",
         version < 8 ? "must contain exactly two point lights"
                     : "must contain at most eight point lights");
  }
  PrototypeEnvironmentLight result{};
  for (std::size_t index = 0; index < points.size(); ++index) {
    result.point_lights.push_back(
        parsePointLight(points[index], index, version));
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

std::optional<PrototypeLightSwitch> parseLegacyLightSwitch(
    const Json& value, PrototypeEnvironmentLight& lighting) {
  if (value.is_null()) return std::nullopt;
  requireObjectFields(
      value, "light_switch",
      {"position", "yaw_degrees", "point_light_index", "initially_on"});
  const auto index = parseUnsigned(value.at("point_light_index"),
                                   "light_switch.point_light_index");
  if (index >= 2)
    fail("light_switch.point_light_index", "must select point light 0 or 1");
  if (!value.at("initially_on").is_boolean())
    fail("light_switch.initially_on", "must be a boolean");
  lighting.point_lights.at(index).initially_on =
      value.at("initially_on").get<bool>();
  return PrototypeLightSwitch{
      parsePosition(value.at("position"), "light_switch.position"),
      parseFloat(value.at("yaw_degrees"), "light_switch.yaw_degrees"),
      lighting.point_lights.at(index).id, "light-switch-0"};
}

PrototypeLightSwitch parseLightSwitch(const Json& value, std::size_t index) {
  const auto path = "light_switches[" + std::to_string(index) + "]";
  requireObjectFields(value, path,
                      {"id", "position", "yaw_degrees", "light_id"});
  PrototypeLightSwitch result{
      parsePosition(value.at("position"), path + ".position"),
      parseFloat(value.at("yaw_degrees"), path + ".yaw_degrees"),
      parseString(value.at("light_id"), path + ".light_id"),
      parseString(value.at("id"), path + ".id")};
  if (result.id.size() > 64 || result.light_id.size() > 64)
    fail(path, "identities must contain at most 64 characters");
  return result;
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

bool parseBool(const Json& value, const std::string& path) {
  if (!value.is_boolean()) fail(path, "must be a boolean");
  return value.get<bool>();
}

std::string parseAudioId(const Json& value, const std::string& path) {
  auto id = parseString(value, path);
  if (id.size() > 64) fail(path, "must contain at most 64 bytes");
  return id;
}
std::optional<std::string> parseAudioReference(const Json& value,
                                               const std::string& path) {
  if (value.is_null()) return {};
  return parseAudioId(value, path);
}

LevelAudio parseAudio(const Json& value) {
  requireObjectFields(value, "audio",
                      {"cues", "sources", "rooms", "connections"});
  const auto records = [&](std::string_view collection,
                           std::size_t bound) -> const Json& {
    const auto& array = value.at(collection);
    if (!array.is_array() || array.size() > bound)
      fail("audio." + std::string(collection),
           "must be an array of at most " + std::to_string(bound) + " records");
    return array;
  };
  LevelAudio audio;
  for (const auto& v : records("cues", level_maximum_audio_cue_count)) {
    const auto p = "audio.cues[" + std::to_string(audio.cues.size()) + "]";
    requireObjectFields(v, p,
                        {"id", "clip", "caption", "kind", "loop", "spatial"});
    const auto kind = parseString(v.at("kind"), p + ".kind");
    if (kind != "dialogue" && kind != "essential" && kind != "ambience")
      fail(p + ".kind", "unsupported cue kind '" + kind + "'");
    audio.cues.push_back({parseAudioId(v.at("id"), p + ".id"),
                          parseAudioId(v.at("clip"), p + ".clip"),
                          parseAudioReference(v.at("caption"), p + ".caption"),
                          kind == "dialogue"    ? AudioCueKind::Dialogue
                          : kind == "essential" ? AudioCueKind::Essential
                                                : AudioCueKind::Ambience,
                          parseBool(v.at("loop"), p + ".loop"),
                          parseBool(v.at("spatial"), p + ".spatial")});
  }
  for (const auto& v : records("sources", level_maximum_audio_source_count)) {
    const auto p =
        "audio.sources[" + std::to_string(audio.sources.size()) + "]";
    requireObjectFields(v, p,
                        {"id", "cue", "position", "gain", "near_distance",
                         "far_distance", "autoplay"});
    audio.sources.push_back(
        {parseAudioId(v.at("id"), p + ".id"),
         parseAudioId(v.at("cue"), p + ".cue"),
         parsePosition(v.at("position"), p + ".position"),
         parseFloat(v.at("gain"), p + ".gain"),
         parseFloat(v.at("near_distance"), p + ".near_distance"),
         parseFloat(v.at("far_distance"), p + ".far_distance"),
         parseBool(v.at("autoplay"), p + ".autoplay")});
  }
  for (const auto& v : records("rooms", level_maximum_audio_room_count)) {
    const auto p = "audio.rooms[" + std::to_string(audio.rooms.size()) + "]";
    requireObjectFields(v, p, {"id", "center", "half_extent"});
    audio.rooms.push_back(
        {parseAudioId(v.at("id"), p + ".id"),
         parsePosition(v.at("center"), p + ".center"),
         parseExtent(v.at("half_extent"), p + ".half_extent")});
  }
  for (const auto& v :
       records("connections", level_maximum_audio_connection_count)) {
    const auto p =
        "audio.connections[" + std::to_string(audio.connections.size()) + "]";
    requireObjectFields(
        v, p, {"id", "room_a", "room_b", "door", "closed_gain", "open_gain"});
    audio.connections.push_back(
        {parseAudioId(v.at("id"), p + ".id"),
         parseAudioReference(v.at("room_a"), p + ".room_a"),
         parseAudioReference(v.at("room_b"), p + ".room_b"),
         parseAudioReference(v.at("door"), p + ".door"),
         parseFloat(v.at("closed_gain"), p + ".closed_gain"),
         parseFloat(v.at("open_gain"), p + ".open_gain")});
  }
  return audio;
}

LevelCharacters parseCharacters(const Json& value) {
  requireObjectFields(value, "characters", {"actors", "marks", "routes"});
  const auto records = [&](std::string_view collection,
                           std::size_t bound) -> const Json& {
    const auto& array = value.at(collection);
    if (!array.is_array() || array.size() > bound)
      fail("characters." + std::string(collection),
           "must be an array of at most " + std::to_string(bound) + " records");
    return array;
  };
  LevelCharacters characters;
  for (const auto& v : records("actors", level_maximum_actor_count)) {
    const auto p =
        "characters.actors[" + std::to_string(characters.actors.size()) + "]";
    requireObjectFields(v, p,
                        {"id", "model", "initial_mark", "initial_route",
                         "speed", "footstep_source", "interaction_source"});
    characters.actors.push_back(
        {parseAudioId(v.at("id"), p + ".id"),
         parseAudioId(v.at("model"), p + ".model"),
         parseAudioId(v.at("initial_mark"), p + ".initial_mark"),
         parseAudioReference(v.at("initial_route"), p + ".initial_route"),
         parseFloat(v.at("speed"), p + ".speed"),
         parseAudioReference(v.at("footstep_source"), p + ".footstep_source"),
         parseAudioReference(v.at("interaction_source"),
                             p + ".interaction_source")});
  }
  for (const auto& v : records("marks", level_maximum_character_mark_count)) {
    const auto p =
        "characters.marks[" + std::to_string(characters.marks.size()) + "]";
    requireObjectFields(v, p, {"id", "feet_position", "yaw_degrees"});
    characters.marks.push_back(
        {parseAudioId(v.at("id"), p + ".id"),
         parsePosition(v.at("feet_position"), p + ".feet_position"),
         parseFloat(v.at("yaw_degrees"), p + ".yaw_degrees")});
  }
  for (const auto& v : records("routes", level_maximum_character_route_count)) {
    const auto p =
        "characters.routes[" + std::to_string(characters.routes.size()) + "]";
    requireObjectFields(v, p, {"id", "actor", "marks", "final_clip"});
    CharacterRouteDefinition route{
        parseAudioId(v.at("id"), p + ".id"),
        parseAudioId(v.at("actor"), p + ".actor"),
        {},
        parseAudioReference(v.at("final_clip"), p + ".final_clip")};
    const auto& marks = v.at("marks");
    if (!marks.is_array() || marks.size() > level_maximum_route_mark_count)
      fail(p + ".marks", "must be an array of at most 32 mark references");
    for (std::size_t i = 0; i < marks.size(); ++i)
      route.marks.push_back(
          parseAudioId(marks[i], p + ".marks[" + std::to_string(i) + "]"));
    characters.routes.push_back(std::move(route));
  }
  return characters;
}

LevelDocument parseDocument(const Json& root) {
  if (!root.is_object()) fail("", "must be an object");
  if (!root.contains("version")) fail("version", "required field is missing");
  const std::uint32_t version = parseUnsigned(root.at("version"), "version");
  if (version != 2 && version != 3 && version != 4 && version != 5 &&
      version != 6 && version != 7 && version != 8 &&
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
  } else if (version == 6) {
    requireObjectFields(
        root, "",
        {"version", "terrain", "solids", "entries", "default_entry",
         "environment_light", "props", "light_switch", "doors"});
  } else if (version == 7) {
    requireObjectFields(
        root, "",
        {"version", "terrain", "solids", "entries", "default_entry",
         "environment_light", "props", "light_switch", "doors", "audio"});
  } else if (version == 8) {
    requireObjectFields(
        root, "",
        {"version", "terrain", "solids", "entries", "default_entry",
         "environment_light", "props", "light_switches", "doors", "audio"});
  } else {
    requireObjectFields(root, "",
                        {"version", "terrain", "solids", "entries",
                         "default_entry", "environment_light", "props",
                         "light_switches", "doors", "audio", "characters"});
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
  if (version >= 7) document.audio = parseAudio(root.at("audio"));
  if (version >= 9)
    document.characters = parseCharacters(root.at("characters"));
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
      parseEnvironmentLight(root.at("environment_light"), version);
  if (version < 6)
    document.props.push_back(parseStaticProp(root.at("static_prop")));
  else {
    const auto& props = root.at("props");
    if (!props.is_array() || props.size() > level_maximum_prop_count)
      fail("props", "must contain at most 128 placements");
    for (std::size_t i = 0; i < props.size(); ++i)
      document.props.push_back(parseProp(props[i], i));
  }
  if (version >= 8) {
    const auto& switches = root.at("light_switches");
    if (!switches.is_array() ||
        switches.size() > level_maximum_light_switch_count)
      fail("light_switches", "must contain at most sixteen switches");
    for (std::size_t i = 0; i < switches.size(); ++i)
      document.light_switches.push_back(parseLightSwitch(switches[i], i));
  } else if (version != 2) {
    if (auto value = parseLegacyLightSwitch(root.at("light_switch"),
                                            document.environment_light))
      document.light_switches.push_back(std::move(*value));
  }
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
  for (std::size_t i = 0; i < document.audio.rooms.size(); ++i)
    requireBounds(document.audio.rooms[i].center,
                  document.audio.rooms[i].half_extent,
                  "audio.rooms[" + std::to_string(i) + "].bounds");
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
  for (const auto& light_switch : document.light_switches)
    for (const auto point : lightSwitchCorners(light_switch))
      if (!finite(point))
        fail("light_switches.position",
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
    value["id"] = point.id;
    value["position"] = positionJson(point.position);
    value["color"] = point.color;
    value["intensity"] = point.intensity;
    value["radius"] = point.radius;
    value["initially_on"] = point.initially_on;
    value["casts_shadows"] = point.casts_shadows;
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
  root["light_switches"] = Json::array();
  for (const auto& light_switch : document.light_switches) {
    Json value = Json::object();
    value["id"] = light_switch.id;
    value["position"] = positionJson(light_switch.position);
    value["yaw_degrees"] = light_switch.yaw_degrees;
    value["light_id"] = light_switch.light_id;
    root["light_switches"].push_back(std::move(value));
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
  const auto referenceJson = [](const std::optional<std::string>& id) {
    return id ? Json(*id) : Json(nullptr);
  };
  auto audio = Json::object();
  audio["cues"] = Json::array();
  for (const auto& c : document.audio.cues)
    audio["cues"].push_back(
        {{"id", c.id},
         {"clip", c.clip},
         {"caption", referenceJson(c.caption)},
         {"kind", c.kind == AudioCueKind::Dialogue    ? "dialogue"
                  : c.kind == AudioCueKind::Essential ? "essential"
                                                      : "ambience"},
         {"loop", c.loop},
         {"spatial", c.spatial}});
  audio["sources"] = Json::array();
  for (const auto& s : document.audio.sources)
    audio["sources"].push_back({{"id", s.id},
                                {"cue", s.cue},
                                {"position", positionJson(s.position)},
                                {"gain", s.gain},
                                {"near_distance", s.near_distance},
                                {"far_distance", s.far_distance},
                                {"autoplay", s.autoplay}});
  audio["rooms"] = Json::array();
  for (const auto& r : document.audio.rooms)
    audio["rooms"].push_back({{"id", r.id},
                              {"center", positionJson(r.center)},
                              {"half_extent", extentJson(r.half_extent)}});
  audio["connections"] = Json::array();
  for (const auto& c : document.audio.connections)
    audio["connections"].push_back({{"id", c.id},
                                    {"room_a", referenceJson(c.room_a)},
                                    {"room_b", referenceJson(c.room_b)},
                                    {"door", referenceJson(c.door)},
                                    {"closed_gain", c.closed_gain},
                                    {"open_gain", c.open_gain}});
  root["audio"] = std::move(audio);
  auto characters = Json::object();
  characters["actors"] = Json::array();
  for (const auto& a : document.characters.actors)
    characters["actors"].push_back(
        {{"id", a.id},
         {"model", a.model},
         {"initial_mark", a.initial_mark},
         {"initial_route", referenceJson(a.initial_route)},
         {"speed", a.speed},
         {"footstep_source", referenceJson(a.footstep_source)},
         {"interaction_source", referenceJson(a.interaction_source)}});
  characters["marks"] = Json::array();
  for (const auto& m : document.characters.marks)
    characters["marks"].push_back(
        {{"id", m.id},
         {"feet_position", positionJson(m.feet_position)},
         {"yaw_degrees", m.yaw_degrees}});
  characters["routes"] = Json::array();
  for (const auto& r : document.characters.routes)
    characters["routes"].push_back(
        {{"id", r.id},
         {"actor", r.actor},
         {"marks", r.marks},
         {"final_clip", referenceJson(r.final_clip)}});
  root["characters"] = std::move(characters);
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
