#include "editor/automation/application_snapshot.hpp"

#include <array>
#include <cmath>
#include <type_traits>

namespace editor_automation {

Json knownValue(Json value, std::string_view type) {
  bool finite = true;
  const auto check = [&](const auto& self, const Json& part) -> void {
    if (part.is_number_float() && !std::isfinite(part.get<double>())) finite = false;
    if (part.is_structured()) for (const auto& child : part) self(self, child);
  };
  check(check, value);
  if (!finite) return unavailableValue("Value is not finite");
  Json result{{"availability", "known"}, {"type", type}, {"value", std::move(value)}};
  if ((type == "strings" || type == "numbers") && result["value"].size() > 256) {
    result["total_count"] = result["value"].size();
    result["value"].erase(result["value"].begin() + 256, result["value"].end());
    result["availability"] = "truncated";
    result["reason"] = "Collection exceeds the observation element limit";
  }
  if (type == "strings") for (auto& part : result["value"]) {
    auto text = knownValue(part, "string");
    if (text["availability"] != "known") {
      part = text["value"];
      result["availability"] = "truncated";
      result["reason"] = "A collection member exceeds the observation byte limit";
    }
  }
  if (type == "string") {
    auto& text = result["value"].get_ref<std::string&>();
    if (text.size() > 16384) {
      std::size_t end = 16384;
      while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
      text.resize(end);
      result["availability"] = "truncated";
      result["reason"] = "UTF-8 value exceeds the observation byte limit";
    }
  }
  return result;
}

Json unavailableValue(std::string_view reason, std::string_view availability) {
  return {{"availability", availability}, {"reason", reason}};
}

std::string objectRecordType(const EditorObjectValue& value) {
  static constexpr std::array names{"solid", "entry", "point_light", "prop", "light_switch", "door",
      "audio_cue", "audio_source", "audio_room", "audio_connection", "actor", "mark", "route",
      "household_box", "readable_document", "radio"};
  return names.at(value.index());
}

namespace {
std::string pathText(const std::filesystem::path& path) {
  const auto value = path.u8string();
  return {value.begin(), value.end()};
}
Json optionalString(const std::optional<std::string>& value) {
  return knownValue(value ? Json(*value) : Json(nullptr), "optional_string");
}
Json objectFields(const EditorObjectValue& object) {
  Json fields = Json::object();
  const auto string = [&](const char* key, const std::string& value) { fields[key] = knownValue(value, "string"); };
  const auto number = [&](const char* key, float value) { fields[key] = knownValue(value, "float"); };
  const auto boolean = [&](const char* key, bool value) { fields[key] = knownValue(value, "boolean"); };
  const auto vector = [&](const char* key, float x, float y, float z) {
    fields[key] = knownValue(Json{{"x", x}, {"y", y}, {"z", z}}, "vector3");
    fields[std::string(key) + ".x"] = knownValue(x, "float");
    fields[std::string(key) + ".y"] = knownValue(y, "float");
    fields[std::string(key) + ".z"] = knownValue(z, "float");
  };
  const auto position = [&](const char* key, WorldPosition value) { vector(key, value.x, value.y, value.z); };
  const auto extent = [&](const char* key, WorldExtent value) { vector(key, value.x, value.y, value.z); };
  fields["record_type"] = knownValue(objectRecordType(object), "string");
  std::visit([&](const auto& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, PrototypeSolid>) {
      position("center", value.center); extent("half_extent", value.half_extent);
      static constexpr std::array kinds{"floor", "boundary", "obstacle", "walkable_step", "low_clearance"};
      string("kind", kinds.at(static_cast<std::size_t>(value.kind)));
      string("material", value.material);
      Json color;
      for (int i = 0; i != 4; ++i) {
        const std::string component(1, "rgba"[i]);
        color[component] = value.color[i];
        fields["color." + component] = knownValue(value.color[i], "integer");
      }
      fields["color"] = knownValue(std::move(color), "color");
    } else if constexpr (std::is_same_v<T, LevelEntry>) {
      string("id", value.id); position("foot_position", value.pose.foot_position);
      number("yaw_degrees", value.pose.yaw_degrees);
    } else if constexpr (std::is_same_v<T, PrototypePointLight>) {
      string("id", value.id); position("position", value.position);
      fields["color"] = knownValue(Json{{"r", value.color[0]}, {"g", value.color[1]}, {"b", value.color[2]}}, "color");
      for (int i = 0; i != 3; ++i) fields[std::string("color.") + "rgb"[i]] = knownValue(value.color[i], "float");
      number("intensity", value.intensity); number("radius", value.radius);
      boolean("initially_on", value.initially_on); boolean("casts_shadows", value.casts_shadows);
    } else if constexpr (std::is_same_v<T, PrototypeStaticProp>) {
      string("id", value.id); string("model", value.model); position("translation", value.translation);
      number("yaw_degrees", value.yaw_degrees); number("uniform_scale", value.uniform_scale);
      Json boxes = Json::array();
      for (const auto& box : value.collision_boxes)
        boxes.push_back({{"center", {{"x", box.center.x}, {"y", box.center.y}, {"z", box.center.z}}},
                         {"half_extent", {{"x", box.half_extent.x}, {"y", box.half_extent.y}, {"z", box.half_extent.z}}}});
      fields["collision_boxes"] = knownValue(std::move(boxes), "collision_boxes");
    } else if constexpr (std::is_same_v<T, PrototypeLightSwitch>) {
      string("id", value.id); string("light_id", value.light_id);
      position("position", value.position); number("yaw_degrees", value.yaw_degrees);
    } else if constexpr (std::is_same_v<T, DoorDefinition>) {
      string("id", value.id); position("hinge_position", value.hinge_position);
      number("closed_yaw_degrees", value.closed_yaw_degrees); number("width", value.width);
      number("height", value.height); number("thickness", value.thickness);
      number("open_angle_degrees", value.open_angle_degrees);
      number("speed_degrees_per_second", value.speed_degrees_per_second);
      static constexpr std::array sides{"none", "positive_z", "negative_z"};
      string("lock_side", sides.at(static_cast<std::size_t>(value.lock_side)));
      boolean("initially_open", value.initially_open); boolean("initially_locked", value.initially_locked);
    } else if constexpr (std::is_same_v<T, AudioCueDefinition>) {
      string("id", value.id); string("clip", value.clip); fields["caption"] = optionalString(value.caption);
      static constexpr std::array kinds{"dialogue", "essential", "ambience"};
      string("kind", kinds.at(static_cast<std::size_t>(value.kind)));
      boolean("loop", value.loop); boolean("spatial", value.spatial);
    } else if constexpr (std::is_same_v<T, AudioSourceDefinition>) {
      string("id", value.id); string("cue", value.cue); position("position", value.position);
      number("gain", value.gain); number("near_distance", value.near_distance);
      number("far_distance", value.far_distance); boolean("autoplay", value.autoplay);
    } else if constexpr (std::is_same_v<T, AudioRoomDefinition>) {
      string("id", value.id); position("center", value.center); extent("half_extent", value.half_extent);
    } else if constexpr (std::is_same_v<T, AudioConnectionDefinition>) {
      string("id", value.id); fields["room_a"] = optionalString(value.room_a);
      fields["room_b"] = optionalString(value.room_b); fields["door"] = optionalString(value.door);
      number("closed_gain", value.closed_gain); number("open_gain", value.open_gain);
    } else if constexpr (std::is_same_v<T, CharacterActorDefinition>) {
      string("id", value.id); string("model", value.model); string("initial_mark", value.initial_mark);
      fields["initial_route"] = optionalString(value.initial_route); number("speed", value.speed);
      fields["footstep_source"] = optionalString(value.footstep_source);
      fields["interaction_source"] = optionalString(value.interaction_source);
    } else if constexpr (std::is_same_v<T, CharacterMarkDefinition>) {
      string("id", value.id); position("feet_position", value.feet_position); number("yaw_degrees", value.yaw_degrees);
    } else if constexpr (std::is_same_v<T, CharacterRouteDefinition>) {
      string("id", value.id); string("actor", value.actor); fields["marks"] = knownValue(value.marks, "strings");
      fields["final_clip"] = optionalString(value.final_clip);
    } else if constexpr (std::is_same_v<T, HouseholdBoxDefinition>) {
      string("id", value.id); position("center", value.center); number("yaw_degrees", value.yaw_degrees);
    } else if constexpr (std::is_same_v<T, HouseholdDocumentDefinition>) {
      string("id", value.id); position("position", value.position); number("yaw_degrees", value.yaw_degrees);
      string("title", value.title); fields["pages"] = knownValue(value.pages, "strings");
    } else if constexpr (std::is_same_v<T, HouseholdRadioDefinition>) {
      string("id", value.id); string("prop", value.prop); string("source", value.source);
      boolean("initially_on", value.initially_on);
    }
  }, object);
  return fields;
}
}  // namespace

