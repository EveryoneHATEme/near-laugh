#include "core/world/narrative.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <type_traits>

#include "core/world/audio.hpp"
#include "core/world/characters.hpp"

namespace {
template <class Records>
const typename Records::value_type* find(const Records& records,
                                         std::string_view id) noexcept {
  const auto it =
      std::find_if(records.begin(), records.end(),
                   [id](const auto& record) { return record.id == id; });
  return it == records.end() ? nullptr : &*it;
}
}  // namespace

const NarrativeFactDefinition* findNarrativeFact(
    const LevelNarrative& narrative, std::string_view id) noexcept {
  return find(narrative.facts, id);
}
const NarrativeRegionDefinition* findNarrativeRegion(
    const LevelNarrative& narrative, std::string_view id) noexcept {
  return find(narrative.regions, id);
}
const NarrativeEventDefinition* findNarrativeEvent(
    const LevelNarrative& narrative, std::string_view id) noexcept {
  return find(narrative.events, id);
}
bool narrativeRegionContains(const NarrativeRegionDefinition& region,
                             WorldPosition feet) noexcept {
  return feet.x >= region.center.x - region.half_extent.x &&
         feet.x < region.center.x + region.half_extent.x &&
         feet.y >= region.center.y - region.half_extent.y &&
         feet.y < region.center.y + region.half_extent.y &&
         feet.z >= region.center.z - region.half_extent.z &&
         feet.z < region.center.z + region.half_extent.z;
}

