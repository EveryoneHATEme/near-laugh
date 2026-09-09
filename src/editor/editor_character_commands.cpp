#include <algorithm>
#include <cmath>
#include <set>

#include "core/world/characters.hpp"
#include "editor/editor_document.hpp"

namespace {
template <class T>
std::string freshId(const std::vector<T>& values, std::string_view prefix,
                    const std::set<std::string_view>& referenced) {
  for (std::size_t n = 1;; ++n) {
    auto id = std::string(prefix) + std::to_string(n);
    if (!referenced.contains(id) &&
        std::none_of(values.begin(), values.end(),
                     [&](const auto& v) { return v.id == id; }))
      return id;
  }
}

// These three bounded collections share one existing document history entry.
void renameReferences(LevelCharacters& characters,
                      const EditorObjectValue& before,
                      const EditorObjectValue& after) {
  if (const auto* old = std::get_if<CharacterActorDefinition>(&before)) {
    const auto& next = std::get<CharacterActorDefinition>(after);
    for (auto& route : characters.routes)
      if (route.actor == old->id) route.actor = next.id;
  } else if (const auto* old = std::get_if<CharacterMarkDefinition>(&before)) {
    const auto& next = std::get<CharacterMarkDefinition>(after);
    for (auto& actor : characters.actors)
      if (actor.initial_mark == old->id) actor.initial_mark = next.id;
    for (auto& route : characters.routes)
      for (auto& mark : route.marks)
        if (mark == old->id) mark = next.id;
  } else if (const auto* old = std::get_if<CharacterRouteDefinition>(&before)) {
    const auto& next = std::get<CharacterRouteDefinition>(after);
    for (auto& actor : characters.actors)
      if (actor.initial_route == old->id) actor.initial_route = next.id;
  } else if (const auto* old = std::get_if<AudioSourceDefinition>(&before)) {
    const auto& next = std::get<AudioSourceDefinition>(after);
    for (auto& actor : characters.actors) {
      if (actor.footstep_source == old->id) actor.footstep_source = next.id;
      if (actor.interaction_source == old->id)
        actor.interaction_source = next.id;
    }
  }
}
}  // namespace

std::optional<EditorCharacterKind> editorCharacterKind(
    const EditorObjectValue& value) {
  if (std::holds_alternative<CharacterActorDefinition>(value))
    return EditorCharacterKind::Actor;
  if (std::holds_alternative<CharacterMarkDefinition>(value))
    return EditorCharacterKind::Mark;
  if (std::holds_alternative<CharacterRouteDefinition>(value))
    return EditorCharacterKind::Route;
  return {};
}

std::string editorCharacterFieldError(const EditorObjectValue& value) {
  return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, CharacterActorDefinition> ||
                      std::is_same_v<T, CharacterMarkDefinition> ||
                      std::is_same_v<T, CharacterRouteDefinition>) {
          if (!levelEntryIdIsValid(v.id))
            return "Character record ID must match [a-z][a-z0-9-]{0,63}.";
          if constexpr (std::is_same_v<T, CharacterActorDefinition>) {
            if (!std::isfinite(v.speed)) return "Actor speed must be finite.";
          } else if constexpr (std::is_same_v<T, CharacterMarkDefinition>) {
            if (!std::isfinite(v.feet_position.x) ||
                !std::isfinite(v.feet_position.y) ||
                !std::isfinite(v.feet_position.z) ||
                !std::isfinite(v.yaw_degrees))
              return "Mark feet position and yaw must be finite.";
          } else if (v.marks.size() > level_maximum_route_mark_count)
            return "A route supports at most 32 ordered marks.";
        }
        // Unknown references and finite gameplay-invalid values remain
        // repairable.
        return {};
      },
      value);
}

void EditorDocument::resetCharacterIds() {
  for (auto& ids : character_ids_) ids.clear();
  if (!document_) return;
  const auto& c = document_->characters;
  const std::array sizes{c.actors.size(), c.marks.size(), c.routes.size()};
  for (std::size_t kind = 0; kind < sizes.size(); ++kind)
    for (std::size_t i = 0; i < sizes[kind]; ++i)
      character_ids_[kind].push_back(next_object_id_++);
}

