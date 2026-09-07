#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>
#include <utility>

#include "core/world/door.hpp"
#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
#include "core/world/scene_assets.hpp"
#include "editor/editor_document.hpp"

namespace {
bool finite(WorldPosition p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}

bool positive(WorldExtent e) {
  return std::isfinite(e.x) && std::isfinite(e.y) && std::isfinite(e.z) &&
         e.x > 0.0F && e.y > 0.0F && e.z > 0.0F;
}

bool finiteBounds(WorldPosition p, WorldExtent e) {
  return finite({p.x - e.x, p.y - e.y, p.z - e.z}) &&
         finite({p.x + e.x, p.y + e.y, p.z + e.z}) &&
         finite({2.0F * e.x, 2.0F * e.y, 2.0F * e.z});
}
}  // namespace

std::optional<EditorObjectValue> editorPlacedObject(
    EditorObjectValue value, const EditorSurfaceHit& hit,
    const EditorPlacementOffsets& offsets) {
  if (!finite(hit.position) || !finite(hit.normal) ||
      !std::isfinite(offsets.height) || !std::isfinite(offsets.outward) ||
      offsets.outward < 0 || !std::isfinite(offsets.door_clearance) ||
      offsets.door_clearance < 0 || hit.face == EditorSurfaceFace::Bottom)
    return std::nullopt;
  const bool top = hit.face == EditorSurfaceFace::Top ||
                   hit.face == EditorSurfaceFace::Terrain;
  bool available = true;
  std::visit(
      [&](auto& object) {
        using T = std::decay_t<decltype(object)>;
        if constexpr (std::is_same_v<T, PrototypeSolid>) {
          object.center = hit.position;
          if (top)
            object.center.y += object.half_extent.y;
          else {
            object.center.x += hit.normal.x * object.half_extent.x;
            object.center.z += hit.normal.z * object.half_extent.z;
          }
        } else if constexpr (std::is_same_v<T, LevelEntry>) {
          if (top)
            object.pose.foot_position = hit.position;
          else
            available = false;
        } else if constexpr (std::is_same_v<T, DoorDefinition>) {
          if (top) {
            object.hinge_position = hit.position;
            object.hinge_position.y += offsets.door_clearance;
          } else
            available = false;
        } else if constexpr (std::is_same_v<T, PrototypeStaticProp>) {
          if (top)
            object.translation = hit.position;
          else
            available = false;
        } else if constexpr (std::is_same_v<T, AudioCueDefinition> ||
                             std::is_same_v<T, AudioRoomDefinition> ||
                             std::is_same_v<T, AudioConnectionDefinition>) {
          available = false;
        } else {
          object.position = hit.position;
          if (top)
            object.position.y += offsets.height;
          else if constexpr (std::is_same_v<T, PrototypeLightSwitch>) {
            object.yaw_degrees =
                static_cast<float>(std::atan2(hit.normal.x, hit.normal.z) *
                                   180 / std::numbers::pi);
            const float outward = light_switch_half_extent.z + 0.001F;
            object.position.x += hit.normal.x * outward;
            object.position.z += hit.normal.z * outward;
          } else {
            object.position.x += hit.normal.x * offsets.outward;
            object.position.z += hit.normal.z * offsets.outward;
          }
        }
      },
      value);
  if (!available || !editorObjectFieldError(value).empty()) return std::nullopt;
  return value;
}