Json captureApplication(const EditorDocument& document, const ObjectRefs& refs, const Json& preview_fields) {
  Json result = Json::array();
  const auto field = [&](std::string_view projection, std::string_view key, Json value, Json owner = nullptr) {
    result.push_back({{"projection", projection}, {"field", key}, {"object_ref", std::move(owner)},
                      {"value", std::move(value)},
                      {"source", projection == "object" || projection == "objects" ? "document" : projection}});
  };
  const auto doc = [&](std::string_view key, Json value, std::string_view type) {
    field("document", key, knownValue(std::move(value), type));
  };
  doc("generation", std::to_string(document.generation()), "string");
  doc("revision", std::to_string(document.revision()), "string");
  doc("object_revision", std::to_string(document.objectRevision()), "string");
  doc("path", document.path() ? Json(pathText(*document.path())) : Json(nullptr), "optional_string");
  field("document", "source_version", document.document() ? knownValue(document.sourceVersion(), "integer") :
        unavailableValue("No document is open", "not_applicable"));
  doc("dirty", document.dirty(), "boolean"); doc("valid", document.valid(), "boolean");
  static constexpr std::array pending{"none", "open", "new_interior", "close", "exit"};
  doc("pending_action", pending.at(static_cast<std::size_t>(document.pendingAction().kind)), "string");
  doc("validation_summary", formatLevelDiagnostics(document.diagnostics()), "string");
  if (document.document()) {
    const auto& level = *document.document();
    doc("default_entry", level.default_entry, "string");
    doc("ambient_intensity", level.environment_light.ambient_intensity, "float");
    doc("terrain_material", level.terrain ? Json(level.terrain->material) : Json(nullptr), "optional_string");
    for (const auto& [key, count] : std::initializer_list<std::pair<const char*, std::size_t>>{
        {"solid_count", level.solids.size()}, {"entry_count", level.entries.size()},
        {"light_count", level.environment_light.point_lights.size()}, {"switch_count", level.light_switches.size()},
        {"door_count", level.doors.size()}, {"prop_count", level.props.size()},
        {"audio_cue_count", level.audio.cues.size()}, {"audio_source_count", level.audio.sources.size()},
        {"audio_room_count", level.audio.rooms.size()}, {"audio_connection_count", level.audio.connections.size()},
        {"actor_count", level.characters.actors.size()}, {"mark_count", level.characters.marks.size()},
        {"route_count", level.characters.routes.size()}, {"box_count", level.household.boxes.size()},
        {"document_count", level.household.documents.size()}, {"radio_count", level.household.radios.size()}})
      doc(key, count, "integer");
  }
  const auto selected = refs.find(document.selection());
  field("selection", "object_ref", knownValue(selected == refs.end() ? Json(nullptr) : Json(selected->second), "ref"));
  field("selection", "revision", knownValue(std::to_string(document.selectionRevision()), "string"));
  const auto object = document.object(document.selection());
  field("selection", "record_type", object ? knownValue(objectRecordType(*object), "string") :
        unavailableValue("No selected object", "not_applicable"));
  field("history", "can_undo", knownValue(document.canUndo(), "boolean"));
  field("history", "can_redo", knownValue(document.canRedo(), "boolean"));
  field("history", "summary", knownValue(std::string(document.canUndo() ? "Undo available" : "No undo") +
      (document.canRedo() ? "; redo available" : "; no redo"), "string"));
  for (const auto& [id, ref] : refs) {
    const auto record = document.object(id);
    if (!record) continue;
    Json fields = objectFields(*record);
    fields["ref"] = knownValue(ref, "ref");
    for (auto it = fields.begin(); it != fields.end(); ++it)
      field("object", it.key(), it.value(), ref);
  }
  for (const auto& diagnostic : document.diagnostics()) {
    static constexpr std::array categories{"parse", "validation", "filesystem"};
    field("diagnostics", "category", knownValue(categories.at(static_cast<std::size_t>(diagnostic.category)), "string"));
    field("diagnostics", "source_path", knownValue(pathText(diagnostic.source_path), "string"));
    field("diagnostics", "document_path", knownValue(diagnostic.document_path, "string"));
    field("diagnostics", "message", knownValue(diagnostic.message, "string"));
    if (diagnostic.terrain_location) {
      field("diagnostics", "terrain_x", knownValue(diagnostic.terrain_location->x, "integer"));
      field("diagnostics", "terrain_z", knownValue(diagnostic.terrain_location->z, "integer"));
      field("diagnostics", "terrain_triangle", diagnostic.terrain_location->triangle ?
          knownValue(*diagnostic.terrain_location->triangle, "integer") : unavailableValue("Not a triangle", "not_applicable"));
    }
  }
  field("diagnostics", "edit_error", knownValue(document.editError(), "string"));
  for (auto it = preview_fields.begin(); it != preview_fields.end(); ++it)
    field("preview", it.key(), it.value());
  return result;
}
}  // namespace editor_automation