std::optional<EditorObjectValue> EditorDocument::characterObject(
    EditorObjectId id) const {
  if (!document_) return {};
  for (std::size_t kind = 0; kind < character_ids_.size(); ++kind) {
    const auto& ids = character_ids_[kind];
    const auto found = std::find(ids.begin(), ids.end(), id);
    if (found == ids.end()) continue;
    const auto index = static_cast<std::size_t>(found - ids.begin());
    switch (static_cast<EditorCharacterKind>(kind)) {
      case EditorCharacterKind::Actor:
        return document_->characters.actors[index];
      case EditorCharacterKind::Mark:
        return document_->characters.marks[index];
      case EditorCharacterKind::Route:
        return document_->characters.routes[index];
    }
  }
  return {};
}

bool EditorDocument::prepareCharacterEdit(Edit& edit) {
  auto characters = document_->characters;
  const auto replace = [&](auto& values, const auto& ids) {
    using T = typename std::decay_t<decltype(values)>::value_type;
    const auto& next = std::get<T>(*edit.after);
    const auto index = static_cast<std::size_t>(
        std::find(ids.begin(), ids.end(), edit.id) - ids.begin());
    for (std::size_t i = 0; i < values.size(); ++i)
      if (i != index && values[i].id == next.id) {
        edit_error_ =
            "Character record ID is already in use in this collection.";
        return false;
      }
    values[index] = next;
    return true;
  };
  if (const auto kind = editorCharacterKind(*edit.after)) {
    const auto& ids = characterIds(*kind);
    switch (*kind) {
      case EditorCharacterKind::Actor:
        if (!replace(characters.actors, ids)) return false;
        break;
      case EditorCharacterKind::Mark:
        if (!replace(characters.marks, ids)) return false;
        break;
      case EditorCharacterKind::Route:
        if (!replace(characters.routes, ids)) return false;
        break;
    }
  }
  renameReferences(characters, *edit.before, *edit.after);
  if (characters != document_->characters) {
    edit.characters_before = document_->characters;
    edit.characters_after = std::move(characters);
  }
  return true;
}

bool EditorDocument::addCharacter(EditorCharacterKind kind) {
  if (!document_) return false;
  const auto pose = document_->entries.front().pose;
  const auto selected = characterObject(selection_);
  switch (kind) {
    case EditorCharacterKind::Actor: {
      CharacterActorDefinition actor;
      actor.model = test_mannequin_catalog.id;
      if (selected &&
          std::holds_alternative<CharacterMarkDefinition>(*selected)) {
        actor.initial_mark = std::get<CharacterMarkDefinition>(*selected).id;
        return addCharacterObject(actor);
      }
      return addCharacterObject(
          actor,
          CharacterMarkDefinition{"", pose.foot_position, pose.yaw_degrees});
    }
    case EditorCharacterKind::Mark:
      return addCharacterObject(
          CharacterMarkDefinition{"", pose.foot_position, pose.yaw_degrees});
    case EditorCharacterKind::Route: {
      CharacterRouteDefinition route;
      const auto* actor =
          selected ? std::get_if<CharacterActorDefinition>(&*selected)
                   : nullptr;
      if (!actor && !document_->characters.actors.empty())
        actor = &document_->characters.actors.front();
      route.actor = actor ? actor->id : "missing-actor";
      if (actor) route.marks.push_back(actor->initial_mark);
      return addCharacterObject(route);
    }
  }
  return false;
}