std::string editorObjectFieldError(const EditorObjectValue& value) {
  if (editorAudioKind(value)) return editorAudioFieldError(value);
  return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrototypeSolid>) {
          if (!finite(v.center)) return "Solid center must be finite.";
          if (!positive(v.half_extent))
            return "Solid half extents must be finite and positive.";
          if (!finiteBounds(v.center, v.half_extent))
            return "Solid bounds overflow; reduce center or half extents.";
          switch (v.kind) {
            case PrototypeSolidKind::Floor:
            case PrototypeSolidKind::Boundary:
            case PrototypeSolidKind::Obstacle:
            case PrototypeSolidKind::WalkableStep:
            case PrototypeSolidKind::LowClearance:
              break;
            default:
              return "Unsupported solid kind.";
          }
          // Unknown material IDs remain repairable document diagnostics.
        } else if constexpr (std::is_same_v<T, LevelEntry>) {
          if (!finite(v.pose.foot_position))
            return "Spawn foot position must be finite.";
          if (!levelEntryIdIsValid(v.id))
            return "Entry ID must match [a-z][a-z0-9-]{0,63}.";
          if (!std::isfinite(v.pose.yaw_degrees))
            return "Entry yaw must be finite.";
        } else if constexpr (std::is_same_v<T, PrototypePointLight>) {
          if (!levelEntryIdIsValid(v.id))
            return "Light ID must match [a-z][a-z0-9-]{0,63}.";
          if (auto error = pointLightFieldError(v); !error.empty())
            return error;
        } else if constexpr (std::is_same_v<T, PrototypeLightSwitch>) {
          if (!levelEntryIdIsValid(v.id))
            return "Switch ID must match [a-z][a-z0-9-]{0,63}.";
          if (!v.light_id.empty() && !levelEntryIdIsValid(v.light_id))
            return "Linked light ID must match [a-z][a-z0-9-]{0,63}.";
          if (!finite(v.position)) return "Switch position must be finite.";
          if (!std::isfinite(v.yaw_degrees))
            return "Switch yaw must be finite.";
          if (!lightSwitchIsValid(v))
            return "Switch bounds must remain finite.";
        } else if constexpr (std::is_same_v<T, DoorDefinition>) {
          if (!levelEntryIdIsValid(v.id))
            return "Door ID must match [a-z][a-z0-9-]{0,63}.";
          // Initial lock/open contradictions are cross-field validation errors.
          if (auto error = doorFieldError(v); !error.empty()) return error;
        } else if constexpr (std::is_same_v<T, PrototypeStaticProp>) {
          if (!levelEntryIdIsValid(v.id))
            return "Prop ID must match [a-z][a-z0-9-]{0,63}.";
          if (!finite(v.translation)) return "Prop translation must be finite.";
          if (!std::isfinite(v.yaw_degrees)) return "Prop yaw must be finite.";
          if (!std::isfinite(v.uniform_scale) || v.uniform_scale <= 0.0F)
            return "Prop scale must be finite and positive.";
          if (v.collision_boxes.size() > 8)
            return "A prop supports at most eight collision boxes.";
          const double yaw =
              static_cast<double>(v.yaw_degrees) * std::numbers::pi / 180;
          const float c = static_cast<float>(std::abs(std::cos(yaw)));
          const float s = static_cast<float>(std::abs(std::sin(yaw)));
          const auto safe_bounds = [&](const PropCollisionBox& bounds) {
            const auto extent = propBoxWorldHalfExtent(v, bounds);
            return finiteBounds(propBoxWorldCenter(v, bounds),
                                {c * extent.x + s * extent.z, extent.y,
                                 s * extent.x + c * extent.z});
          };
          for (const auto& proxy : v.collision_boxes) {
            if (!finite(proxy.center) || !positive(proxy.half_extent))
              return "Proxy centers must be finite and half extents positive.";
            if (!safe_bounds(proxy))
              return "Transformed prop proxy overflows; reduce transform or "
                     "dimensions.";
          }
          if (const auto* model = findSceneModel(v.model);
              model && !safe_bounds(sceneModelBounds(*model)))
            return "Transformed model bounds overflow; reduce the transform.";
        }
        return {};
      },
      value);
}

void EditorDocument::resetEditing() {
  ++generation_;
  solid_ids_.clear();
  entry_ids_.clear();
  door_ids_.clear();
  prop_ids_.clear();
  light_ids_.clear();
  switch_ids_.clear();
  launch_entry_ = document_ ? document_->default_entry : std::string{};
  next_object_id_ = editor_first_solid;
  if (document_) {
    for (std::size_t i = 0; i < document_->solids.size(); ++i) {
      solid_ids_.push_back(next_object_id_++);
    }
  }
  if (document_) {
    for (std::size_t i = 0; i < document_->entries.size(); ++i)
      entry_ids_.push_back(i == 0 ? editor_spawn : next_object_id_++);
  }
  if (document_) {
    for (std::size_t i = 0; i < document_->doors.size(); ++i)
      door_ids_.push_back(next_object_id_++);
    for (std::size_t i = 0; i < document_->props.size(); ++i)
      prop_ids_.push_back(i == 0 ? editor_prop : next_object_id_++);
    for (std::size_t i = 0;
         i < document_->environment_light.point_lights.size(); ++i)
      light_ids_.push_back(next_object_id_++);
    for (std::size_t i = 0; i < document_->light_switches.size(); ++i)
      switch_ids_.push_back(next_object_id_++);
  }
  selection_ = editor_no_object;
  resetAudioIds();
  history_.clear();
  terrain_stroke_.reset();
  history_position_ = 0;
  current_revision_ = ++next_revision_;
  saved_revision_ = current_revision_;
  ++revision_;
  ++object_revision_;
  refreshValidation();
}

