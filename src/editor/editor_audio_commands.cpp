#include <algorithm>
#include <cmath>

#include "core/world/audio.hpp"
#include "editor/editor_document.hpp"

std::optional<EditorAudioKind> editorAudioKind(const EditorObjectValue& value) {
  if (std::holds_alternative<AudioCueDefinition>(value))
    return EditorAudioKind::Cue;
  if (std::holds_alternative<AudioSourceDefinition>(value))
    return EditorAudioKind::Source;
  if (std::holds_alternative<AudioRoomDefinition>(value))
    return EditorAudioKind::Room;
  if (std::holds_alternative<AudioConnectionDefinition>(value))
    return EditorAudioKind::Connection;
  return {};
}
std::string editorAudioFieldError(const EditorObjectValue& value) {
  return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, AudioCueDefinition> ||
                      std::is_same_v<T, AudioSourceDefinition> ||
                      std::is_same_v<T, AudioRoomDefinition> ||
                      std::is_same_v<T, AudioConnectionDefinition>) {
          if (!levelEntryIdIsValid(v.id))
            return "Audio ID must match [a-z][a-z0-9-]{0,63}.";
          if constexpr (std::is_same_v<T, AudioSourceDefinition>) {
            if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) ||
                !std::isfinite(v.position.z))
              return "Source position must be finite.";
            if (!std::isfinite(v.gain) || v.gain < 0 || v.gain > 1)
              return "Gain must be in [0,1].";
            if (!std::isfinite(v.near_distance) ||
                !std::isfinite(v.far_distance) || v.near_distance <= 0 ||
                v.near_distance >= v.far_distance || v.far_distance > 100)
              return "Distances must satisfy 0 < near < far <= 100 metres.";
          } else if constexpr (std::is_same_v<T, AudioRoomDefinition>) {
            if (!audioRoomBoundsAreSafe(v))
              return "Room extents must be positive with finite representable "
                     "bounds.";
          } else if constexpr (std::is_same_v<T, AudioConnectionDefinition>) {
            if (!std::isfinite(v.closed_gain) || !std::isfinite(v.open_gain) ||
                v.closed_gain < 0 || v.closed_gain > v.open_gain ||
                v.open_gain > 1)
              return "Gains must satisfy 0 <= closed <= open <= 1.";
          } else if (v.kind != AudioCueKind::Ambience &&
                     v.kind != AudioCueKind::Dialogue &&
                     v.kind != AudioCueKind::Essential)
            return "Unknown cue kind.";
        }
        return {};  // Broken catalog and object references remain editable
                    // diagnostics.
      },
      value);
}

