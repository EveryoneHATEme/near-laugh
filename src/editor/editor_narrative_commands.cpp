#include <algorithm>
#include <cmath>
#include <type_traits>

#include "editor/editor_document.hpp"

namespace {
std::string recordId(const EditorObjectValue& value) {
  return std::visit(
      [](const auto& v) -> std::string {
        if constexpr (requires { v.id; }) return v.id;
        return {};
      },
      value);
}
// Visit concrete typed links only; equal IDs in unrelated collections stay
// intact.
template <class T, class V, class F>
void visitLinks(V& v, const F& replace) {
  if constexpr (std::is_same_v<T, NarrativeFactDefinition>) {
    if constexpr (requires { v.fact; }) replace(v.fact);
  } else if constexpr (std::is_same_v<T, NarrativeRegionDefinition>) {
    if constexpr (requires { v.region; }) replace(v.region);
  } else if constexpr (std::is_same_v<T, NarrativeEventDefinition>) {
    if constexpr (requires { v.event; }) replace(v.event);
  } else if constexpr (std::is_same_v<T, PrototypePointLight>) {
    if constexpr (requires { v.light; }) replace(v.light);
  } else if constexpr (std::is_same_v<T, DoorDefinition>) {
    if constexpr (requires { v.door; }) replace(v.door);
  } else if constexpr (std::is_same_v<T, CharacterActorDefinition>) {
    if constexpr (requires { v.actor; }) replace(v.actor);
  } else if constexpr (std::is_same_v<T, CharacterRouteDefinition>) {
    if constexpr (requires { v.route; }) replace(v.route);
  } else if constexpr (std::is_same_v<T, AudioSourceDefinition>) {
    if constexpr (requires { v.source; }) replace(v.source);
  } else if constexpr (std::is_same_v<T, HouseholdBoxDefinition>) {
    if constexpr (requires { v.box; }) replace(v.box);
  } else if constexpr (std::is_same_v<T, HouseholdDocumentDefinition>) {
    if constexpr (requires { v.document; }) replace(v.document);
  } else if constexpr (std::is_same_v<T, HouseholdRadioDefinition>) {
    if constexpr (requires { v.radio; }) replace(v.radio);
  }
  if constexpr (std::is_same_v<std::remove_cv_t<V>,
                               NarrativeInteractionTrigger>) {
    const bool matches =
        (std::is_same_v<T, DoorDefinition> &&
         v.target_kind == NarrativeInteractionTarget::Door) ||
        (std::is_same_v<T, PrototypeLightSwitch> &&
         v.target_kind == NarrativeInteractionTarget::Switch) ||
        (std::is_same_v<T, HouseholdRadioDefinition> &&
         v.target_kind == NarrativeInteractionTarget::Radio) ||
        (std::is_same_v<T, HouseholdDocumentDefinition> &&
         v.target_kind == NarrativeInteractionTarget::Document) ||
        (std::is_same_v<T, HouseholdBoxDefinition> &&
         v.target_kind == NarrativeInteractionTarget::Box);
    if (matches) replace(v.target);
  }
  if constexpr (requires { v.predicates; })
    for (auto& p : v.predicates)
      std::visit([&](auto& term) { visitLinks<T>(term, replace); }, p);
}
template <class T, class N, class F>
void visitEventLinks(N& narrative, const F& replace) {
  for (auto& event : narrative.events) {
    std::visit([&](auto& v) { visitLinks<T>(v, replace); }, event.trigger);
    for (auto& p : event.guards)
      std::visit([&](auto& v) { visitLinks<T>(v, replace); }, p);
    if (event.cancel)
      for (auto& p : *event.cancel)
        std::visit([&](auto& v) { visitLinks<T>(v, replace); }, p);
    for (auto& step : event.steps)
      std::visit([&](auto& v) { visitLinks<T>(v, replace); }, step);
  }
}
template <class T>
void renameEvents(LevelNarrative& narrative, const std::string& old,
                  const std::string& next) {
  visitEventLinks<T>(narrative, [&](std::string& id) {
    if (id == old) id = next;
  });
}
template <class V>
std::string parameterError(const V& v) {
  if constexpr (requires { v.seconds; })
    if (!std::isfinite(v.seconds) || v.seconds < 0 ||
        v.seconds > level_maximum_narrative_seconds)
      return "Duration must be finite and between 0 and 3600 seconds.";
  if constexpr (requires { v.predicates; }) {
    if (v.predicates.size() > level_maximum_narrative_predicate_count)
      return "At most eight predicates are supported per condition list.";
    for (const auto& p : v.predicates) {
      auto error =
          std::visit([](const auto& term) { return parameterError(term); }, p);
      if (!error.empty()) return error;
    }
  }
  return {};
}
}  // namespace