void EditorDocument::refreshValidation() {
  diagnostics_ = document_
                     ? validateLevelDocument(
                           *document_, path_.value_or(std::filesystem::path{}))
                     : std::vector<LevelDiagnostic>{};
  valid_ = document_.has_value() && diagnostics_.empty();
  edit_error_.clear();
}

std::optional<std::size_t> EditorDocument::solidIndex(EditorObjectId id) const {
  const auto found = std::find(solid_ids_.begin(), solid_ids_.end(), id);
  if (found == solid_ids_.end()) return std::nullopt;
  return static_cast<std::size_t>(found - solid_ids_.begin());
}

std::optional<std::size_t> EditorDocument::entryIndex(EditorObjectId id) const {
  const auto found = std::find(entry_ids_.begin(), entry_ids_.end(), id);
  if (found == entry_ids_.end()) return std::nullopt;
  return static_cast<std::size_t>(found - entry_ids_.begin());
}

std::optional<std::size_t> EditorDocument::doorIndex(EditorObjectId id) const {
  const auto found = std::find(door_ids_.begin(), door_ids_.end(), id);
  if (found == door_ids_.end()) return std::nullopt;
  return static_cast<std::size_t>(found - door_ids_.begin());
}

std::optional<std::size_t> EditorDocument::propIndex(EditorObjectId id) const {
  const auto found = std::find(prop_ids_.begin(), prop_ids_.end(), id);
  if (found == prop_ids_.end()) return std::nullopt;
  return static_cast<std::size_t>(found - prop_ids_.begin());
}

bool EditorDocument::addDoor() {
  static_cast<void>(finishTerrainStroke());
  if (!document_ || document_->doors.size() >= 32) return false;
  DoorDefinition door;
  for (std::size_t i = 1;; ++i) {
    door.id = "door-" + std::to_string(i);
    if (std::none_of(document_->doors.begin(), document_->doors.end(),
                     [&](const auto& d) { return d.id == door.id; }))
      break;
  }
  const auto* entry = findLevelEntry(*document_, document_->default_entry);
  door.hinge_position = entry ? entry->pose.foot_position : WorldPosition{};
  door.hinge_position.x += 1.5F;
  door.hinge_position.y += 0.02F;
  const auto id = next_object_id_++;
  return commit(
      {id, document_->doors.size(), std::nullopt, door, selection_, id});
}

bool EditorDocument::addProp(std::string_view model) {
  static_cast<void>(finishTerrainStroke());
  const auto* asset = findSceneModel(model);
  if (!document_ || !asset || document_->props.size() >= 128) return false;
  PrototypeStaticProp prop;
  prop.model = model;
  prop.collision_boxes.assign(asset->default_boxes.begin(),
                              asset->default_boxes.end());
  for (std::size_t i = 1;; ++i) {
    prop.id = "prop-" + std::to_string(i);
    if (std::none_of(document_->props.begin(), document_->props.end(),
                     [&](const auto& p) { return p.id == prop.id; }))
      break;
  }
  const auto* entry = findLevelEntry(*document_, document_->default_entry);
  prop.translation = entry ? entry->pose.foot_position : WorldPosition{};
  prop.translation.x += 1.5F;
  const auto id = next_object_id_++;
  return commit(
      {id, document_->props.size(), std::nullopt, prop, selection_, id});
}

bool EditorDocument::setTerrainMaterial(std::string material) {
  static_cast<void>(finishTerrainStroke());
  if (!document_ || !document_->terrain || !findStructuralMaterial(material) ||
      document_->terrain->material == material)
    return false;
  Edit edit;
  edit.selection_before = edit.selection_after = selection_;
  edit.terrain_material_before = document_->terrain->material;
  edit.terrain_material_after = std::move(material);
  return commit(std::move(edit));
}

bool EditorDocument::selectLaunchEntry(std::string_view id) {
  if (!document_ || !findLevelEntry(*document_, id)) return false;
  launch_entry_ = id;
  return true;
}

