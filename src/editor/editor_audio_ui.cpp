#include <imgui.h>
#include "editor/editor_widget_metadata.hpp"

#include <algorithm>
#include <cstring>

#include "core/world/audio.hpp"
#include "editor/editor_ui.hpp"

EditorAuditionAction EditorUi::drawAudition(const EditorAuditionView& view,
                                            bool can_start) {
  auto action = EditorAuditionAction::None;
  EditorWidgets::SetNextWindowPos({340, 470}, ImGuiCond_FirstUseEver);
  EditorWidgets::SetNextWindowSize({380, 230}, ImGuiCond_FirstUseEver);
  EditorWidgets::Begin("Audio audition");
  EditorWidgets::BeginDisabled(!can_start ||
                       playtest_.state() != EditorPlayState::Idle);
  if (EditorWidgets::Button("Start audition")) action = EditorAuditionAction::Start;
  EditorWidgets::EndDisabled();
  EditorWidgets::SameLine();
  EditorWidgets::BeginDisabled(!view.active);
  if (EditorWidgets::Button("Stop")) action = EditorAuditionAction::Stop;
  bool muted = view.muted, paused = view.paused;
  EditorWidgetMetadata::next("mute", "audition.muted", "", "preview");
  if (EditorWidgets::Checkbox("Mute", &muted)) action = EditorAuditionAction::Mute;
  EditorWidgets::SameLine();
  EditorWidgetMetadata::next("pause", "audition.paused", "", "preview");
  if (EditorWidgets::Checkbox("Pause", &paused)) action = EditorAuditionAction::Pause;
  EditorWidgets::EndDisabled();
  if (view.active) {
    EditorWidgets::Text("Source: %.*s  Gain: %.3f", int(view.source.size()),
                view.source.data(), view.gain);
    EditorWidgets::Text("Source room: %.*s", int(view.source_room.size()),
                view.source_room.data());
    EditorWidgets::Text("Listener room: %.*s", int(view.listener_room.size()),
                view.listener_room.data());
  }
  for (const auto caption : {view.captions.foreground, view.captions.ambience})
    if (!caption.text.empty())
      EditorWidgets::TextWrapped("%.*s: %.*s", int(caption.label.size()),
                         caption.label.data(), int(caption.text.size()),
                         caption.text.data());
  if (!view.warning.empty())
    EditorWidgets::TextWrapped("%.*s", int(view.warning.size()), view.warning.data());
  EditorWidgets::End();
  return action;
}

void EditorUi::drawAudioObjects(EditorDocument& document) {
  EditorWidgets::SetNextWindowPos({340, 205}, ImGuiCond_FirstUseEver);
  EditorWidgets::SetNextWindowSize({380, 255}, ImGuiCond_FirstUseEver);
  EditorWidgets::Begin("Audio authoring");
  if (document.document()) {
    const std::array buttons{"Add cue", "Add source", "Add room",
                             "Add connection"};
    const std::array<std::size_t, 4> limits{128, 64, 32, 64};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
      const auto kind = static_cast<EditorAudioKind>(i);
      EditorWidgets::BeginDisabled(document.audioIds(kind).size() >= limits[i]);
      if (i % 2) EditorWidgets::SameLine();
      if (EditorWidgets::Button(buttons[i])) static_cast<void>(document.addAudio(kind));
      EditorWidgets::EndDisabled();
    }
    const auto selected = document.object(document.selection());
    EditorWidgets::BeginDisabled(!selected || !editorAudioKind(*selected));
    if (EditorWidgets::Button("Duplicate audio"))
      static_cast<void>(document.duplicateSelected());
    EditorWidgets::SameLine();
    if (EditorWidgets::Button("Delete audio"))
      static_cast<void>(document.removeSelected());
    EditorWidgets::EndDisabled();
    EditorWidgets::Separator();
    const std::array labels{"Cue: ", "Source: ", "Room: ", "Connection: "};
    for (std::size_t kind = 0; kind < labels.size(); ++kind)
      for (const auto id :
           document.audioIds(static_cast<EditorAudioKind>(kind))) {
        const auto value = *document.object(id);
        std::visit(
            [&](const auto& v) {
              if constexpr (requires { v.id; }) {
                EditorWidgetMetadata::Owner owner(id);
                EditorWidgetMetadata::next("select");
                if (EditorWidgets::Selectable((labels[kind] + v.id).c_str(),
                                      document.selection() == id))
                  selectObject(document, id);
              }
            },
            value);
      }
  } else
    EditorWidgets::TextUnformatted("Open a level to author audio.");
  EditorWidgets::End();
}

