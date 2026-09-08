#include "core/world/characters.hpp"

#include <algorithm>
#include <cmath>
#include <set>

#include "core/world/audio.hpp"

const CharacterCatalogEntry* findCharacterModel(std::string_view id) noexcept {
  return id == test_mannequin_catalog.id ? &test_mannequin_catalog : nullptr;
}
const CharacterMarkDefinition* findCharacterMark(
    const LevelCharacters& characters, std::string_view id) noexcept {
  const auto it =
      std::find_if(characters.marks.begin(), characters.marks.end(),
                   [id](const auto& mark) { return mark.id == id; });
  return it == characters.marks.end() ? nullptr : &*it;
}
const CharacterRouteDefinition* findCharacterRoute(
    const LevelCharacters& characters, std::string_view id) noexcept {
  const auto it =
      std::find_if(characters.routes.begin(), characters.routes.end(),
                   [id](const auto& route) { return route.id == id; });
  return it == characters.routes.end() ? nullptr : &*it;
}

std::vector<LevelDiagnostic> validateCharacterDefinitions(
    const LevelCharacters& characters, const LevelAudio& audio,
    const std::filesystem::path& source_path) {
  std::vector<LevelDiagnostic> diagnostics;
  const auto error = [&](std::string path, std::string message) {
    diagnostics.push_back({LevelDiagnosticCategory::Validation,
                           source_path,
                           std::move(path),
                           std::move(message),
                           {}});
  };
  const auto checkIds = [&](const auto& records, std::size_t maximum,
                            std::string_view collection) {
    const auto path = "characters." + std::string(collection);
    if (records.size() > maximum)
      error(path, "exceeds the " + std::to_string(maximum) + "-record limit");
    std::set<std::string_view> ids;
    for (std::size_t i = 0; i < records.size(); ++i) {
      const auto& id = records[i].id;
      const auto field = path + "[" + std::to_string(i) + "].id";
      if (!levelEntryIdIsValid(id))
        error(field, "'" + id + "' must match [a-z][a-z0-9-]{0,63}");
      if (!ids.insert(id).second)
        error(field, "duplicate identifier '" + id + "'");
    }
  };
  checkIds(characters.actors, level_maximum_actor_count, "actors");
  checkIds(characters.marks, level_maximum_character_mark_count, "marks");
  checkIds(characters.routes, level_maximum_character_route_count, "routes");
  std::set<std::string_view> owned_sources;
  for (std::size_t i = 0; i < characters.actors.size(); ++i) {
    const auto& actor = characters.actors[i];
    const auto p = "characters.actors[" + std::to_string(i) + "]";
    const auto label = "actor '" + actor.id + "': ";
    const auto* model = findCharacterModel(actor.model);
    if (!model)
      error(p + ".model",
            label + "unknown catalog model '" + actor.model + "'");
    if (!std::isfinite(actor.speed) || actor.speed < 0.25F ||
        actor.speed > 1.5F ||
        (model && (actor.speed < model->minimum_speed_m_s ||
                   actor.speed > model->maximum_speed_m_s)))
      error(p + ".speed",
            label + "must be within the catalog speed range [0.25,1.5]");
    if (!findCharacterMark(characters, actor.initial_mark))
      error(p + ".initial_mark",
            label + "unknown mark '" + actor.initial_mark + "'");
    if (actor.initial_route) {
      const auto* route = findCharacterRoute(characters, *actor.initial_route);
      if (!route)
        error(p + ".initial_route",
              label + "unknown route '" + *actor.initial_route + "'");
      else if (route->actor != actor.id)
        error(p + ".initial_route",
              label + "route belongs to actor '" + route->actor + "'");
    }
    const auto source = [&](const std::optional<std::string>& id,
                            bool footstep) {
      if (!id) return;
      const auto field =
          p + (footstep ? ".footstep_source" : ".interaction_source");
      if (!owned_sources.insert(*id).second)
        error(field, label + "source '" + *id +
                         "' must be exclusively owned by one actor role");
      const auto it = std::find_if(audio.sources.begin(), audio.sources.end(),
                                   [&](const auto& s) { return s.id == *id; });
      if (it == audio.sources.end()) {
        error(field, label + "unknown source '" + *id + "'");
        return;
      }
      const auto* cue = findAudioCue(audio, it->cue);
      if (it->autoplay || !cue || !cue->spatial || cue->loop ||
          cue->kind !=
              (footstep ? AudioCueKind::Ambience : AudioCueKind::Essential) ||
          (!footstep && !cue->caption))
        error(field,
              label + "source '" + *id +
                  "' requires a spatial non-autoplay one-shot " +
                  (footstep ? "ambience cue" : "essential captioned cue"));
    };
    source(actor.footstep_source, true);
    source(actor.interaction_source, false);
  }
  for (std::size_t i = 0; i < characters.marks.size(); ++i) {
    const auto& mark = characters.marks[i];
    const auto p = "characters.marks[" + std::to_string(i) + "]";
    const auto label = "mark '" + mark.id + "': ";
    if (!std::isfinite(mark.feet_position.x) ||
        !std::isfinite(mark.feet_position.y) ||
        !std::isfinite(mark.feet_position.z))
      error(p + ".feet_position", label + "must be finite");
    if (!std::isfinite(mark.yaw_degrees))
      error(p + ".yaw_degrees", label + "must be finite");
  }
  for (std::size_t i = 0; i < characters.routes.size(); ++i) {
    const auto& route = characters.routes[i];
    const auto p = "characters.routes[" + std::to_string(i) + "]";
    const auto label = "route '" + route.id + "': ";
    if (std::none_of(
            characters.actors.begin(), characters.actors.end(),
            [&](const auto& actor) { return actor.id == route.actor; }))
      error(p + ".actor", label + "unknown actor '" + route.actor + "'");
    if (route.marks.empty() ||
        route.marks.size() > level_maximum_route_mark_count)
      error(p + ".marks", label + "requires 1 through 32 ordered marks");
    for (std::size_t j = 0; j < route.marks.size(); ++j)
      if (!findCharacterMark(characters, route.marks[j]))
        error(p + ".marks[" + std::to_string(j) + "]",
              label + "unknown mark '" + route.marks[j] + "'");
    if (route.final_clip && *route.final_clip != "interact")
      error(p + ".final_clip", label + "only null or interact is supported");
  }
  return diagnostics;
}
