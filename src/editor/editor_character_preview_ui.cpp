#include <imgui.h>

#include "editor/editor_ui.hpp"

std::optional<EditorCharacterPreviewRequest> EditorUi::drawCharacterPreview(
    const EditorDocument& document, EditorCharacterPreview& preview,
    bool can_start) {
  std::optional<EditorCharacterPreviewRequest> start;
  ImGui::SetNextWindowPos({680, 450}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({410, 320}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Character inspection");
  preview.synchronize(document);
  if (preview_generation_ != document.generation() ||
      preview_revision_ != document.revision() ||
      preview_selection_revision_ != document.selectionRevision() ||
      preview_selection_ != document.selection()) {
    preview_generation_ = document.generation();
    preview_revision_ = document.revision();
    preview_selection_revision_ = document.selectionRevision();
    preview_selection_ = document.selection();
    preview_request_ = {};
    const auto selected = document.object(document.selection());
    if (selected) {
      if (std::holds_alternative<CharacterActorDefinition>(*selected))
        preview_request_.actor = document.selection();
      if (std::holds_alternative<CharacterMarkDefinition>(*selected))
        preview_request_.mark = document.selection();
      if (const auto* route =
              std::get_if<CharacterRouteDefinition>(&*selected)) {
        preview_request_.mode = EditorCharacterPreviewMode::Route;
        for (const auto id : document.characterIds(EditorCharacterKind::Actor))
          if (std::get<CharacterActorDefinition>(*document.object(id)).id ==
              route->actor)
            preview_request_.actor = id;
      }
    }
  }
  if (!document.document()) {
    ImGui::TextUnformatted("Open a level to inspect characters.");
    ImGui::End();
    return start;
  }
  const bool route_mode =
      preview_request_.mode == EditorCharacterPreviewMode::Route;
  ImGui::TextWrapped(
      route_mode ? "Schematic route: no collision, door operation or sound. "
                   "Use saved-file Play to check obstruction and audio."
                 : "Silent clip snapshot. Authored initial values and other "
                   "actors stay unchanged.");
  const auto selected = document.object(document.selection());
  const bool character_selected =
      selected && editorCharacterKind(*selected).has_value();
  if (!character_selected)
    ImGui::TextUnformatted("Select an actor, mark or route in Objects.");
  ImGui::BeginDisabled(!character_selected || preview.active());
  if (!route_mode) {
    const auto actor = document.object(preview_request_.actor);
    const std::string actor_label =
        actor ? std::get<CharacterActorDefinition>(*actor).id : "Choose actor";
    if (ImGui::BeginCombo("Inspection actor", actor_label.c_str())) {
      for (const auto id : document.characterIds(EditorCharacterKind::Actor)) {
        const auto value =
            std::get<CharacterActorDefinition>(*document.object(id));
        if (ImGui::Selectable(value.id.c_str(), preview_request_.actor == id))
          preview_request_.actor = id;
      }
      ImGui::EndCombo();
    }
    const auto mark = document.object(preview_request_.mark);
    const std::string mark_label =
        mark ? std::get<CharacterMarkDefinition>(*mark).id
             : "Actor initial mark";
    if (ImGui::BeginCombo("Inspection mark", mark_label.c_str())) {
      if (ImGui::Selectable("Actor initial mark", !preview_request_.mark))
        preview_request_.mark = 0;
      for (const auto id : document.characterIds(EditorCharacterKind::Mark)) {
        const auto value =
            std::get<CharacterMarkDefinition>(*document.object(id));
        if (ImGui::Selectable(value.id.c_str(), preview_request_.mark == id))
          preview_request_.mark = id;
      }
      ImGui::EndCombo();
    }
  }
  ImGui::EndDisabled();
  if (route_mode && selected) {
    const auto& route = std::get<CharacterRouteDefinition>(*selected);
    ImGui::Text("Route %s, owner %s", route.id.c_str(), route.actor.c_str());
    ImGui::Text("Final action: %s",
                route.final_clip ? route.final_clip->c_str() : "None");
    for (std::size_t i = 0; i < route.marks.size(); ++i)
      ImGui::Text("%zu: %s", i + 1, route.marks[i].c_str());
  } else {
    ImGui::BeginDisabled(!character_selected || !can_start);
    if (ImGui::BeginCombo("Preview clip", preview_request_.clip.c_str())) {
      for (const auto id : {"idle", "walk", "interact"})
        if (ImGui::Selectable(id, preview_request_.clip == id)) {
          preview_request_.clip = id;
          if (preview.active()) preview.selectClip(id);
        }
      ImGui::EndCombo();
    }
    ImGui::EndDisabled();
  }
  const auto error =
      EditorCharacterPreview::startError(document, preview_request_);
  if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
  if (!can_start)
    ImGui::TextWrapped(
        "Inspection requires the current usable scene; finish the pending "
        "operation or repair resource errors.");
  ImGui::BeginDisabled(!character_selected || !can_start || !error.empty() ||
                       preview.active());
  if (ImGui::Button(route_mode ? "Start route snapshot"
                               : "Start clip snapshot"))
    start = preview_request_;
  ImGui::EndDisabled();
  ImGui::BeginDisabled(!preview.active() || !can_start);
  if (ImGui::Button(preview.paused() ? "Resume preview" : "Pause preview"))
    preview.pause();
  ImGui::SameLine();
  if (ImGui::Button("Restart preview")) preview.restart();
  ImGui::SameLine();
  if (ImGui::Button("Stop preview")) preview.stop();
  if (preview.active()) {
    ImGui::Text("%s: %.3f / %.3f s", preview.clip().data(), preview.time(),
                preview.duration());
    if (!route_mode) {
      float time = static_cast<float>(preview.time());
      if (ImGui::SliderFloat("Clip time", &time, 0,
                             static_cast<float>(preview.duration()), "%.3f s"))
        preview.seek(time);
    } else {
      constexpr const char* stages[]{
          "Standing turn to segment", "Walking (schematic)",
          "Standing turn to mark", "Final interaction (silent)", "Completed"};
      ImGui::TextUnformatted(stages[static_cast<int>(preview.stage())]);
      ImGui::Text(
          "Segment/target %zu: %s",
          preview.segment() +
              (preview.stage() == EditorRoutePreviewStage::Completed ||
                       preview.stage() == EditorRoutePreviewStage::Interaction
                   ? 0
                   : 1),
          preview.targetMark().data());
      const auto& p = preview.placement();
      ImGui::Text("Feet %.3f, %.3f, %.3f; yaw %.1f", p.position[0],
                  p.position[1], p.position[2], p.yaw_degrees);
    }
  }
  ImGui::EndDisabled();
  if (!preview.error().empty())
    ImGui::TextWrapped("%s", preview.error().data());
  ImGui::End();
  return start;
}