bool EditorDocument::addCharacterObject(
    EditorObjectValue value, std::optional<CharacterMarkDefinition> mark) {
  const auto kind = editorCharacterKind(value);
  if (!document_ || !kind) return false;
  const auto slot = static_cast<std::size_t>(*kind);
  const std::array limits{level_maximum_actor_count,
                          level_maximum_character_mark_count,
                          level_maximum_character_route_count};
  // Preflight both collections before finishing a gesture or allocating
  // handles.
  if (character_ids_[slot].size() >= limits[slot] ||
      (mark && document_->characters.marks.size() >=
                   level_maximum_character_mark_count)) {
    edit_error_ =
        "Character capacity reached (4 actors, 32 marks, 16 routes); no "
        "records added.";
    return false;
  }
  auto characters = document_->characters;
  auto ids = character_ids_;
  const auto handle = next_object_id_;
  // A new record must not silently reconnect an existing broken reference.
  std::array<std::set<std::string_view>, 3> referenced;
  for (const auto& actor : document_->characters.actors) {
    referenced[1].insert(actor.initial_mark);
    if (actor.initial_route) referenced[2].insert(*actor.initial_route);
  }
  for (const auto& route : document_->characters.routes) {
    referenced[0].insert(route.actor);
    for (const auto& id : route.marks) referenced[1].insert(id);
  }
  if (mark) {
    mark->id = freshId(characters.marks, "mark-", referenced[1]);
    edit_error_ = editorCharacterFieldError(*mark);
    if (!edit_error_.empty()) return false;
    std::get<CharacterActorDefinition>(value).initial_mark = mark->id;
    characters.marks.push_back(*mark);
    ids[static_cast<std::size_t>(EditorCharacterKind::Mark)].push_back(handle +
                                                                       1);
  }
  const auto add = [&](auto& values) {
    using T = typename std::decay_t<decltype(values)>::value_type;
    auto& v = std::get<T>(value);
    const std::array prefixes{"actor-", "mark-", "route-"};
    v.id = freshId(values, prefixes[slot], referenced[slot]);
    values.push_back(v);
  };
  switch (*kind) {
    case EditorCharacterKind::Actor:
      add(characters.actors);
      break;
    case EditorCharacterKind::Mark:
      add(characters.marks);
      break;
    case EditorCharacterKind::Route:
      add(characters.routes);
      break;
  }
  edit_error_ = editorCharacterFieldError(value);
  if (!edit_error_.empty()) return false;
  static_cast<void>(finishTerrainStroke());
  ids[slot].push_back(handle);
  Edit edit{handle,     character_ids_[slot].size(),
            {},         std::move(value),
            selection_, handle};
  edit.characters_before = document_->characters;
  edit.characters_after = std::move(characters);
  edit.character_ids_before = character_ids_;
  edit.character_ids_after = std::move(ids);
  next_object_id_ += mark ? 2 : 1;
  return commit(std::move(edit));
}

bool EditorDocument::duplicateCharacter(EditorObjectValue value) {
  if (auto* actor = std::get_if<CharacterActorDefinition>(&value)) {
    const auto* initial =
        findCharacterMark(document_->characters, actor->initial_mark);
    if (!initial) {
      edit_error_ =
          "Repair the actor's missing initial mark before duplicating its "
          "placement.";
      return false;
    }
    auto mark = *initial;
    mark.feet_position.x += 1.5F;
    actor->initial_route.reset();
    actor->footstep_source.reset();
    actor->interaction_source.reset();
    return addCharacterObject(std::move(value), std::move(mark));
  }
  if (auto* mark = std::get_if<CharacterMarkDefinition>(&value))
    mark->feet_position.x += 1.5F;
  return addCharacterObject(std::move(value));
}

bool EditorDocument::removeCharacter() {
  const auto value = characterObject(selection_);
  if (!value) return false;
  auto characters = document_->characters;
  auto ids = character_ids_;
  const auto kind = *editorCharacterKind(*value);
  auto& handles = ids[static_cast<std::size_t>(kind)];
  const auto found = std::find(handles.begin(), handles.end(), selection_);
  const auto index = found - handles.begin();
  handles.erase(found);
  switch (kind) {
    case EditorCharacterKind::Actor:
      characters.actors.erase(characters.actors.begin() + index);
      break;
    case EditorCharacterKind::Mark:
      characters.marks.erase(characters.marks.begin() + index);
      break;
    case EditorCharacterKind::Route:
      characters.routes.erase(characters.routes.begin() + index);
      break;
  }
  Edit edit{selection_, static_cast<std::size_t>(index),
            value,      {},
            selection_, editor_no_object};
  edit.characters_before = document_->characters;
  edit.characters_after = std::move(characters);
  edit.character_ids_before = character_ids_;
  edit.character_ids_after = std::move(ids);
  return commit(std::move(edit));
}
