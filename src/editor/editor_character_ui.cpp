#include <imgui.h>
#include "editor/editor_widget_metadata.hpp"

#include <algorithm>
#include <cstring>
#include <type_traits>

#include "core/world/characters.hpp"
#include "editor/editor_ui.hpp"

namespace {
void showMarkConsumers(const LevelCharacters& characters, std::string_view id) {
  EditorWidgets::TextUnformatted("Mark consumers (all share this placement):");
  bool used = false;
  for (const auto& actor : characters.actors)
    if (actor.initial_mark == id) {
      EditorWidgets::TextWrapped("Actor %s: initial mark", actor.id.c_str());
      used = true;
    }
  for (const auto& route : characters.routes)
    for (std::size_t i = 0; i < route.marks.size(); ++i)
      if (route.marks[i] == id) {
        EditorWidgets::TextWrapped("Route %s: mark %zu", route.id.c_str(), i + 1);
        used = true;
      }
  if (!used) EditorWidgets::TextUnformatted("No incoming references.");
}
}  // namespace

void EditorUi::drawCharacterObjects(EditorDocument& document) {
  if (!EditorWidgets::CollapsingHeader("Characters", ImGuiTreeNodeFlags_DefaultOpen))
    return;
  const std::array buttons{"Add actor", "Add mark", "Add route"};
  const std::array labels{"Actor: ", "Mark: ", "Route: "};
  const std::array limits{level_maximum_actor_count,
                          level_maximum_character_mark_count,
                          level_maximum_character_route_count};
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    if (i) EditorWidgets::SameLine();
    const auto kind = static_cast<EditorCharacterKind>(i);
    // Compound actor/mark capacity is checked by the document command so its
    // failure remains visible and cannot create only part of an actor.
    EditorWidgets::BeginDisabled(document.characterIds(kind).size() >= limits[i]);
    if (EditorWidgets::Button(buttons[i])) {
      if (document.addCharacter(kind)) placing_ = sculpting_ = false;
    }
    EditorWidgets::EndDisabled();
  }
  for (std::size_t kind = 0; kind < labels.size(); ++kind) {
    for (const auto id :
         document.characterIds(static_cast<EditorCharacterKind>(kind))) {
      const auto value = *document.object(id);
      std::visit(
          [&](const auto& record) {
            if constexpr (requires { record.id; }) {
              // Handles keep list identity stable through durable-ID edits.
              EditorWidgetMetadata::Owner owner(id);
              EditorWidgetMetadata::next("select");
              EditorWidgets::PushID(static_cast<int>(id));
              if (EditorWidgets::Selectable((labels[kind] + record.id).c_str(),
                                    document.selection() == id))
                selectObject(document, id);
              EditorWidgets::PopID();
            }
          },
          value);
    }
  }
  EditorWidgets::Separator();
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
    if (EditorWidgets::InputText(label, buffer.data(), buffer.size()))
      value = buffer.data();
    commit |= editorTextEditFinished();
  };
  const auto required_reference = [&](const char* label, std::string& value,
                                      const auto& choices) {
    if (EditorWidgets::BeginCombo(label, value.empty() ? "<missing>" : value.c_str())) {
      for (const auto& choice : choices)
        if (EditorWidgets::Selectable(choice.id.c_str(), value == choice.id)) {
          value = choice.id;
          commit = true;
        }
      EditorWidgets::EndCombo();
    }
    if (std::none_of(choices.begin(), choices.end(),
                     [&](const auto& choice) { return choice.id == value; }))
      EditorWidgets::TextWrapped("Unresolved %s: %s", label,
                         value.empty() ? "<empty>" : value.c_str());
  };
  const auto optional_reference = [&](const char* label,
                                      std::optional<std::string>& value,
                                      const auto& choices) {
    if (EditorWidgets::BeginCombo(label, value ? value->c_str() : "None")) {
      if (EditorWidgets::Selectable("None", !value)) {
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
    if (value &&
        std::none_of(choices.begin(), choices.end(),
                     [&](const auto& choice) { return choice.id == *value; }))
      EditorWidgets::TextWrapped("Unresolved %s: %s", label, value->c_str());
  };
  std::visit(
      [&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, CharacterActorDefinition>) {
          EditorWidgetMetadata::next("actor-id", "id", "", "object");
          text("Actor ID", value.id);
          EditorWidgetMetadata::next("character-model", "model", "", "object");
          if (EditorWidgets::BeginCombo("Character model", value.model.c_str())) {
            if (EditorWidgets::Selectable(test_mannequin_catalog.id.data(),
                                  value.model == test_mannequin_catalog.id)) {
              value.model = test_mannequin_catalog.id;
              commit = true;
            }
            EditorWidgets::EndCombo();
          }
          if (!findCharacterModel(value.model))
            EditorWidgets::TextWrapped("Unresolved character model: %s",
                               value.model.c_str());
          EditorWidgetMetadata::next("speed-m-s", "speed", "m/s", "object");
          EditorWidgets::DragFloat("Speed (m/s)", &value.speed, .01F, 0, 0, "%.3f");
          commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
          EditorWidgetMetadata::next("initial-mark", "initial_mark", "", "object");
          required_reference("Initial mark", value.initial_mark,
                             characters.marks);
          EditorWidgetMetadata::next("initial-route", "initial_route", "", "object");
          optional_reference("Initial route", value.initial_route,
                             characters.routes);
          EditorWidgetMetadata::next("footstep-source", "footstep_source", "", "object");
          optional_reference("Footstep source", value.footstep_source,
                             level.audio.sources);
          EditorWidgetMetadata::next("interaction-source", "interaction_source", "", "object");
          optional_reference("Interaction source", value.interaction_source,
                             level.audio.sources);
          EditorWidgets::Separator();
          EditorWidgets::TextUnformatted("Actor placement is its initial mark.");
          if (const auto* mark =
                  findCharacterMark(characters, value.initial_mark)) {
            EditorWidgets::Text("Feet: (%.3f, %.3f, %.3f), yaw %.3f",
                        mark->feet_position.x, mark->feet_position.y,
                        mark->feet_position.z, mark->yaw_degrees);
            showMarkConsumers(characters, mark->id);
            select_initial_mark = EditorWidgets::Button("Select/edit initial mark");
          }
        } else if constexpr (std::is_same_v<T, CharacterMarkDefinition>) {
          // Use the committed identity while an ID draft is being typed, so
          // its existing consumers stay visible before the rename commits.
          const auto original = document.object(document.selection());
          showMarkConsumers(characters,
                            std::get<CharacterMarkDefinition>(*original).id);
          EditorWidgets::Separator();
          EditorWidgetMetadata::next("mark-id", "id", "", "object");
          text("Mark ID", value.id);
          float xyz[]{value.feet_position.x, value.feet_position.y,
                      value.feet_position.z};
          EditorWidgetMetadata::next("feet-position", "feet_position", "m", "object");
          if (EditorWidgets::DragFloat3("Feet position", xyz, .05F, 0, 0, "%.3f"))
            value.feet_position = {xyz[0], xyz[1], xyz[2]};
          commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
          EditorWidgetMetadata::next("facing-yaw-degrees", "yaw_degrees", "degrees", "object");
          EditorWidgets::DragFloat("Facing yaw (degrees)", &value.yaw_degrees, .5F, 0,
                           0, "%.3f");
          commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
        } else if constexpr (std::is_same_v<T, CharacterRouteDefinition>) {
          EditorWidgetMetadata::next("route-id", "id", "", "object");
          text("Route ID", value.id);
          EditorWidgetMetadata::next("owner-actor", "actor", "", "object");
          required_reference("Owner actor", value.actor, characters.actors);
          EditorWidgetMetadata::next("final-clip", "final_clip", "", "object");
          if (EditorWidgets::BeginCombo("Final clip", value.final_clip
                                                  ? value.final_clip->c_str()
                                                  : "None")) {
            if (EditorWidgets::Selectable("None", !value.final_clip)) {
              value.final_clip.reset();
              commit = true;
            }
            if (EditorWidgets::Selectable("interact", value.final_clip == "interact")) {
              value.final_clip = "interact";
              commit = true;
            }
            EditorWidgets::EndCombo();
          }
          if (value.final_clip && value.final_clip != "interact")
            EditorWidgets::TextWrapped("Unsupported final clip: %s",
                               value.final_clip->c_str());
          EditorWidgets::Text("Ordered marks: %zu / %zu", value.marks.size(),
                      level_maximum_route_mark_count);
          EditorWidgets::BeginDisabled(value.marks.size() >=
                                   level_maximum_route_mark_count ||
                               characters.marks.empty());
          if (EditorWidgets::BeginCombo("Add route mark", "Choose mark")) {
            for (const auto& mark : characters.marks)
              if (EditorWidgets::Selectable(mark.id.c_str())) {
                EditorWidgetMetadata::structureChanged("marks");
                value.marks.push_back(mark.id);
                commit = true;
              }
            EditorWidgets::EndCombo();
          }
          EditorWidgets::EndDisabled();
          for (std::size_t i = 0; i < value.marks.size(); ++i) {
            EditorWidgetMetadata::Row row("marks", i);
            EditorWidgetMetadata::next("mark", "marks");
            EditorWidgets::PushID(static_cast<int>(i));
            required_reference(("Mark " + std::to_string(i + 1)).c_str(),
                               value.marks[i], characters.marks);
            EditorWidgets::BeginDisabled(i == 0);
            const bool up = EditorWidgets::Button("Up");
            EditorWidgets::EndDisabled();
            EditorWidgets::SameLine();
            EditorWidgets::BeginDisabled(i + 1 == value.marks.size());
            const bool down = EditorWidgets::Button("Down");
            EditorWidgets::EndDisabled();
            EditorWidgets::SameLine();
            const bool remove = EditorWidgets::Button("Remove mark");
            EditorWidgets::PopID();
            if (up) std::swap(value.marks[i], value.marks[i - 1]);
            if (down) std::swap(value.marks[i], value.marks[i + 1]);
            if (remove)
              value.marks.erase(value.marks.begin() +
                                static_cast<std::ptrdiff_t>(i));
            if (up || down || remove) {
              EditorWidgetMetadata::structureChanged("marks");
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