bool EditorDocument::addEntry(PrototypePlayerSpawn pose) {
  static_cast<void>(finishTerrainStroke());
  if (!document_ || document_->entries.size() >= level_maximum_entry_count) {
    edit_error_ = "A level supports at most 16 entries.";
    return false;
  }
  std::string name;
  for (std::size_t i = 1;; ++i) {
    name = "entry-" + std::to_string(i);
    if (!findLevelEntry(*document_, name)) break;
  }
  LevelEntry entry{std::move(name), pose};
  edit_error_ = editorObjectFieldError(entry);
  if (!edit_error_.empty()) return false;
  const auto id = next_object_id_++;
  return commit(
      {id, document_->entries.size(), std::nullopt, entry, selection_, id});
}

bool EditorDocument::makeSelectedEntryDefault() {
  static_cast<void>(finishTerrainStroke());
  const auto index = entryIndex(selection_);
  if (!index || document_->default_entry == document_->entries[*index].id)
    return false;
  Edit edit;
  edit.selection_before = edit.selection_after = selection_;
  edit.default_before = document_->default_entry;
  edit.default_after = document_->entries[*index].id;
  return commit(std::move(edit));
}

std::optional<EditorObjectValue> EditorDocument::object(
    EditorObjectId id) const {
  if (!document_) return std::nullopt;
  if (const auto index = entryIndex(id)) return document_->entries[*index];
  if (const auto index = lightIndex(id))
    return document_->environment_light.point_lights[*index];
  if (const auto index = propIndex(id)) return document_->props[*index];
  if (const auto index = doorIndex(id)) return document_->doors[*index];
  if (const auto index = switchIndex(id))
    return document_->light_switches[*index];
  if (const auto index = solidIndex(id)) return document_->solids[*index];
  return audioObject(id);
}

void EditorDocument::select(EditorObjectId id) {
  selection_ = object(id) ? id : editor_no_object;
}

bool EditorDocument::replaceObject(EditorObjectId id, EditorObjectValue value) {
  static_cast<void>(finishTerrainStroke());
  const auto before = object(id);
  if (!before || before->index() != value.index()) {
    edit_error_ = "Select an existing object of the matching type.";
    return false;
  }
  edit_error_ = editorObjectFieldError(value);
  if (!edit_error_.empty()) return false;
  if (*before == value) return false;
  if (const auto kind = editorAudioKind(value)) {
    for (const auto other_id : audioIds(*kind)) {
      if (other_id == id) continue;
      const auto other = *audioObject(other_id);
      const auto name = [](const EditorObjectValue& v) {
        return std::visit(
            [](const auto& object) -> std::string {
              if constexpr (requires { object.id; })
                return object.id;
              else
                return {};
            },
            v);
      };
      if (name(other) == name(value)) {
        edit_error_ = "Audio ID is already in use.";
        return false;
      }
    }
  }
  Edit edit{id,
            solidIndex(id).value_or(entryIndex(id).value_or(
                doorIndex(id).value_or(propIndex(id).value_or(
                    lightIndex(id).value_or(switchIndex(id).value_or(0)))))),
            before,
            std::move(value),
            selection_,
            selection_};
  if (!prepareLightingEdit(edit)) return false;
  if (const auto* entry = std::get_if<LevelEntry>(&*edit.after)) {
    const auto& old = std::get<LevelEntry>(*before);
    if (old.id != entry->id && findLevelEntry(*document_, entry->id)) {
      edit_error_ = "Entry ID is already in use.";
      return false;
    }
    if (old.id == document_->default_entry) {
      edit.default_before = old.id;
      edit.default_after = entry->id;
    }
  }
  if (const auto* door = std::get_if<DoorDefinition>(&*edit.after)) {
    const auto& old = std::get<DoorDefinition>(*before);
    if (old.id != door->id &&
        std::any_of(document_->doors.begin(), document_->doors.end(),
                    [&](const auto& other) { return other.id == door->id; })) {
      edit_error_ = "Door ID is already in use.";
      return false;
    }
  }
  if (const auto* prop = std::get_if<PrototypeStaticProp>(&*edit.after)) {
    const auto& old = std::get<PrototypeStaticProp>(*before);
    if (old.id != prop->id &&
        std::any_of(document_->props.begin(), document_->props.end(),
                    [&](const auto& other) { return other.id == prop->id; })) {
      edit_error_ = "Prop ID is already in use.";
      return false;
    }
  }
  auto updated_audio = document_->audio;
  renameAudioReferences(updated_audio, *edit.before, *edit.after);
  if (updated_audio != document_->audio) {
    edit.audio_before = document_->audio;
    edit.audio_after = std::move(updated_audio);
  }
  return commit(std::move(edit));
}

