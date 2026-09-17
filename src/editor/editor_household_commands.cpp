#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <type_traits>

#include "core/text/utf8.hpp"
#include "core/world/audio.hpp"
#include "core/world/household.hpp"
#include "editor/editor_document.hpp"

namespace {
bool safePose(const DoorLeafPose& pose) {
  if (!std::isfinite(pose.center.x) || !std::isfinite(pose.center.y) ||
      !std::isfinite(pose.center.z) || !std::isfinite(pose.yaw_degrees))
    return false;
  const double yaw =
      std::remainder(double(pose.yaw_degrees), 360.) * std::numbers::pi / 180.;
  const double c = std::abs(std::cos(yaw)), s = std::abs(std::sin(yaw));
  const std::array<float, 3> extents{
      float(c * pose.half_extent.x + s * pose.half_extent.z),
      pose.half_extent.y,
      float(s * pose.half_extent.x + c * pose.half_extent.z)};
  const std::array center{pose.center.x, pose.center.y, pose.center.z};
  for (std::size_t i = 0; i < center.size(); ++i) {
    const float lo = center[i] - extents[i], hi = center[i] + extents[i];
    if (!std::isfinite(lo) || !std::isfinite(hi) || !(lo < hi)) return false;
  }
  return true;
}

std::string recordId(const EditorObjectValue& value) {
  return std::visit(
      [](const auto& v) -> std::string {
        if constexpr (requires { v.id; }) return v.id;
        return {};
      },
      value);
}
}  // namespace

std::optional<EditorHouseholdKind> editorHouseholdKind(
    const EditorObjectValue& value) {
  if (std::holds_alternative<HouseholdBoxDefinition>(value))
    return EditorHouseholdKind::Box;
  if (std::holds_alternative<HouseholdDocumentDefinition>(value))
    return EditorHouseholdKind::Document;
  if (std::holds_alternative<HouseholdRadioDefinition>(value))
    return EditorHouseholdKind::Radio;
  return {};
}

std::string editorHouseholdFieldError(const EditorObjectValue& value) {
  if (!editorHouseholdKind(value)) return {};
  if (!levelEntryIdIsValid(recordId(value)))
    return "Household record ID must match [a-z][a-z0-9-]{0,63}.";
  if (const auto* box = std::get_if<HouseholdBoxDefinition>(&value)) {
    if (!safePose(householdBoxPose(*box)))
      return "Box center and yaw must produce finite, nondegenerate bounds.";
  } else if (const auto* document =
                 std::get_if<HouseholdDocumentDefinition>(&value)) {
    if (!safePose(householdDocumentPose(*document)))
      return "Document position and yaw must produce finite, nondegenerate "
             "bounds.";
    if (document->pages.size() > level_maximum_document_page_count)
      return "A document supports at most 16 ordered pages.";
    try {
      if (readableScalars(document->title).size() >
          level_maximum_document_title_scalars)
        return "Document title supports at most 80 Unicode scalar values.";
    } catch (const std::invalid_argument& error) {
      return std::string("Document title: ") + error.what();
    }
    for (std::size_t i = 0; i < document->pages.size(); ++i) {
      try {
        (void)readableScalars(document->pages[i], true);
      } catch (const std::invalid_argument& error) {
        return "Document page " + std::to_string(i + 1) + ": " + error.what();
      }
    }
    // Empty title/pages stay editable while shared validation gates Save/Play.
  } else if (const auto* radio =
                 std::get_if<HouseholdRadioDefinition>(&value)) {
    if (radio->prop.size() > 64 || radio->source.size() > 64)
      return "Radio prop/source references support at most 64 bytes.";
    // Unresolved references and ownership conflicts stay editable diagnostics.
  }
  return {};
}

void EditorDocument::resetHouseholdIds() {
  for (auto& ids : household_ids_) ids.clear();
  if (!document_) return;
  const auto& h = document_->household;
  const std::array sizes{h.boxes.size(), h.documents.size(), h.radios.size()};
  for (std::size_t kind = 0; kind < sizes.size(); ++kind)
    for (std::size_t i = 0; i < sizes[kind]; ++i)
      household_ids_[kind].push_back(next_object_id_++);
}

std::optional<EditorObjectValue> EditorDocument::householdObject(
    EditorObjectId id) const {
  if (!document_) return {};
  for (std::size_t kind = 0; kind < household_ids_.size(); ++kind) {
    const auto& ids = household_ids_[kind];
    const auto found = std::find(ids.begin(), ids.end(), id);
    if (found == ids.end()) continue;
    const auto index = static_cast<std::size_t>(found - ids.begin());
    switch (static_cast<EditorHouseholdKind>(kind)) {
      case EditorHouseholdKind::Box:
        return document_->household.boxes[index];
      case EditorHouseholdKind::Document:
        return document_->household.documents[index];
      case EditorHouseholdKind::Radio:
        return document_->household.radios[index];
    }
  }
  return {};
}

