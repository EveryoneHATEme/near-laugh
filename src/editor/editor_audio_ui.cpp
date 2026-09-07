#include <imgui.h>

#include <algorithm>
#include <cstring>

#include "core/world/audio.hpp"
#include "editor/editor_ui.hpp"

EditorAuditionAction EditorUi::drawAudition(const EditorAuditionView& view,
                                            bool can_start) {
  auto action = EditorAuditionAction::None;
  ImGui::SetNextWindowPos({340, 470}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({380, 230}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Audio audition");
  ImGui::BeginDisabled(!can_start ||
                       playtest_.state() != EditorPlayState::Idle);
  if (ImGui::Button("Start audition")) action = EditorAuditionAction::Start;
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!view.active);
  if (ImGui::Button("Stop")) action = EditorAuditionAction::Stop;
  bool muted = view.muted, paused = view.paused;
  if (ImGui::Checkbox("Mute", &muted)) action = EditorAuditionAction::Mute;
  ImGui::SameLine();
  if (ImGui::Checkbox("Pause", &paused)) action = EditorAuditionAction::Pause;
  ImGui::EndDisabled();
  if (view.active) {
    ImGui::Text("Source: %.*s  Gain: %.3f", int(view.source.size()),
                view.source.data(), view.gain);
    ImGui::Text("Source room: %.*s", int(view.source_room.size()),
                view.source_room.data());
    ImGui::Text("Listener room: %.*s", int(view.listener_room.size()),
                view.listener_room.data());
  }
  for (const auto caption : {view.captions.foreground, view.captions.ambience})
    if (!caption.text.empty())
      ImGui::TextWrapped("%.*s: %.*s", int(caption.label.size()),
                         caption.label.data(), int(caption.text.size()),
                         caption.text.data());
  if (!view.warning.empty())
    ImGui::TextWrapped("%.*s", int(view.warning.size()), view.warning.data());
  ImGui::End();
  return action;
}

void EditorUi::drawAudioObjects(EditorDocument& document) {
  ImGui::SetNextWindowPos({340, 205}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({380, 255}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Audio authoring");
  if (document.document()) {
    const std::array buttons{"Add cue", "Add source", "Add room",
                             "Add connection"};
    const std::array<std::size_t, 4> limits{128, 64, 32, 64};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
      const auto kind = static_cast<EditorAudioKind>(i);
      ImGui::BeginDisabled(document.audioIds(kind).size() >= limits[i]);
      if (i % 2) ImGui::SameLine();
      if (ImGui::Button(buttons[i])) static_cast<void>(document.addAudio(kind));
      ImGui::EndDisabled();
    }
    const auto selected = document.object(document.selection());
    ImGui::BeginDisabled(!selected || !editorAudioKind(*selected));
    if (ImGui::Button("Duplicate audio"))
      static_cast<void>(document.duplicateSelected());
    ImGui::SameLine();
    if (ImGui::Button("Delete audio"))
      static_cast<void>(document.removeSelected());
    ImGui::EndDisabled();
    ImGui::Separator();
    const std::array labels{"Cue: ", "Source: ", "Room: ", "Connection: "};
    for (std::size_t kind = 0; kind < labels.size(); ++kind)
      for (const auto id :
           document.audioIds(static_cast<EditorAudioKind>(kind))) {
        const auto value = *document.object(id);
        std::visit(
            [&](const auto& v) {
              if constexpr (requires { v.id; })
                if (ImGui::Selectable((labels[kind] + v.id).c_str(),
                                      document.selection() == id))
                  document.select(id);
            },
            value);
      }
  } else
    ImGui::TextUnformatted("Open a level to author audio.");
  ImGui::End();
}

bool drawEditorAudioProperties(EditorObjectValue& object,
                               const LevelDocument& level) {
  bool commit = false;
  const auto text = [&](const char* label, std::string& value) {
    std::array<char, 65> buffer{};
    std::memcpy(buffer.data(), value.data(),
                std::min(value.size(), buffer.size() - 1));
    if (ImGui::InputText(label, buffer.data(), buffer.size()))
      value = buffer.data();
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  const auto scalar = [&](const char* label, float& value) {
    ImGui::DragFloat(label, &value, .02F);
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  const auto triple = [&](const char* label, auto& value) {
    float xyz[]{value.x, value.y, value.z};
    if (ImGui::DragFloat3(label, xyz, .05F)) value = {xyz[0], xyz[1], xyz[2]};
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  const auto reference = [&](const char* label,
                             std::optional<std::string>& value,
                             const auto& choices, const char* empty) {
    if (ImGui::BeginCombo(label, value ? value->c_str() : empty)) {
      if (ImGui::Selectable(empty, !value)) {
        value.reset();
        commit = true;
      }
      for (const auto& choice : choices)
        if (ImGui::Selectable(choice.id.c_str(), value == choice.id)) {
          value = choice.id;
          commit = true;
        }
      ImGui::EndCombo();
    }
  };
  std::visit(
      [&](auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, AudioCueDefinition> ||
                      std::is_same_v<T, AudioSourceDefinition> ||
                      std::is_same_v<T, AudioRoomDefinition> ||
                      std::is_same_v<T, AudioConnectionDefinition>) {
          text("Audio ID", v.id);
          if constexpr (std::is_same_v<T, AudioCueDefinition>) {
            if (ImGui::BeginCombo("Clip", v.clip.c_str())) {
              for (const auto& entry : audioCatalog())
                if (ImGui::Selectable(entry.label.data(), v.clip == entry.id)) {
                  v.clip = entry.id;
                  commit = true;
                }
              ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("Caption",
                                  v.caption ? v.caption->c_str() : "None")) {
              if (ImGui::Selectable("None", !v.caption)) {
                v.caption.reset();
                commit = true;
              }
              for (const auto& entry : audioCatalog())
                if (ImGui::Selectable(entry.label.data(),
                                      v.caption == entry.id)) {
                  v.caption = entry.id;
                  commit = true;
                }
              ImGui::EndCombo();
            }
            int kind = static_cast<int>(v.kind);
            if (ImGui::Combo("Cue kind", &kind,
                             "Dialogue\0Essential\0Ambience\0")) {
              v.kind = static_cast<AudioCueKind>(kind);
              commit = true;
            }
            commit |= ImGui::Checkbox("Loop", &v.loop);
            commit |= ImGui::Checkbox("Spatial", &v.spatial);
          } else if constexpr (std::is_same_v<T, AudioSourceDefinition>) {
            if (ImGui::BeginCombo("Cue", v.cue.c_str())) {
              for (const auto& cue : level.audio.cues)
                if (ImGui::Selectable(cue.id.c_str(), v.cue == cue.id)) {
                  v.cue = cue.id;
                  commit = true;
                }
              ImGui::EndCombo();
            }
            triple("Position", v.position);
            scalar("Gain", v.gain);
            scalar("Near distance (m)", v.near_distance);
            scalar("Far distance (m)", v.far_distance);
            commit |= ImGui::Checkbox("Autoplay", &v.autoplay);
          } else if constexpr (std::is_same_v<T, AudioRoomDefinition>) {
            triple("Center", v.center);
            triple("Half extent", v.half_extent);
          } else {
            reference("Room A", v.room_a, level.audio.rooms, "Outside");
            reference("Room B", v.room_b, level.audio.rooms, "Outside");
            reference("Door", v.door, level.doors, "No door");
            scalar("Closed gain", v.closed_gain);
            scalar("Open gain", v.open_gain);
          }
        }
      },
      object);
  return commit;
}