bool EditorDocument::addSolid(PrototypeSolid solid) {
  static_cast<void>(finishTerrainStroke());
  if (!document_ || document_->solids.size() >= level_maximum_solid_count) {
    edit_error_ =
        "Open a level with fewer than 240 solids before adding a solid.";
    return false;
  }
  edit_error_ = editorObjectFieldError(solid);
  if (!edit_error_.empty()) return false;
  const EditorObjectId id = next_object_id_++;
  return commit(
      {id, document_->solids.size(), std::nullopt, solid, selection_, id});
}

bool EditorDocument::duplicateSelected() {
  static_cast<void>(finishTerrainStroke());
  if (!document_) return false;
  if (const auto index = lightIndex(selection_)) {
    auto copy = document_->environment_light.point_lights[*index];
    copy.position.x += 1.5F;
    return addPointLight(std::move(copy));
  }
  if (const auto index = switchIndex(selection_)) {
    auto copy = document_->light_switches[*index];
    copy.position.x += .5F;
    return addLightSwitch(std::move(copy));
  }
  if (auto value = audioObject(selection_))
    return addAudioObject(std::move(*value));
  if (const auto index = doorIndex(selection_)) {
    if (document_->doors.size() >= 32) return false;
    auto copy = document_->doors[*index];
    copy.hinge_position.x += copy.width + 0.5F;
    for (std::size_t i = 1;; ++i) {
      copy.id = "door-" + std::to_string(i);
      if (std::none_of(document_->doors.begin(), document_->doors.end(),
                       [&](const auto& d) { return d.id == copy.id; }))
        break;
    }
    if (!editorObjectFieldError(copy).empty()) return false;
    const auto id = next_object_id_++;
    return commit(
        {id, document_->doors.size(), std::nullopt, copy, selection_, id});
  }
  if (const auto index = propIndex(selection_)) {
    if (document_->props.size() >= 128) return false;
    auto copy = document_->props[*index];
    copy.translation.x += 1.5F;
    for (std::size_t i = 1;; ++i) {
      copy.id = "prop-" + std::to_string(i);
      if (std::none_of(document_->props.begin(), document_->props.end(),
                       [&](const auto& p) { return p.id == copy.id; }))
        break;
    }
    if (!editorObjectFieldError(copy).empty()) return false;
    const auto id = next_object_id_++;
    return commit(
        {id, document_->props.size(), std::nullopt, copy, selection_, id});
  }
  if (const auto index = entryIndex(selection_))
    return addEntry(document_->entries[*index].pose);
  const auto index = solidIndex(selection_);
  if (!index) return false;
  PrototypeSolid solid = document_->solids[*index];
  solid.center.x += solid.half_extent.x * 2.0F + 0.5F;
  return addSolid(solid);
}

bool EditorDocument::removeSelected() {
  static_cast<void>(finishTerrainStroke());
  if (auto value = audioObject(selection_)) {
    const auto& ids = audioIds(*editorAudioKind(*value));
    const auto index = static_cast<std::size_t>(
        std::find(ids.begin(), ids.end(), selection_) - ids.begin());
    return commit(
        {selection_, index, value, std::nullopt, selection_, editor_no_object});
  }
  if (const auto index = doorIndex(selection_))
    return commit({selection_, *index, document_->doors[*index], std::nullopt,
                   selection_, editor_no_object});
  if (const auto index = propIndex(selection_))
    return commit({selection_, *index, document_->props[*index], std::nullopt,
                   selection_, editor_no_object});
  if (const auto index = lightIndex(selection_))
    return commit({selection_, *index,
                   document_->environment_light.point_lights[*index],
                   std::nullopt, selection_, editor_no_object});
  if (const auto index = switchIndex(selection_))
    return commit({selection_, *index, document_->light_switches[*index],
                   std::nullopt, selection_, editor_no_object});
  if (const auto index = entryIndex(selection_)) {
    const auto& entry = document_->entries[*index];
    if (document_->entries.size() == 1 ||
        entry.id == document_->default_entry) {
      edit_error_ = "Choose another default entry before deleting this entry.";
      return false;
    }
    return commit({selection_, *index, entry, std::nullopt, selection_,
                   editor_no_object});
  }
  const auto index = solidIndex(selection_);
  if (!index) return false;
  return commit({selection_, *index, document_->solids[*index], std::nullopt,
                 selection_, editor_no_object});
}

