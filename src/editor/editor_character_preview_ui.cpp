#include <imgui.h>
#include "editor/editor_widget_metadata.hpp"

#include "editor/editor_ui.hpp"

std::optional<EditorCharacterPreviewRequest> EditorUi::drawCharacterPreview(
    const EditorDocument& document, EditorCharacterPreview& preview,
    bool can_start) {
  std::optional<EditorCharacterPreviewRequest> start;
  EditorWidgets::SetNextWindowPos({680, 450}, ImGuiCond_FirstUseEver);
  EditorWidgets::SetNextWindowSize({410, 320}, ImGuiCond_FirstUseEver);
  EditorWidgets::Begin("Character inspection");
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
    EditorWidgets::TextUnformatted("Open a level to inspect characters.");
    EditorWidgets::End();
    return start;
  }
  const bool route_mode =
      preview_request_.mode == EditorCharacterPreviewMode::Route;
  EditorWidgets::TextWrapped(
      route_mode ? "Schematic route: no collision, door operation or sound. "
                   "Use saved-file Play to check obstruction and audio."
                 : "Silent clip snapshot. Authored initial values and other "
                   "actors stay unchanged.");
  const auto selected = document.object(document.selection());
  const bool character_selected =
      selected && editorCharacterKind(*selected).has_value();
  if (!character_selected)
    EditorWidgets::TextUnformatted("Select an actor, mark or route in Objects.");
  EditorWidgets::BeginDisabled(!character_selected || preview.active());
  if (!route_mode) {
    const auto actor = document.object(preview_request_.actor);
    const std::string actor_label =
        actor ? std::get<CharacterActorDefinition>(*actor).id : "Choose actor";
    EditorWidgetMetadata::next("inspection-actor", "character.actor", "", "preview");
    if (EditorWidgets::BeginCombo("Inspection actor", actor_label.c_str())) {
      for (const auto id : document.characterIds(EditorCharacterKind::Actor)) {
        const auto value =
            std::get<CharacterActorDefinition>(*document.object(id));
        if (EditorWidgets::Selectable(value.id.c_str(), preview_request_.actor == id))
          preview_request_.actor = id;
      }
      EditorWidgets::EndCombo();
    }
    const auto mark = document.object(preview_request_.mark);
    const std::string mark_label =
        mark ? std::get<CharacterMarkDefinition>(*mark).id
             : "Actor initial mark";
    if (EditorWidgets::BeginCombo("Inspection mark", mark_label.c_str())) {
      if (EditorWidgets::Selectable("Actor initial mark", !preview_request_.mark))
        preview_request_.mark = 0;
      for (const auto id : document.characterIds(EditorCharacterKind::Mark)) {
        const auto value =
            std::get<CharacterMarkDefinition>(*document.object(id));
        if (EditorWidgets::Selectable(value.id.c_str(), preview_request_.mark == id))
          preview_request_.mark = id;
      }
      EditorWidgets::EndCombo();
    }
  }
  EditorWidgets::EndDisabled();
  if (route_mode && selected) {
    const auto& route = std::get<CharacterRouteDefinition>(*selected);
    EditorWidgets::Text("Route %s, owner %s", route.id.c_str(), route.actor.c_str());
    EditorWidgets::Text("Final action: %s",
                route.final_clip ? route.final_clip->c_str() : "None");
    for (std::size_t i = 0; i < route.marks.size(); ++i)
      EditorWidgets::Text("%zu: %s", i + 1, route.marks[i].c_str());
  } else {
    EditorWidgets::BeginDisabled(!character_selected || !can_start);
    EditorWidgetMetadata::next("preview-clip", "character.clip", "", "preview");
    if (EditorWidgets::BeginCombo("Preview clip", preview_request_.clip.c_str())) {
      for (const auto id : {"idle", "walk", "interact"})
        if (EditorWidgets::Selectable(id, preview_request_.clip == id)) {
          preview_request_.clip = id;
          if (preview.active()) preview.selectClip(id);
        }
      EditorWidgets::EndCombo();
    }
    EditorWidgets::EndDisabled();
  }
  const auto error =
      EditorCharacterPreview::startError(document, preview_request_);
  if (!error.empty()) EditorWidgets::TextWrapped("%s", error.c_str());
  if (!can_start)
    EditorWidgets::TextWrapped(
        "Inspection requires the current usable scene; finish the pending "
        "operation or repair resource errors.");
  EditorWidgets::BeginDisabled(!character_selected || !can_start || !error.empty() ||
                       preview.active());
  EditorWidgetMetadata::next("start-preview", "character.active", "", "preview");
  if (EditorWidgets::Button(route_mode ? "Start route snapshot"
                               : "Start clip snapshot"))
    start = preview_request_;
  EditorWidgets::EndDisabled();
  EditorWidgets::BeginDisabled(!preview.active() || !can_start);
  EditorWidgetMetadata::next("pause-preview", "character.paused", "", "preview");
  if (EditorWidgets::Button(preview.paused() ? "Resume preview" : "Pause preview"))
    preview.pause();
  EditorWidgets::SameLine();
  if (EditorWidgets::Button("Restart preview")) preview.restart();
  EditorWidgets::SameLine();
  if (EditorWidgets::Button("Stop preview")) preview.stop();
  if (preview.active()) {
    EditorWidgets::Text("%s: %.3f / %.3f s", preview.clip().data(), preview.time(),
                preview.duration());
    if (!route_mode) {
      float time = static_cast<float>(preview.time());
      EditorWidgetMetadata::next("clip-time", "character.time", "s", "preview",
                                 true);
      if (EditorWidgets::SliderFloat("Clip time", &time, 0,
                             static_cast<float>(preview.duration()), "%.3f s"))
        preview.seek(time);
    } else {
      constexpr const char* stages[]{
          "Standing turn to segment", "Walking (schematic)",
          "Standing turn to mark", "Final interaction (silent)", "Completed"};
      EditorWidgets::TextUnformatted(stages[static_cast<int>(preview.stage())]);
      EditorWidgets::Text(
          "Segment/target %zu: %s",
          preview.segment() +
              (preview.stage() == EditorRoutePreviewStage::Completed ||
                       preview.stage() == EditorRoutePreviewStage::Interaction
                   ? 0
                   : 1),
          preview.targetMark().data());
      const auto& p = preview.placement();
      EditorWidgets::Text("Feet %.3f, %.3f, %.3f; yaw %.1f", p.position[0],
                  p.position[1], p.position[2], p.yaw_degrees);
    }
  }
  EditorWidgets::EndDisabled();
  if (!preview.error().empty())
    EditorWidgets::TextWrapped("%s", preview.error().data());
  EditorWidgets::End();
  return start;
}
