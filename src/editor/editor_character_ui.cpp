#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <type_traits>

#include "core/world/characters.hpp"
#include "editor/editor_ui.hpp"

namespace {
void showMarkConsumers(const LevelCharacters& characters, std::string_view id) {
  ImGui::TextUnformatted("Mark consumers (all share this placement):");
  bool used = false;
  for (const auto& actor : characters.actors)
    if (actor.initial_mark == id) {
      ImGui::TextWrapped("Actor %s: initial mark", actor.id.c_str());
      used = true;
    }
  for (const auto& route : characters.routes)
    for (std::size_t i = 0; i < route.marks.size(); ++i)
      if (route.marks[i] == id) {
        ImGui::TextWrapped("Route %s: mark %zu", route.id.c_str(), i + 1);
        used = true;
      }
  if (!used) ImGui::TextUnformatted("No incoming references.");
}
}  // namespace

void EditorUi::drawCharacterObjects(EditorDocument& document) {
  if (!ImGui::CollapsingHeader("Characters", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  const std::array buttons{"Add actor", "Add mark", "Add route"};
  const std::array labels{"Actor: ", "Mark: ", "Route: "};
  const std::array limits{level_maximum_actor_count,
                          level_maximum_character_mark_count,
                          level_maximum_character_route_count};
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    if (i) ImGui::SameLine();
    const auto kind = static_cast<EditorCharacterKind>(i);
    // Compound actor/mark capacity is checked by the document command so its
    // failure remains visible and cannot create only part of an actor.
    ImGui::BeginDisabled(document.characterIds(kind).size() >= limits[i]);
    if (ImGui::Button(buttons[i])) {
      if (document.addCharacter(kind)) placing_ = sculpting_ = false;
    }
    ImGui::EndDisabled();
  }
  for (std::size_t kind = 0; kind < labels.size(); ++kind) {
    for (const auto id :
         document.characterIds(static_cast<EditorCharacterKind>(kind))) {
      const auto value = *document.object(id);
      std::visit(
          [&](const auto& record) {
            if constexpr (requires { record.id; }) {
              // Handles keep list identity stable through durable-ID edits.
              ImGui::PushID(static_cast<int>(id));
              if (ImGui::Selectable((labels[kind] + record.id).c_str(),
                                    document.selection() == id))
                selectObject(document, id);
              ImGui::PopID();
            }
          },
          value);
    }
  }
  ImGui::Separator();
}

void EditorUi::drawCharacterProperties(EditorDocument& document) {
  const auto& level = *document.document();
  const auto& characters = level.characters;
  bool commit = false;
  bool select_initial_mark = false;
  const auto text = [&](const char* label, std::string& value) {
    std::array<char, 65> buffer{};
    std::memcpy(buffer.data(), value.data(),
                std::min(value.size(), buffer.size() - 1));
    if (ImGui::InputText(label, buffer.data(), buffer.size()))
      value = buffer.data();
    commit |= ImGui::IsItemDeactivatedAfterEdit();
  };
  const auto required_reference = [&](const char* label, std::string& value,
                                      const auto& choices) {
    if (ImGui::BeginCombo(label, value.empty() ? "<missing>" : value.c_str())) {
      for (const auto& choice : choices)
        if (ImGui::Selectable(choice.id.c_str(), value == choice.id)) {
          value = choice.id;
          commit = true;
        }
      ImGui::EndCombo();
    }
    if (std::none_of(choices.begin(), choices.end(),
                     [&](const auto& choice) { return choice.id == value; }))
      ImGui::TextWrapped("Unresolved %s: %s", label,
                         value.empty() ? "<empty>" : value.c_str());
  };
  const auto optional_reference = [&](const char* label,
                                      std::optional<std::string>& value,
                                      const auto& choices) {
    if (ImGui::BeginCombo(label, value ? value->c_str() : "None")) {
      if (ImGui::Selectable("None", !value)) {
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
    if (value &&
        std::none_of(choices.begin(), choices.end(),
                     [&](const auto& choice) { return choice.id == *value; }))
      ImGui::TextWrapped("Unresolved %s: %s", label, value->c_str());
  };
  std::visit(
      [&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, CharacterActorDefinition>) {
          text("Actor ID", value.id);
          if (ImGui::BeginCombo("Character model", value.model.c_str())) {
            if (ImGui::Selectable(test_mannequin_catalog.id.data(),
                                  value.model == test_mannequin_catalog.id)) {
              value.model = test_mannequin_catalog.id;
              commit = true;
            }
            ImGui::EndCombo();
          }
          if (!findCharacterModel(value.model))
            ImGui::TextWrapped("Unresolved character model: %s",
                               value.model.c_str());
          ImGui::DragFloat("Speed (m/s)", &value.speed, .01F, 0, 0, "%.3f");
          commit |= ImGui::IsItemDeactivatedAfterEdit();
          required_reference("Initial mark", value.initial_mark,
                             characters.marks);
          optional_reference("Initial route", value.initial_route,
                             characters.routes);
          optional_reference("Footstep source", value.footstep_source,
                             level.audio.sources);
          optional_reference("Interaction source", value.interaction_source,
                             level.audio.sources);
          ImGui::Separator();
          ImGui::TextUnformatted("Actor placement is its initial mark.");
          if (const auto* mark =
                  findCharacterMark(characters, value.initial_mark)) {
            ImGui::Text("Feet: (%.3f, %.3f, %.3f), yaw %.3f",
                        mark->feet_position.x, mark->feet_position.y,
                        mark->feet_position.z, mark->yaw_degrees);
            showMarkConsumers(characters, mark->id);
            select_initial_mark = ImGui::Button("Select/edit initial mark");
          }
        } else if constexpr (std::is_same_v<T, CharacterMarkDefinition>) {
          // Use the committed identity while an ID draft is being typed, so
          // its existing consumers stay visible before the rename commits.
          const auto original = document.object(document.selection());
          showMarkConsumers(characters,
                            std::get<CharacterMarkDefinition>(*original).id);
          ImGui::Separator();
          text("Mark ID", value.id);
          float xyz[]{value.feet_position.x, value.feet_position.y,
                      value.feet_position.z};
          if (ImGui::DragFloat3("Feet position", xyz, .05F, 0, 0, "%.3f"))
            value.feet_position = {xyz[0], xyz[1], xyz[2]};
          commit |= ImGui::IsItemDeactivatedAfterEdit();
          ImGui::DragFloat("Facing yaw (degrees)", &value.yaw_degrees, .5F, 0,
                           0, "%.3f");
          commit |= ImGui::IsItemDeactivatedAfterEdit();
        } else if constexpr (std::is_same_v<T, CharacterRouteDefinition>) {
          text("Route ID", value.id);
          required_reference("Owner actor", value.actor, characters.actors);
          if (ImGui::BeginCombo("Final clip", value.final_clip
                                                  ? value.final_clip->c_str()
                                                  : "None")) {
            if (ImGui::Selectable("None", !value.final_clip)) {
              value.final_clip.reset();
              commit = true;
            }
            if (ImGui::Selectable("interact", value.final_clip == "interact")) {
              value.final_clip = "interact";
              commit = true;
            }
            ImGui::EndCombo();
          }
          if (value.final_clip && value.final_clip != "interact")
            ImGui::TextWrapped("Unsupported final clip: %s",
                               value.final_clip->c_str());
          ImGui::Text("Ordered marks: %zu / %zu", value.marks.size(),
                      level_maximum_route_mark_count);
          ImGui::BeginDisabled(value.marks.size() >=
                                   level_maximum_route_mark_count ||
                               characters.marks.empty());
          if (ImGui::BeginCombo("Add route mark", "Choose mark")) {
            for (const auto& mark : characters.marks)
              if (ImGui::Selectable(mark.id.c_str())) {
                value.marks.push_back(mark.id);
                commit = true;
              }
            ImGui::EndCombo();
          }
          ImGui::EndDisabled();
          for (std::size_t i = 0; i < value.marks.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            required_reference(("Mark " + std::to_string(i + 1)).c_str(),
                               value.marks[i], characters.marks);
            ImGui::BeginDisabled(i == 0);
            const bool up = ImGui::Button("Up");
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(i + 1 == value.marks.size());
            const bool down = ImGui::Button("Down");
            ImGui::EndDisabled();
            ImGui::SameLine();
            const bool remove = ImGui::Button("Remove mark");
            ImGui::PopID();
            if (up) std::swap(value.marks[i], value.marks[i - 1]);
            if (down) std::swap(value.marks[i], value.marks[i + 1]);
            if (remove)
              value.marks.erase(value.marks.begin() +
                                static_cast<std::ptrdiff_t>(i));
            if (up || down || remove) {
              commit = true;
              break;
            }
          }
        }
      },
      *property_edit_.value());
  if (commit || select_initial_mark) {
    const bool changed =
        property_edit_.value() != document.object(document.selection());
    if (changed && !property_edit_.commit(document)) return;
  }
  if (select_initial_mark) {
    const auto actor = std::get<CharacterActorDefinition>(
        *document.object(document.selection()));
    for (const auto id : document.characterIds(EditorCharacterKind::Mark)) {
      if (std::get<CharacterMarkDefinition>(*document.object(id)).id ==
          actor.initial_mark) {
        document.select(id);
        property_edit_.synchronize(document);
        break;
      }
    }
  }
}