bool EditorDocument::placeSelected(WorldPosition terrain_hit) {
  auto value = object(selection_);
  if (!value || !document_->terrain || !finite(terrain_hit) ||
      !prototypeTerrainContains(*document_->terrain, terrain_hit.x,
                                terrain_hit.z))
    return false;
  terrain_hit.y = prototypeTerrainHeightAt(*document_->terrain, terrain_hit.x,
                                           terrain_hit.z);
  std::visit(
      [&](auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrototypeSolid>) {
          v.center = terrain_hit;
          v.center.y += v.half_extent.y;
        } else if constexpr (std::is_same_v<T, LevelEntry>) {
          v.pose.foot_position = terrain_hit;
        } else if constexpr (std::is_same_v<T, PrototypePointLight> ||
                             std::is_same_v<T, AudioSourceDefinition> ||
                             std::is_same_v<T, PrototypeLightSwitch>) {
          const float offset = v.position.y - prototypeTerrainHeightAt(
                                                  *document_->terrain,
                                                  v.position.x, v.position.z);
          v.position = terrain_hit;
          v.position.y += offset;
        } else if constexpr (std::is_same_v<T, DoorDefinition>) {
          v.hinge_position = terrain_hit;
          v.hinge_position.y += 0.02F;
        } else if constexpr (std::is_same_v<T, PrototypeStaticProp>) {
          v.translation = terrain_hit;
        }
      },
      *value);
  return replaceObject(selection_, std::move(*value));
}

bool EditorDocument::commit(Edit edit) {
  edit.revision_before = current_revision_;
  edit.revision_after = ++next_revision_;
  history_.erase(
      history_.begin() + static_cast<std::ptrdiff_t>(history_position_),
      history_.end());
  history_.push_back(std::move(edit));
  applyEdit(history_.back(), true);
  if (history_.size() > 128) history_.pop_front();
  history_position_ = history_.size();
  return true;
}