bool editorNarrativeReferences(const LevelNarrative& narrative,
                               const EditorObjectValue& type,
                               std::string_view id) {
  bool referenced = false;
  std::visit(
      [&](const auto& value) {
        visitEventLinks<std::decay_t<decltype(value)>>(
            narrative,
            [&](const std::string& link) { referenced |= link == id; });
      },
      type);
  return referenced;
}

std::optional<EditorNarrativeKind> editorNarrativeKind(
    const EditorObjectValue& value) {
  if (std::holds_alternative<NarrativeFactDefinition>(value))
    return EditorNarrativeKind::Fact;
  if (std::holds_alternative<NarrativeRegionDefinition>(value))
    return EditorNarrativeKind::Region;
  if (std::holds_alternative<NarrativeEventDefinition>(value))
    return EditorNarrativeKind::Event;
  return {};
}
std::string editorNarrativeFieldError(const EditorObjectValue& value) {
  if (!editorNarrativeKind(value)) return {};
  if (!levelEntryIdIsValid(recordId(value)))
    return "Narrative ID must match [a-z][a-z0-9-]{0,63}.";
  if (const auto* region = std::get_if<NarrativeRegionDefinition>(&value)) {
    const std::array centers{region->center.x, region->center.y,
                             region->center.z};
    const std::array extents{region->half_extent.x, region->half_extent.y,
                             region->half_extent.z};
    for (std::size_t i = 0; i < centers.size(); ++i) {
      const auto lo = centers[i] - extents[i], hi = centers[i] + extents[i];
      if (!std::isfinite(centers[i]) || !std::isfinite(extents[i]) ||
          extents[i] <= 0 || !std::isfinite(lo) || !std::isfinite(hi) ||
          !(lo < hi))
        return "Region center/extents must produce finite, positive, "
               "nondegenerate bounds.";
    }
  }
  if (const auto* event = std::get_if<NarrativeEventDefinition>(&value)) {
    if (event->steps.size() > level_maximum_narrative_step_count)
      return "An event supports at most 32 ordered steps.";
    if (event->guards.size() > level_maximum_narrative_predicate_count ||
        (event->cancel &&
         event->cancel->size() > level_maximum_narrative_predicate_count))
      return "At most eight predicates are supported per condition list.";
    auto error = std::visit([](const auto& v) { return parameterError(v); },
                            event->trigger);
    if (!error.empty()) return "Trigger: " + error;
    for (const auto& p : event->guards) {
      error = std::visit([](const auto& v) { return parameterError(v); }, p);
      if (!error.empty()) return "Guard: " + error;
    }
    if (event->cancel)
      for (const auto& p : *event->cancel) {
        error = std::visit([](const auto& v) { return parameterError(v); }, p);
        if (!error.empty()) return "Cancellation: " + error;
      }
    for (std::size_t i = 0; i < event->steps.size(); ++i) {
      error = std::visit([](const auto& v) { return parameterError(v); },
                         event->steps[i]);
      if (!error.empty()) return "Step " + std::to_string(i + 1) + ": " + error;
    }
  }
  // Dangling links and unsupported combinations remain repairable in the draft
  // document. Shared validation blocks Save/Play with exact event/step paths.
  return {};
}
void EditorDocument::resetNarrativeIds() {
  for (auto& ids : narrative_ids_) ids.clear();
  if (!document_) return;
  const auto& n = document_->narrative;
  const std::array sizes{n.facts.size(), n.regions.size(), n.events.size()};
  for (std::size_t k = 0; k < sizes.size(); ++k)
    for (std::size_t i = 0; i < sizes[k]; ++i)
      narrative_ids_[k].push_back(next_object_id_++);
}
std::optional<EditorObjectValue> EditorDocument::narrativeObject(
    EditorObjectId id) const {
  if (!document_) return {};
  for (std::size_t k = 0; k < narrative_ids_.size(); ++k) {
    const auto& ids = narrative_ids_[k];
    const auto it = std::find(ids.begin(), ids.end(), id);
    if (it == ids.end()) continue;
    const auto i = static_cast<std::size_t>(it - ids.begin());
    switch (static_cast<EditorNarrativeKind>(k)) {
      case EditorNarrativeKind::Fact:
        return document_->narrative.facts[i];
      case EditorNarrativeKind::Region:
        return document_->narrative.regions[i];
      case EditorNarrativeKind::Event:
        return document_->narrative.events[i];
    }
  }
  return {};
}
bool EditorDocument::addNarrative(EditorNarrativeKind kind) {
  if (!document_) return false;
  switch (kind) {
    case EditorNarrativeKind::Fact:
      return addNarrativeObject(NarrativeFactDefinition{});
    case EditorNarrativeKind::Region: {
      const auto* entry = findLevelEntry(*document_, document_->default_entry);
      auto p = entry ? entry->pose.foot_position : WorldPosition{};
      p.x += 2;
      p.y += 1;
      return addNarrativeObject(NarrativeRegionDefinition{"", p, {1, 1, 1}});
    }
    case EditorNarrativeKind::Event: {
      NarrativeEventDefinition event;
      event.steps.emplace_back(NarrativeDelayStep{});
      return addNarrativeObject(event);
    }
  }
  return false;
}
bool EditorDocument::addNarrativeObject(EditorObjectValue value) {
  static_cast<void>(finishTerrainStroke());
  const auto kind = editorNarrativeKind(value);
  if (!document_ || !kind) return false;
  const auto slot = static_cast<std::size_t>(*kind);
  const std::array limits{level_maximum_narrative_fact_count,
                          level_maximum_narrative_region_count,
                          level_maximum_narrative_event_count};
  if (narrative_ids_[slot].size() >= limits[slot]) {
    edit_error_ =
        "Narrative capacity reached (32 facts, 32 regions, 64 events); no "
        "record added.";
    return false;
  }
  const std::array prefixes{"fact-", "region-", "event-"};
  const auto name = editorFreshId(
      prefixes[slot], document_->narrative, value, [&](const std::string& id) {
        return std::any_of(narrative_ids_[slot].begin(),
                           narrative_ids_[slot].end(), [&](auto other) {
                             return recordId(*narrativeObject(other)) == id;
                           });
      });
  std::visit(
      [&](auto& v) {
        if constexpr (requires { v.id; }) v.id = name;
      },
      value);
  edit_error_ = editorNarrativeFieldError(value);
  if (!edit_error_.empty()) return false;
  const auto id = next_object_id_++;
  return commit(
      {id, narrative_ids_[slot].size(), {}, std::move(value), selection_, id});
}
bool EditorDocument::prepareNarrativeEdit(Edit& edit) {
  const auto old = recordId(*edit.before), next = recordId(*edit.after);
  if (const auto kind = editorNarrativeKind(*edit.after))
    for (auto id : narrativeIds(*kind))
      if (id != edit.id && recordId(*narrativeObject(id)) == next) {
        edit_error_ = "Narrative ID is already in use in this collection.";
        return false;
      }
  if (old == next) return true;
  auto narrative = document_->narrative;
  std::visit(
      [&](const auto& value) {
        renameEvents<std::decay_t<decltype(value)>>(narrative, old, next);
      },
      *edit.before);
  // The edited event's own outgoing links must receive the same typed rename.
  if (auto* event = std::get_if<NarrativeEventDefinition>(&*edit.after)) {
    LevelNarrative single;
    single.events.push_back(*event);
    renameEvents<NarrativeEventDefinition>(single, old, next);
    *event = std::move(single.events.front());
  }
  if (narrative != document_->narrative) {
    edit.narrative_before = document_->narrative;
    edit.narrative_after = std::move(narrative);
  }
  return true;
}
bool EditorDocument::applyNarrativeEdit(const Edit& edit, bool forward) {
  const auto& identity = edit.after ? edit.after : edit.before;
  if (!identity) return false;
  const auto kind = editorNarrativeKind(*identity);
  if (!kind) return false;
  auto& ids = narrative_ids_[static_cast<std::size_t>(*kind)];
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
    case EditorNarrativeKind::Fact:
      apply(document_->narrative.facts);
      break;
    case EditorNarrativeKind::Region:
      apply(document_->narrative.regions);
      break;
    case EditorNarrativeKind::Event:
      apply(document_->narrative.events);
      break;
  }
  return true;
}