bool EditorDocument::addHousehold(EditorHouseholdKind kind) {
  if (!document_) return false;
  const auto* entry = findLevelEntry(*document_, document_->default_entry);
  WorldPosition position = entry ? entry->pose.foot_position : WorldPosition{};
  position.x += 1.5F;
  switch (kind) {
    case EditorHouseholdKind::Box:
      position.y += household_box_half_extent;
      return addHouseholdObject(HouseholdBoxDefinition{"", position, 0});
    case EditorHouseholdKind::Document:
      position.y += household_document_half_extent.y;
      return addHouseholdObject(HouseholdDocumentDefinition{
          "", position, 0, "Заметка", {"Текст заметки."}});
    case EditorHouseholdKind::Radio: {
      HouseholdRadioDefinition radio{"", "missing-prop", "missing-source",
                                     false};
      const auto selected = object(selection_);
      const auto available_prop = [&](const PrototypeStaticProp& prop) {
        return prop.model == "apartment-radio" &&
               prop.collision_boxes.empty() &&
               std::none_of(document_->household.radios.begin(),
                            document_->household.radios.end(),
                            [&](const auto& r) { return r.prop == prop.id; });
      };
      const auto available_source = [&](const AudioSourceDefinition& source) {
        const auto* cue = findAudioCue(document_->audio, source.cue);
        return !source.autoplay && cue && cue->spatial && cue->loop &&
               cue->kind == AudioCueKind::Ambience && cue->caption &&
               audioCatalogContains(*cue->caption) &&
               std::none_of(
                   document_->household.radios.begin(),
                   document_->household.radios.end(),
                   [&](const auto& r) { return r.source == source.id; }) &&
               std::none_of(document_->characters.actors.begin(),
                            document_->characters.actors.end(),
                            [&](const auto& a) {
                              return a.footstep_source == source.id ||
                                     a.interaction_source == source.id;
                            });
      };
      const auto prop = std::find_if(document_->props.begin(),
                                     document_->props.end(), available_prop);
      if (prop != document_->props.end()) radio.prop = prop->id;
      const auto source =
          std::find_if(document_->audio.sources.begin(),
                       document_->audio.sources.end(), available_source);
      if (source != document_->audio.sources.end()) radio.source = source->id;
      if (selected) {
        if (const auto* p = std::get_if<PrototypeStaticProp>(&*selected);
            p && available_prop(*p))
          radio.prop = p->id;
        if (const auto* s = std::get_if<AudioSourceDefinition>(&*selected);
            s && available_source(*s))
          radio.source = s->id;
      }
      return addHouseholdObject(std::move(radio));
    }
  }
  return false;
}

bool EditorDocument::addHouseholdObject(EditorObjectValue value) {
  static_cast<void>(finishTerrainStroke());
  const auto kind = editorHouseholdKind(value);
  if (!document_ || !kind) return false;
  const auto slot = static_cast<std::size_t>(*kind);
  const std::array limits{level_maximum_household_box_count,
                          level_maximum_household_document_count,
                          level_maximum_household_radio_count};
  if (household_ids_[slot].size() >= limits[slot]) {
    edit_error_ =
        "Household capacity reached (16 boxes, 32 documents, 8 radios); no "
        "object added.";
    return false;
  }
  const std::array prefixes{"box-", "document-", "radio-"};
  std::string name;
  for (std::size_t n = 1;; ++n) {
    name = prefixes[slot] + std::to_string(n);
    if (std::none_of(
            household_ids_[slot].begin(), household_ids_[slot].end(),
            [&](auto id) { return recordId(*householdObject(id)) == name; }))
      break;
  }
  std::visit(
      [&](auto& v) {
        if constexpr (requires { v.id; }) v.id = name;
      },
      value);
  edit_error_ = editorHouseholdFieldError(value);
  if (!edit_error_.empty()) return false;
  const auto id = next_object_id_++;
  return commit(
      {id, household_ids_[slot].size(), {}, std::move(value), selection_, id});
}

bool EditorDocument::prepareHouseholdEdit(Edit& edit) {
  if (const auto kind = editorHouseholdKind(*edit.after)) {
    const auto& ids = householdIds(*kind);
    const auto name = recordId(*edit.after);
    for (auto id : ids)
      if (id != edit.id && recordId(*householdObject(id)) == name) {
        edit_error_ =
            "Household record ID is already in use in this collection.";
        return false;
      }
  }
  const auto* old_prop = std::get_if<PrototypeStaticProp>(&*edit.before);
  const auto* old_source = std::get_if<AudioSourceDefinition>(&*edit.before);
  if ((!old_prop && !old_source) ||
      recordId(*edit.before) == recordId(*edit.after))
    return true;
  auto household = document_->household;
  if (const auto* old = old_prop) {
    const auto& next = std::get<PrototypeStaticProp>(*edit.after);
    for (auto& radio : household.radios)
      if (radio.prop == old->id) radio.prop = next.id;
  } else if (const auto* old = old_source) {
    const auto& next = std::get<AudioSourceDefinition>(*edit.after);
    for (auto& radio : household.radios)
      if (radio.source == old->id) radio.source = next.id;
  }
  if (household != document_->household) {
    edit.household_before = document_->household;
    edit.household_after = std::move(household);
  }
  return true;
}

bool EditorDocument::applyHouseholdEdit(const Edit& edit, bool forward) {
  const auto& identity = edit.after ? edit.after : edit.before;
  if (!identity) return false;
  const auto kind = editorHouseholdKind(*identity);
  if (!kind) return false;
  auto& ids = household_ids_[static_cast<std::size_t>(*kind)];
  const auto found = std::find(ids.begin(), ids.end(), edit.id);
  const auto index = static_cast<std::size_t>(found - ids.begin());
  const auto& value = forward ? edit.after : edit.before;
  const auto apply = [&](auto& values) {
    using T = typename std::decay_t<decltype(values)>::value_type;
    if (!value) {
      values.erase(values.begin() + index);
      ids.erase(ids.begin() + index);
    } else if (found != ids.end())
      values[index] = std::get<T>(*value);
    else {
      values.insert(values.begin() + edit.index, std::get<T>(*value));
      ids.insert(ids.begin() + edit.index, edit.id);
    }
  };
  switch (*kind) {
    case EditorHouseholdKind::Box:
      apply(document_->household.boxes);
      break;
    case EditorHouseholdKind::Document:
      apply(document_->household.documents);
      break;
    case EditorHouseholdKind::Radio:
      apply(document_->household.radios);
      break;
  }
  return true;
}
