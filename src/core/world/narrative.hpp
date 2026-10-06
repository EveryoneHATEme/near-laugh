#ifndef CORE_WORLD_NARRATIVE_HPP
#define CORE_WORLD_NARRATIVE_HPP

#include <array>
#include <string_view>
#include <variant>

#include "core/world/level_document.hpp"

// Persisted names, indexed by enum value or variant alternative.
inline constexpr std::array<std::string_view, 7> narrative_actor_state_names{
    "idle",        "turning",   "walking", "blocked",
    "interacting", "completed", "canceled"};
inline constexpr std::array<std::string_view, 3> narrative_event_state_names{
    "completed", "canceled", "failed"};
inline constexpr std::array<std::string_view, 5> narrative_target_kind_names{
    "door", "switch", "radio", "document", "box"};
inline constexpr std::array<std::string_view, 10> narrative_action_names{
    "door_interact", "door_lock", "door_knock",    "switch_activate",
    "radio_on",      "radio_off", "document_open", "box_pickup",
    "box_drop",      "box_throw"};
inline constexpr std::array<std::string_view, 2> narrative_repeat_names{
    "once", "rearm"};
inline constexpr std::array<std::string_view, 11> narrative_predicate_kind_names{
    "fact",  "region", "light",    "door_endpoint", "door_locked", "radio",
    "box",   "document", "actor",  "event",         "elapsed"};
inline constexpr std::array<std::string_view, 4> narrative_trigger_kind_names{
    "scene_entry", "region_entry", "interaction", "condition"};
inline constexpr std::array<std::string_view, 9> narrative_step_kind_names{
    "set_fact",  "set_light", "set_door_open", "set_door_locked", "set_radio",
    "play_cue",  "run_route", "delay",         "wait_until"};
static_assert(narrative_predicate_kind_names.size() ==
              std::variant_size_v<NarrativePredicate>);
static_assert(narrative_trigger_kind_names.size() ==
              std::variant_size_v<NarrativeTrigger>);
static_assert(narrative_step_kind_names.size() ==
              std::variant_size_v<NarrativeStep>);

[[nodiscard]] const NarrativeFactDefinition* findNarrativeFact(
    const LevelNarrative& narrative, std::string_view id) noexcept;
[[nodiscard]] const NarrativeRegionDefinition* findNarrativeRegion(
    const LevelNarrative& narrative, std::string_view id) noexcept;
[[nodiscard]] const NarrativeEventDefinition* findNarrativeEvent(
    const LevelNarrative& narrative, std::string_view id) noexcept;
[[nodiscard]] bool narrativeRegionContains(
    const NarrativeRegionDefinition& region, WorldPosition feet) noexcept;
// Metadata only: missing resource files are a startup/Play concern.
[[nodiscard]] std::vector<LevelDiagnostic> validateNarrativeDefinitions(
    const LevelDocument& document,
    const std::filesystem::path& source_path = {});

#endif