bool drawEditorAudioProperties(EditorObjectValue& object,
                               const LevelDocument& level) {
  bool commit = false;
  const auto text = [&](const char* label, std::string& value) {
    std::array<char, 65> buffer{};
    std::memcpy(buffer.data(), value.data(),
                std::min(value.size(), buffer.size() - 1));
    if (EditorWidgets::InputText(label, buffer.data(), buffer.size()))
      value = buffer.data();
    commit |= editorTextEditFinished();
  };
  const auto scalar = [&](const char* label, float& value) {
    EditorWidgets::DragFloat(label, &value, .02F);
    commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
  };
  const auto triple = [&](const char* label, auto& value) {
    float xyz[]{value.x, value.y, value.z};
    if (EditorWidgets::DragFloat3(label, xyz, .05F)) value = {xyz[0], xyz[1], xyz[2]};
    commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
  };
  const auto reference = [&](const char* label,
                             std::optional<std::string>& value,
                             const auto& choices, const char* empty) {
    if (EditorWidgets::BeginCombo(label, value ? value->c_str() : empty)) {
      if (EditorWidgets::Selectable(empty, !value)) {
        value.reset();
        commit = true;
      }
      for (const auto& choice : choices)
        if (EditorWidgets::Selectable(choice.id.c_str(), value == choice.id)) {
          value = choice.id;
          commit = true;
        }
      EditorWidgets::EndCombo();
    }
  };
  std::visit(
      [&](auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, AudioCueDefinition> ||
                      std::is_same_v<T, AudioSourceDefinition> ||
                      std::is_same_v<T, AudioRoomDefinition> ||
                      std::is_same_v<T, AudioConnectionDefinition>) {
          EditorWidgetMetadata::next("audio-id", "id", "", "object");
          text("Audio ID", v.id);
          if constexpr (std::is_same_v<T, AudioCueDefinition>) {
            EditorWidgetMetadata::next("clip", "clip", "", "object");
            if (EditorWidgets::BeginCombo("Clip", v.clip.c_str())) {
              for (const auto& entry : audioCatalog())
                if (EditorWidgets::Selectable(entry.label.data(), v.clip == entry.id)) {
                  v.clip = entry.id;
                  commit = true;
                }
              EditorWidgets::EndCombo();
            }
            EditorWidgetMetadata::next("caption", "caption", "", "object");
            if (EditorWidgets::BeginCombo("Caption",
                                  v.caption ? v.caption->c_str() : "None")) {
              if (EditorWidgets::Selectable("None", !v.caption)) {
                v.caption.reset();
                commit = true;
              }
              for (const auto& entry : audioCatalog())
                if (EditorWidgets::Selectable(entry.label.data(),
                                      v.caption == entry.id)) {
                  v.caption = entry.id;
                  commit = true;
                }
              EditorWidgets::EndCombo();
            }
            int kind = static_cast<int>(v.kind);
            EditorWidgetMetadata::next("cue-kind", "kind", "", "object");
            if (EditorWidgets::Combo("Cue kind", &kind,
                             "Dialogue\0Essential\0Ambience\0")) {
              v.kind = static_cast<AudioCueKind>(kind);
              commit = true;
            }
            EditorWidgetMetadata::next("loop", "loop", "", "object");
            commit |= EditorWidgets::Checkbox("Loop", &v.loop);
            EditorWidgetMetadata::next("spatial", "spatial", "", "object");
            commit |= EditorWidgets::Checkbox("Spatial", &v.spatial);
          } else if constexpr (std::is_same_v<T, AudioSourceDefinition>) {
            EditorWidgetMetadata::next("cue", "cue", "", "object");
            if (EditorWidgets::BeginCombo("Cue", v.cue.c_str())) {
              for (const auto& cue : level.audio.cues)
                if (EditorWidgets::Selectable(cue.id.c_str(), v.cue == cue.id)) {
                  v.cue = cue.id;
                  commit = true;
                }
              EditorWidgets::EndCombo();
            }
            EditorWidgetMetadata::next("position", "position", "m", "object");
            triple("Position", v.position);
            EditorWidgetMetadata::next("gain", "gain", "", "object");
            scalar("Gain", v.gain);
            EditorWidgetMetadata::next("near-distance-m", "near_distance", "m", "object");
            scalar("Near distance (m)", v.near_distance);
            EditorWidgetMetadata::next("far-distance-m", "far_distance", "m", "object");
            scalar("Far distance (m)", v.far_distance);
            EditorWidgetMetadata::next("autoplay", "autoplay", "", "object");
            commit |= EditorWidgets::Checkbox("Autoplay", &v.autoplay);
          } else if constexpr (std::is_same_v<T, AudioRoomDefinition>) {
            EditorWidgetMetadata::next("center", "center", "m", "object");
            triple("Center", v.center);
            EditorWidgetMetadata::next("half-extent", "half_extent", "m", "object");
            triple("Half extent", v.half_extent);
          } else {
            EditorWidgetMetadata::next("room-a", "room_a", "", "object");
            reference("Room A", v.room_a, level.audio.rooms, "Outside");
            EditorWidgetMetadata::next("room-b", "room_b", "", "object");
            reference("Room B", v.room_b, level.audio.rooms, "Outside");
            EditorWidgetMetadata::next("door", "door", "", "object");
            reference("Door", v.door, level.doors, "No door");
            EditorWidgetMetadata::next("closed-gain", "closed_gain", "", "object");
            scalar("Closed gain", v.closed_gain);
            EditorWidgetMetadata::next("open-gain", "open_gain", "", "object");
            scalar("Open gain", v.open_gain);
          }
        }
      },
      object);
  return commit;
}