std::vector<LevelDiagnostic> validateNarrativeDefinitions(
    const LevelDocument& document, const std::filesystem::path& source_path) {
  std::vector<LevelDiagnostic> diagnostics;
  const auto& narrative = document.narrative;
  const auto error = [&](std::string path, std::string message) {
    diagnostics.push_back({LevelDiagnosticCategory::Validation,
                           source_path,
                           std::move(path),
                           std::move(message),
                           {}});
  };
  const auto ids = [&](const auto& records, std::size_t maximum,
                       std::string_view collection) {
    const auto path = "narrative." + std::string(collection);
    if (records.size() > maximum)
      error(path, "exceeds the " + std::to_string(maximum) + "-record limit");
    std::set<std::string_view> seen;
    for (std::size_t i = 0; i < records.size(); ++i) {
      const auto field = path + "[" + std::to_string(i) + "].id";
      const auto& id = records[i].id;
      if (!levelEntryIdIsValid(id))
        error(field, "'" + id + "' must match [a-z][a-z0-9-]{0,63}");
      if (!seen.insert(id).second)
        error(field, "duplicate identifier '" + id + "'");
    }
  };
  ids(narrative.facts, level_maximum_narrative_fact_count, "facts");
  ids(narrative.regions, level_maximum_narrative_region_count, "regions");
  ids(narrative.events, level_maximum_narrative_event_count, "events");
  for (std::size_t i = 0; i < narrative.regions.size(); ++i) {
    const auto& r = narrative.regions[i];
    const auto p = "narrative.regions[" + std::to_string(i) + "]";
    const auto label = "region '" + r.id + "': ";
    if (!std::isfinite(r.center.x) || !std::isfinite(r.center.y) ||
        !std::isfinite(r.center.z))
      error(p + ".center", label + "must be finite");
    const auto extent = [](float c, float e) {
      // Rounded float bounds must still enclose a half-open interval.
      return e > 0 && std::isfinite(e) && std::isfinite(c - e) &&
             std::isfinite(c + e) && c - e < c + e;
    };
    if (!extent(r.center.x, r.half_extent.x) ||
        !extent(r.center.y, r.half_extent.y) ||
        !extent(r.center.z, r.half_extent.z))
      error(p + ".half_extent",
            label + "must be positive with finite, nondegenerate bounds");
  }
  for (std::size_t index = 0; index < narrative.events.size(); ++index) {
    const auto& event = narrative.events[index];
    const auto base = "narrative.events[" + std::to_string(index) + "]";
    const auto label = "event '" + event.id + "': ";
    const auto reference = [&](const auto& records, const std::string& id,
                               const std::string& p, std::string_view kind) {
      if (!levelEntryIdIsValid(id) || !find(records, id))
        error(p, label + "unknown " + std::string(kind) + " '" + id + "'");
    };
    const auto duration = [&](float seconds, const std::string& p) {
      if (!std::isfinite(seconds) || seconds < 0 ||
          seconds > level_maximum_narrative_seconds)
        error(p, label + "must be finite and in [0,3600] seconds");
    };
    const auto predicates = [&](const std::vector<NarrativePredicate>& values,
                                const std::string& path, bool empty_allowed) {
      if ((!empty_allowed && values.empty()) ||
          values.size() > level_maximum_narrative_predicate_count)
        error(path, label + (empty_allowed ? "requires 0-8 predicates"
                                           : "requires 1-8 predicates"));
      for (std::size_t i = 0; i < values.size(); ++i) {
        const auto p = path + "[" + std::to_string(i) + "]";
        std::visit(
            [&](const auto& v) {
              using T = std::decay_t<decltype(v)>;
              if constexpr (std::is_same_v<T, NarrativeFactPredicate>)
                reference(narrative.facts, v.fact, p + ".fact", "fact");
              else if constexpr (std::is_same_v<T, NarrativeRegionPredicate>)
                reference(narrative.regions, v.region, p + ".region", "region");
              else if constexpr (std::is_same_v<T, NarrativeLightPredicate>)
                reference(document.environment_light.point_lights, v.light,
                          p + ".light", "light");
              else if constexpr (std::is_same_v<
                                     T, NarrativeDoorEndpointPredicate> ||
                                 std::is_same_v<T,
                                                NarrativeDoorLockedPredicate>)
                reference(document.doors, v.door, p + ".door", "door");
              else if constexpr (std::is_same_v<T, NarrativeRadioPredicate>)
                reference(document.household.radios, v.radio, p + ".radio",
                          "radio");
              else if constexpr (std::is_same_v<T, NarrativeBoxPredicate>)
                reference(document.household.boxes, v.box, p + ".box", "box");
              else if constexpr (std::is_same_v<T, NarrativeDocumentPredicate>)
                reference(document.household.documents, v.document,
                          p + ".document", "document");
              else if constexpr (std::is_same_v<T, NarrativeActorPredicate>) {
                reference(document.characters.actors, v.actor, p + ".actor",
                          "actor");
                if (v.state < NarrativeActorState::Idle ||
                    v.state > NarrativeActorState::Canceled)
                  error(p + ".state", label + "unsupported actor state");
              } else if constexpr (std::is_same_v<T, NarrativeEventPredicate>) {
                reference(narrative.events, v.event, p + ".event", "event");
                if (v.event == event.id)
                  error(p + ".event",
                        label + "self event references are unsupported");
                if (v.state < NarrativeEventTerminalState::Completed ||
                    v.state > NarrativeEventTerminalState::Failed)
                  error(p + ".state",
                        label + "unsupported event terminal state");
              } else if constexpr (std::is_same_v<T, NarrativeElapsedPredicate>)
                duration(v.seconds, p + ".seconds");
            },
            values[i]);
      }
    };
    predicates(event.guards, base + ".guards", true);
    if (event.cancel) predicates(*event.cancel, base + ".cancel", false);
    if (event.repeat != NarrativeRepeat::Once &&
        event.repeat != NarrativeRepeat::Rearm)
      error(base + ".repeat", label + "unsupported repeat mode");
    std::visit(
        [&](const auto& v) {
          using T = std::decay_t<decltype(v)>;
          const auto p = base + ".trigger";
          if constexpr (std::is_same_v<T, NarrativeSceneEntryTrigger>) {
            if (event.repeat != NarrativeRepeat::Once)
              error(base + ".repeat", label + "scene entry requires once mode");
          } else if constexpr (std::is_same_v<T, NarrativeRegionEntryTrigger>)
            reference(narrative.regions, v.region, p + ".region", "region");
          else if constexpr (std::is_same_v<T, NarrativeConditionTrigger>)
            predicates(v.predicates, p + ".predicates", false);
          else if constexpr (std::is_same_v<T, NarrativeInteractionTrigger>) {
            bool compatible = false;
            switch (v.target_kind) {
              case NarrativeInteractionTarget::Door:
                reference(document.doors, v.target, p + ".target", "door");
                compatible =
                    v.action == NarrativeInteractionAction::DoorInteract ||
                    v.action == NarrativeInteractionAction::DoorLock ||
                    v.action == NarrativeInteractionAction::DoorKnock;
                break;
              case NarrativeInteractionTarget::Switch:
                reference(document.light_switches, v.target, p + ".target",
                          "switch");
                compatible =
                    v.action == NarrativeInteractionAction::SwitchActivate;
                break;
              case NarrativeInteractionTarget::Radio:
                reference(document.household.radios, v.target, p + ".target",
                          "radio");
                compatible = v.action == NarrativeInteractionAction::RadioOn ||
                             v.action == NarrativeInteractionAction::RadioOff;
                break;
              case NarrativeInteractionTarget::Document:
                reference(document.household.documents, v.target, p + ".target",
                          "document");
                compatible =
                    v.action == NarrativeInteractionAction::DocumentOpen;
                break;
              case NarrativeInteractionTarget::Box:
                reference(document.household.boxes, v.target, p + ".target",
                          "box");
                compatible =
                    v.action == NarrativeInteractionAction::BoxPickup ||
                    v.action == NarrativeInteractionAction::BoxDrop ||
                    v.action == NarrativeInteractionAction::BoxThrow;
                break;
              default:
                error(p + ".target_kind",
                      label + "unsupported interaction target");
            }
            if (!compatible)
              error(p + ".action",
                    label + "action is not supported for this target kind");
          }
        },
        event.trigger);
    if (event.steps.empty() ||
        event.steps.size() > level_maximum_narrative_step_count)
      error(base + ".steps", label + "requires 1-32 steps");
    for (std::size_t i = 0; i < event.steps.size(); ++i) {
      const auto p = base + ".steps[" + std::to_string(i) + "]";
      std::visit(
          [&](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, NarrativeSetFactStep>)
              reference(narrative.facts, v.fact, p + ".fact", "fact");
            else if constexpr (std::is_same_v<T, NarrativeSetLightStep>)
              reference(document.environment_light.point_lights, v.light,
                        p + ".light", "light");
            else if constexpr (std::is_same_v<T, NarrativeSetDoorOpenStep> ||
                               std::is_same_v<T, NarrativeSetDoorLockedStep>)
              reference(document.doors, v.door, p + ".door", "door");
            else if constexpr (std::is_same_v<T, NarrativeSetRadioStep>)
              reference(document.household.radios, v.radio, p + ".radio",
                        "radio");
            else if constexpr (std::is_same_v<T, NarrativePlayCueStep>) {
              reference(document.audio.sources, v.source, p + ".source",
                        "source");
              const auto* source = find(document.audio.sources, v.source);
              if (source) {
                const auto* cue = findAudioCue(document.audio, source->cue);
                if (source->autoplay || !cue || cue->loop)
                  error(p + ".source",
                        label + "sequence source '" + v.source +
                            "' requires a non-autoplay one-shot cue");
                for (const auto& actor : document.characters.actors)
                  if (actor.footstep_source == v.source ||
                      actor.interaction_source == v.source)
                    error(p + ".source",
                          label + "source '" + v.source + "' is actor-owned");
                for (const auto& radio : document.household.radios)
                  if (radio.source == v.source)
                    error(p + ".source",
                          label + "source '" + v.source + "' is radio-owned");
              }
            } else if constexpr (std::is_same_v<T, NarrativeRunRouteStep>) {
              reference(document.characters.actors, v.actor, p + ".actor",
                        "actor");
              reference(document.characters.routes, v.route, p + ".route",
                        "route");
              const auto* route =
                  findCharacterRoute(document.characters, v.route);
              if (route && route->actor != v.actor)
                error(p + ".route",
                      label + "route belongs to actor '" + route->actor + "'");
            } else if constexpr (std::is_same_v<T, NarrativeDelayStep>)
              duration(v.seconds, p + ".seconds");
            else if constexpr (std::is_same_v<T, NarrativeWaitUntilStep>)
              predicates(v.predicates, p + ".predicates", false);
          },
          event.steps[i]);
    }
  }
  return diagnostics;
}