void EditorDocument::resetAudioIds() {
  for (auto& ids : audio_ids_) ids.clear();
  if (!document_) return;
  const auto& a = document_->audio;
  const std::array sizes{a.cues.size(), a.sources.size(), a.rooms.size(),
                         a.connections.size()};
  for (std::size_t kind = 0; kind < sizes.size(); ++kind)
    for (std::size_t i = 0; i < sizes[kind]; ++i)
      audio_ids_[kind].push_back(next_object_id_++);
}
std::optional<EditorObjectValue> EditorDocument::audioObject(
    EditorObjectId id) const {
  if (!document_) return {};
  for (std::size_t kind = 0; kind < audio_ids_.size(); ++kind) {
    const auto& ids = audio_ids_[kind];
    const auto found = std::find(ids.begin(), ids.end(), id);
    if (found == ids.end()) continue;
    const auto index = static_cast<std::size_t>(found - ids.begin());
    switch (static_cast<EditorAudioKind>(kind)) {
      case EditorAudioKind::Cue:
        return document_->audio.cues[index];
      case EditorAudioKind::Source:
        return document_->audio.sources[index];
      case EditorAudioKind::Room:
        return document_->audio.rooms[index];
      case EditorAudioKind::Connection:
        return document_->audio.connections[index];
    }
  }
  return {};
}
bool EditorDocument::addAudio(EditorAudioKind kind) {
  if (!document_) return false;
  WorldPosition position = document_->entries.front().pose.foot_position;
  position.y += 1.5F;
  switch (kind) {
    case EditorAudioKind::Cue:
      return addAudioObject(AudioCueDefinition{
          "", "radio", "radio", AudioCueKind::Ambience, true, true});
    case EditorAudioKind::Source:
      return addAudioObject(AudioSourceDefinition{
          "",
          document_->audio.cues.empty() ? "missing-cue"
                                        : document_->audio.cues.front().id,
          position, 1, 1, 20, false});
    case EditorAudioKind::Room:
      return addAudioObject(AudioRoomDefinition{"", position, {1, 1.5F, 1}});
    case EditorAudioKind::Connection:
      return addAudioObject(AudioConnectionDefinition{
          "",
          document_->audio.rooms.empty() ? "missing-room"
                                         : document_->audio.rooms.front().id,
          {},
          {},
          .2F,
          1});
  }
  return false;
}
bool EditorDocument::addAudioObject(EditorObjectValue value) {
  static_cast<void>(finishTerrainStroke());
  const auto kind = editorAudioKind(value);
  if (!document_ || !kind) return false;
  const auto slot = static_cast<std::size_t>(*kind);
  const std::array<std::size_t, 4> limits{128, 64, 32, 64};
  if (audio_ids_[slot].size() >= limits[slot]) {
    edit_error_ = "Audio collection limit reached.";
    return false;
  }
  const std::array prefixes{"cue-", "source-", "room-", "connection-"};
  std::visit(
      [&](auto& v) {
        if constexpr (requires { v.id; }) {
          for (std::size_t n = 1;; ++n) {
            v.id = prefixes[slot] + std::to_string(n);
            bool used = false;
            for (const auto id : audio_ids_[slot]) {
              const auto other = *audioObject(id);
              if (const auto* same =
                      std::get_if<std::decay_t<decltype(v)>>(&other))
                used |= same->id == v.id;
            }
            if (*kind == EditorAudioKind::Source)
              for (const auto& actor : document_->characters.actors)
                used |= actor.footstep_source == v.id ||
                        actor.interaction_source == v.id;
            if (!used) break;
          }
        }
      },
      value);
  edit_error_ = editorAudioFieldError(value);
  if (!edit_error_.empty()) return false;
  const auto id = next_object_id_++;
  return commit(
      {id, audio_ids_[slot].size(), {}, std::move(value), selection_, id});
}
bool EditorDocument::applyAudioEdit(const Edit& edit, bool forward) {
  const auto& identity = edit.after ? edit.after : edit.before;
  if (!identity) return false;
  const auto kind = editorAudioKind(*identity);
  if (!kind) return false;
  auto& ids = audio_ids_[static_cast<std::size_t>(*kind)];
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
    case EditorAudioKind::Cue:
      apply(document_->audio.cues);
      break;
    case EditorAudioKind::Source:
      apply(document_->audio.sources);
      break;
    case EditorAudioKind::Room:
      apply(document_->audio.rooms);
      break;
    case EditorAudioKind::Connection:
      apply(document_->audio.connections);
      break;
  }
  return true;
}
void EditorDocument::renameAudioReferences(LevelAudio& audio,
                                           const EditorObjectValue& before,
                                           const EditorObjectValue& after) {
  if (const auto* old = std::get_if<AudioCueDefinition>(&before)) {
    const auto& next = std::get<AudioCueDefinition>(after);
    for (auto& source : audio.sources)
      if (source.cue == old->id) source.cue = next.id;
  } else if (const auto* old = std::get_if<AudioRoomDefinition>(&before)) {
    const auto& next = std::get<AudioRoomDefinition>(after);
    for (auto& connection : audio.connections) {
      if (connection.room_a == old->id) connection.room_a = next.id;
      if (connection.room_b == old->id) connection.room_b = next.id;
    }
  } else if (const auto* old = std::get_if<DoorDefinition>(&before)) {
    const auto& next = std::get<DoorDefinition>(after);
    for (auto& connection : audio.connections)
      if (connection.door == old->id) connection.door = next.id;
  }
}
