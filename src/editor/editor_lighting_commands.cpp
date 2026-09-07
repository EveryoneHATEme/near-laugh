#include <algorithm>
#include <cmath>

#include "editor/editor_document.hpp"

std::optional<std::size_t> EditorDocument::lightIndex(EditorObjectId id) const {
  const auto it = std::find(light_ids_.begin(), light_ids_.end(), id);
  return it == light_ids_.end()
             ? std::nullopt
             : std::optional<std::size_t>(it - light_ids_.begin());
}
std::optional<std::size_t> EditorDocument::switchIndex(
    EditorObjectId id) const {
  const auto it = std::find(switch_ids_.begin(), switch_ids_.end(), id);
  return it == switch_ids_.end()
             ? std::nullopt
             : std::optional<std::size_t>(it - switch_ids_.begin());
}

bool EditorDocument::addPointLight() {
  if (!document_) return false;
  auto position = document_->entries.empty()
                      ? WorldPosition{}
                      : document_->entries.front().pose.foot_position;
  position.y += 2;
  return addPointLight({position, {1, 1, 1}, .8F, 5});
}
bool EditorDocument::addPointLight(PrototypePointLight value) {
  (void)finishTerrainStroke();
  if (!document_ || document_->environment_light.point_lights.size() >=
                        level_maximum_point_light_count) {
    edit_error_ = "A level supports at most eight point lights.";
    return false;
  }
  auto& lights = document_->environment_light.point_lights;
  for (std::size_t i = 1;; ++i) {
    value.id = "point-light-" + std::to_string(i);
    if (std::none_of(lights.begin(), lights.end(),
                     [&](const auto& v) { return v.id == value.id; }))
      break;
  }
  edit_error_ = editorObjectFieldError(value);
  if (!edit_error_.empty()) return false;
  const auto id = next_object_id_++;
  return commit(
      {id, lights.size(), std::nullopt, std::move(value), selection_, id});
}

bool EditorDocument::addLightSwitch() {
  if (!document_) return false;
  auto position = document_->entries.empty()
                      ? WorldPosition{}
                      : document_->entries.front().pose.foot_position;
  position.y += 1.65F;
  const auto& lights = document_->environment_light.point_lights;
  return addLightSwitch({position, 0, lights.empty() ? "" : lights.front().id});
}
bool EditorDocument::addLightSwitch(PrototypeLightSwitch value) {
  (void)finishTerrainStroke();
  if (!document_ ||
      document_->light_switches.size() >= level_maximum_light_switch_count) {
    edit_error_ = "A level supports at most sixteen switches.";
    return false;
  }
  for (std::size_t i = 1;; ++i) {
    value.id = "light-switch-" + std::to_string(i);
    if (std::none_of(document_->light_switches.begin(),
                     document_->light_switches.end(),
                     [&](const auto& v) { return v.id == value.id; }))
      break;
  }
  edit_error_ = editorObjectFieldError(value);
  if (!edit_error_.empty()) return false;
  const auto id = next_object_id_++;
  return commit({id, document_->light_switches.size(), std::nullopt,
                 std::move(value), selection_, id});
}

bool EditorDocument::setAmbient(float value) {
  (void)finishTerrainStroke();
  if (!document_) return false;
  if (!std::isfinite(value) || value < 0 ||
      value > prototype_maximum_ambient_intensity) {
    edit_error_ = "Ambient must be finite and between 0 and 0.20.";
    return false;
  }
  if (value == document_->environment_light.ambient_intensity) return false;
  Edit edit;
  edit.selection_before = edit.selection_after = selection_;
  edit.ambient_before = document_->environment_light.ambient_intensity;
  edit.ambient_after = value;
  return commit(std::move(edit));
}

bool EditorDocument::prepareLightingEdit(Edit& edit) {
  if (const auto* light = std::get_if<PrototypePointLight>(&*edit.after)) {
    const auto& old = std::get<PrototypePointLight>(*edit.before);
    if (old.id == light->id) return true;
    const auto& lights = document_->environment_light.point_lights;
    if (std::any_of(lights.begin(), lights.end(),
                    [&](const auto& v) { return v.id == light->id; })) {
      edit_error_ = "Light ID is already in use.";
      return false;
    }
    auto switches = document_->light_switches;
    for (auto& value : switches)
      if (value.light_id == old.id) value.light_id = light->id;
    if (switches != document_->light_switches) {
      edit.switches_before = document_->light_switches;
      edit.switches_after = std::move(switches);
    }
  } else if (const auto* value =
                 std::get_if<PrototypeLightSwitch>(&*edit.after)) {
    const auto& old = std::get<PrototypeLightSwitch>(*edit.before);
    if (old.id != value->id &&
        std::any_of(document_->light_switches.begin(),
                    document_->light_switches.end(),
                    [&](const auto& v) { return v.id == value->id; })) {
      edit_error_ = "Switch ID is already in use.";
      return false;
    }
  }
  return true;
}

bool EditorDocument::applyLightingEdit(const Edit& edit, bool forward) {
  if (edit.switches_after)
    document_->light_switches =
        forward ? *edit.switches_after : *edit.switches_before;
  if (edit.ambient_after) {
    document_->environment_light.ambient_intensity =
        forward ? *edit.ambient_after : *edit.ambient_before;
    return true;
  }
  const auto& value = forward ? edit.after : edit.before;
  const auto apply = [&]<class T>(std::vector<T>& values,
                                  std::vector<EditorObjectId>& ids,
                                  std::optional<std::size_t> index) {
    if (!value) {
      values.erase(values.begin() + static_cast<std::ptrdiff_t>(*index));
      ids.erase(ids.begin() + static_cast<std::ptrdiff_t>(*index));
    } else if (index)
      values[*index] = std::get<T>(*value);
    else {
      values.insert(values.begin() + static_cast<std::ptrdiff_t>(edit.index),
                    std::get<T>(*value));
      ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(edit.index),
                 edit.id);
    }
  };
  if ((value && std::holds_alternative<PrototypePointLight>(*value)) ||
      lightIndex(edit.id)) {
    apply(document_->environment_light.point_lights, light_ids_,
          lightIndex(edit.id));
    return true;
  }
  if ((value && std::holds_alternative<PrototypeLightSwitch>(*value)) ||
      switchIndex(edit.id)) {
    apply(document_->light_switches, switch_ids_, switchIndex(edit.id));
    return true;
  }
  return false;
}
