#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

#include "core/world/light_switch.hpp"
#include "core/world/prototype_level.hpp"
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

std::string editorObjectFieldError(const EditorObjectValue& value) {
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
          if (!prototypeSurfaceIsValid(v.surface))
            return "Unsupported solid surface.";
        } else if constexpr (std::is_same_v<T, PrototypePlayerSpawn>) {
          if (!finite(v.foot_position))
            return "Spawn foot position must be finite.";
          if (!std::isfinite(v.yaw_degrees)) return "Spawn yaw must be finite.";
        } else if constexpr (std::is_same_v<T, PrototypePointLight>) {
          if (!finite(v.position)) return "Light position must be finite.";
          if (!std::all_of(v.color.begin(), v.color.end(), [](float c) {
                return std::isfinite(c) && c >= 0.0F;
              }))
            return "Light color must be finite and non-negative.";
          if (!std::isfinite(v.intensity) || v.intensity <= 0.0F)
            return "Light intensity must be finite and positive.";
          if (!std::isfinite(v.radius) || v.radius <= 0.0F)
            return "Light radius must be finite and positive.";
        } else if constexpr (std::is_same_v<T, PrototypeLightSwitch>) {
          if (!finite(v.position)) return "Switch position must be finite.";
          if (!std::isfinite(v.yaw_degrees))
            return "Switch yaw must be finite.";
          if (v.point_light_index >= prototype_point_light_count)
            return "Switch must link Point light 1 or 2.";
          if (!lightSwitchIsValid(v))
            return "Switch bounds must remain finite.";
        } else {
          if (!finite(v.translation)) return "Prop translation must be finite.";
          if (!std::isfinite(v.yaw_degrees)) return "Prop yaw must be finite.";
          if (!std::isfinite(v.uniform_scale) || v.uniform_scale <= 0.0F)
            return "Prop scale must be finite and positive.";
          if (v.surface != PrototypeSurface::Obstacle)
            return "The prop uses the obstacle surface.";
          if (!finite(v.box_proxy_center))
            return "Proxy center must be finite.";
          if (!positive(v.box_proxy_half_extent))
            return "Proxy half extents must be finite and positive.";
          if (!prototypeStaticPropIsValid(v) ||
              !finiteBounds(prototypeStaticPropProxyWorldCenter(v),
                            prototypeStaticPropProxyWorldHalfExtent(v))) {
            return "Transformed prop proxy overflows; reduce the transform or "
                   "proxy dimensions.";
          }
        }
        return {};
      },
      value);
}

void EditorDocument::resetEditing() {
  solid_ids_.clear();
  next_object_id_ = editor_first_solid;
  if (document_) {
    for (std::size_t i = 0; i < document_->solids.size(); ++i) {
      solid_ids_.push_back(next_object_id_++);
    }
  }
  selection_ = editor_no_object;
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

std::optional<EditorObjectValue> EditorDocument::object(
    EditorObjectId id) const {
  if (!document_) return std::nullopt;
  if (id == editor_spawn) return document_->player_spawn;
  if (id >= editor_first_light && id < editor_prop) {
    return document_->environment_light.point_lights[id - editor_first_light];
  }
  if (id == editor_prop) return document_->static_prop;
  if (id == editor_light_switch && document_->light_switch)
    return *document_->light_switch;
  if (const auto index = solidIndex(id)) return document_->solids[*index];
  return std::nullopt;
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
  return commit({id, solidIndex(id).value_or(0), before, std::move(value),
                 selection_, selection_});
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
  const auto index = solidIndex(selection_);
  if (!index) return false;
  PrototypeSolid solid = document_->solids[*index];
  solid.center.x += solid.half_extent.x * 2.0F + 0.5F;
  return addSolid(solid);
}

bool EditorDocument::addLightSwitch() {
  static_cast<void>(finishTerrainStroke());
  if (!document_ || document_->light_switch) return false;
  auto position = document_->player_spawn.foot_position;
  position.y += 1.65F;
  return commit({editor_light_switch, 0, std::nullopt,
                 PrototypeLightSwitch{position, 0, 0, true}, selection_,
                 editor_light_switch});
}

bool EditorDocument::removeSelected() {
  static_cast<void>(finishTerrainStroke());
  if (selection_ == editor_light_switch && document_ && document_->light_switch)
    return commit({editor_light_switch, 0, *document_->light_switch,
                   std::nullopt, selection_, editor_no_object});
  const auto index = solidIndex(selection_);
  if (!index) return false;
  return commit({selection_, *index, document_->solids[*index], std::nullopt,
                 selection_, editor_no_object});
}

bool EditorDocument::placeSelected(WorldPosition terrain_hit) {
  auto value = object(selection_);
  if (!value || !finite(terrain_hit) ||
      !prototypeTerrainContains(document_->terrain, terrain_hit.x,
                                terrain_hit.z))
    return false;
  terrain_hit.y = prototypeTerrainHeightAt(document_->terrain, terrain_hit.x,
                                           terrain_hit.z);
  std::visit(
      [&](auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, PrototypeSolid>) {
          v.center = terrain_hit;
          v.center.y += v.half_extent.y;
        } else if constexpr (std::is_same_v<T, PrototypePlayerSpawn>) {
          v.foot_position = terrain_hit;
        } else if constexpr (std::is_same_v<T, PrototypePointLight> ||
                             std::is_same_v<T, PrototypeLightSwitch>) {
          const float offset = v.position.y - prototypeTerrainHeightAt(
                                                  document_->terrain,
                                                  v.position.x, v.position.z);
          v.position = terrain_hit;
          v.position.y += offset;
        } else {
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
  if (edit.brush) {
    for (const auto& sample : edit.terrain)
      document_->terrain.heights[sample.index] =
          forward ? sample.after : sample.before;
    terrain_brush_ = *edit.brush;
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
  } else if (edit.id == editor_spawn) {
    document_->player_spawn = std::get<PrototypePlayerSpawn>(*value);
  } else if (edit.id == editor_prop) {
    document_->static_prop = std::get<PrototypeStaticProp>(*value);
  } else if (edit.id == editor_light_switch) {
    document_->light_switch =
        value ? std::optional{std::get<PrototypeLightSwitch>(*value)}
              : std::nullopt;
  } else {
    document_->environment_light.point_lights[edit.id - editor_first_light] =
        std::get<PrototypePointLight>(*value);
  }
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
  if (!document_ || terrain_stroke_) return;
  terrain_stroke_.emplace();
  terrain_stroke_->brush = terrain_brush_;
  stroke_selection_ = selection_;
  extendTerrainStroke(hit);
}

void EditorDocument::extendTerrainStroke(std::optional<WorldPosition> hit) {
  if (terrain_stroke_ && terrain_stroke_->advance(document_->terrain, hit))
    ++revision_;
}

bool EditorDocument::finishTerrainStroke() {
  if (!terrain_stroke_) return false;
  Edit edit;
  edit.terrain = terrain_stroke_->changes(document_->terrain);
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