void EditorDocument::applyEdit(const Edit& edit, bool forward) {
  const auto& value = forward ? edit.after : edit.before;
  if (edit.audio_after)
    document_->audio = forward ? *edit.audio_after : *edit.audio_before;
  if (applyLightingEdit(edit, forward)) {
    // Lighting uses the same revision, selection and validation below.
  } else if (applyAudioEdit(edit, forward)) {
    // Audio commands share the same revision, selection and validation below.
  } else if (edit.brush) {
    for (const auto& sample : edit.terrain)
      document_->terrain->heights[sample.index] =
          forward ? sample.after : sample.before;
    terrain_brush_ = *edit.brush;
  } else if (edit.id == editor_no_object && edit.default_after) {
    // A default-only command shares the ordinary history revision.
  } else if ((value && std::holds_alternative<LevelEntry>(*value)) ||
             entryIndex(edit.id)) {
    const auto index = entryIndex(edit.id);
    if (!value) {
      document_->entries.erase(document_->entries.begin() +
                               static_cast<std::ptrdiff_t>(*index));
      entry_ids_.erase(entry_ids_.begin() +
                       static_cast<std::ptrdiff_t>(*index));
    } else if (index) {
      const auto& entry = std::get<LevelEntry>(*value);
      if (launch_entry_ == document_->entries[*index].id)
        launch_entry_ = entry.id;
      document_->entries[*index] = entry;
    } else {
      document_->entries.insert(
          document_->entries.begin() + static_cast<std::ptrdiff_t>(edit.index),
          std::get<LevelEntry>(*value));
      entry_ids_.insert(
          entry_ids_.begin() + static_cast<std::ptrdiff_t>(edit.index),
          edit.id);
    }
  } else if (edit.terrain_material_after) {
    document_->terrain->material =
        forward ? *edit.terrain_material_after : *edit.terrain_material_before;
  } else if ((value && std::holds_alternative<DoorDefinition>(*value)) ||
             doorIndex(edit.id)) {
    const auto index = doorIndex(edit.id);
    if (!value) {
      document_->doors.erase(document_->doors.begin() +
                             static_cast<std::ptrdiff_t>(*index));
      door_ids_.erase(door_ids_.begin() + static_cast<std::ptrdiff_t>(*index));
    } else if (index)
      document_->doors[*index] = std::get<DoorDefinition>(*value);
    else {
      document_->doors.insert(
          document_->doors.begin() + static_cast<std::ptrdiff_t>(edit.index),
          std::get<DoorDefinition>(*value));
      door_ids_.insert(
          door_ids_.begin() + static_cast<std::ptrdiff_t>(edit.index), edit.id);
    }
  } else if ((value && std::holds_alternative<PrototypeStaticProp>(*value)) ||
             propIndex(edit.id)) {
    const auto index = propIndex(edit.id);
    if (!value) {
      document_->props.erase(document_->props.begin() +
                             static_cast<std::ptrdiff_t>(*index));
      prop_ids_.erase(prop_ids_.begin() + static_cast<std::ptrdiff_t>(*index));
    } else if (index)
      document_->props[*index] = std::get<PrototypeStaticProp>(*value);
    else {
      document_->props.insert(
          document_->props.begin() + static_cast<std::ptrdiff_t>(edit.index),
          std::get<PrototypeStaticProp>(*value));
      prop_ids_.insert(
          prop_ids_.begin() + static_cast<std::ptrdiff_t>(edit.index), edit.id);
    }
  } else if (edit.id >= editor_first_solid) {
    const auto index = solidIndex(edit.id);
    if (!value) {
      document_->solids.erase(document_->solids.begin() +
                              static_cast<std::ptrdiff_t>(*index));
      solid_ids_.erase(solid_ids_.begin() +
                       static_cast<std::ptrdiff_t>(*index));
    } else if (index) {
      document_->solids[*index] = std::get<PrototypeSolid>(*value);
    } else {
      document_->solids.insert(
          document_->solids.begin() + static_cast<std::ptrdiff_t>(edit.index),
          std::get<PrototypeSolid>(*value));
      solid_ids_.insert(
          solid_ids_.begin() + static_cast<std::ptrdiff_t>(edit.index),
          edit.id);
    }
  }
  if (edit.default_after)
    document_->default_entry =
        forward ? *edit.default_after : *edit.default_before;
  if (!findLevelEntry(*document_, launch_entry_))
    launch_entry_ = document_->default_entry;
  select(forward ? edit.selection_after : edit.selection_before);
  current_revision_ = forward ? edit.revision_after : edit.revision_before;
  ++revision_;
  if (!edit.brush) ++object_revision_;
  refreshValidation();
}

bool EditorDocument::undo() {
  static_cast<void>(finishTerrainStroke());
  if (!canUndo()) return false;
  applyEdit(history_[--history_position_], false);
  return true;
}

bool EditorDocument::redo() {
  static_cast<void>(finishTerrainStroke());
  if (!canRedo()) return false;
  applyEdit(history_[history_position_++], true);
  return true;
}

bool EditorDocument::setTerrainBrush(const EditorTerrainBrush& brush) {
  return commitEditorBrush(terrain_brush_, brush, edit_error_);
}

void EditorDocument::beginTerrainStroke(std::optional<WorldPosition> hit) {
  if (!document_ || !document_->terrain || terrain_stroke_) return;
  terrain_stroke_.emplace();
  terrain_stroke_->brush = terrain_brush_;
  stroke_selection_ = selection_;
  extendTerrainStroke(hit);
}

void EditorDocument::extendTerrainStroke(std::optional<WorldPosition> hit) {
  if (terrain_stroke_ && terrain_stroke_->advance(*document_->terrain, hit))
    ++revision_;
}

bool EditorDocument::finishTerrainStroke() {
  if (!terrain_stroke_) return false;
  Edit edit;
  edit.terrain = terrain_stroke_->changes(*document_->terrain);
  edit.brush = terrain_stroke_->brush;
  edit.selection_before = stroke_selection_;
  edit.selection_after = selection_;
  terrain_stroke_.reset();
  if (edit.terrain.empty()) return false;
  // Samples are already visible; commit installs the same final values and
  // assigns one saved-state revision for the entire gesture.
  const auto next_brush = terrain_brush_;
  const bool committed = commit(std::move(edit));
  terrain_brush_ = next_brush;
  return committed;
}
