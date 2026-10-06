#include <algorithm>
#include <array>
#include <cstring>
#include <type_traits>

#include "core/world/narrative.hpp"
#include "editor/editor_ui.hpp"
#include "editor/editor_widget_metadata.hpp"

namespace {
constexpr std::array predicateNames{
    "Fact",        "Region occupancy", "Light enabled", "Door endpoint",
    "Door locked", "Radio enabled",    "Box held",      "Document open",
    "Actor state", "Event terminal",   "Elapsed time"};
constexpr std::array stepNames{"Set fact",        "Set light", "Set door open",
                               "Set door locked", "Set radio", "Play cue",
                               "Run route",       "Delay",     "Wait until"};
constexpr std::array triggerNames{"Scene entry", "Region entry",
                                  "Player interaction", "Condition transition"};
constexpr std::array actionNames{"Door interact",   "Door lock",  "Door knock",
                                 "Switch activate", "Radio on",   "Radio off",
                                 "Document open",   "Box pickup", "Box drop",
                                 "Box throw"};

template <std::size_t I = 0, class V>
V newKind(std::size_t index) {
  if constexpr (I + 1 < std::variant_size_v<V>) {
    if (index != I) return newKind<I + 1, V>(index);
  }
  return V{std::in_place_index<I>};
}
void metadata(std::string_view key, const std::string& path) {
  EditorWidgetMetadata::next(key, path.substr(0, path.find('.')), "", "object");
}
bool choice(const char* label, std::size_t& index, const auto& names,
            const std::string& path) {
  metadata(label, path);
  bool changed = false;
  // Shared world names are string_views over null-terminated literals.
  const auto name = [&](std::size_t i) { return std::string_view(names[i]).data(); };
  if (EditorWidgets::BeginCombo(
          label, index < names.size() ? name(index) : "<unsupported>")) {
    for (std::size_t i = 0; i < names.size(); ++i)
      if (EditorWidgets::Selectable(name(i), i == index)) {
        index = i;
        changed = true;
      }
    EditorWidgets::EndCombo();
  }
  return changed;
}
bool reference(const char* label, std::string& value, const auto& values,
               const std::string& path) {
  metadata(path.substr(path.rfind('.') + 1), path);
  bool changed = false;
  if (EditorWidgets::BeginCombo(label,
                                value.empty() ? "<missing>" : value.c_str())) {
    for (const auto& record : values)
      if (EditorWidgets::Selectable(record.id.c_str(), value == record.id)) {
        value = record.id;
        changed = true;
      }
    EditorWidgets::EndCombo();
  }
  if (std::none_of(values.begin(), values.end(),
                   [&](const auto& r) { return r.id == value; }))
    EditorWidgets::TextWrapped("Unresolved %s: %s", label,
                               value.empty() ? "<empty>" : value.c_str());
  return changed;
}
bool parameters(auto& v, const LevelDocument& level, const std::string& path) {
  bool commit = false;
  if constexpr (requires { v.fact; })
    commit |= reference("Fact", v.fact, level.narrative.facts, path + ".fact");
  if constexpr (requires { v.region; })
    commit |= reference("Region", v.region, level.narrative.regions,
                        path + ".region");
  if constexpr (requires { v.light; })
    commit |= reference("Light", v.light, level.environment_light.point_lights,
                        path + ".light");
  if constexpr (requires { v.door; })
    commit |= reference("Door", v.door, level.doors, path + ".door");
  if constexpr (requires { v.radio; })
    commit |=
        reference("Radio", v.radio, level.household.radios, path + ".radio");
  if constexpr (requires { v.box; })
    commit |= reference("Box", v.box, level.household.boxes, path + ".box");
  if constexpr (requires { v.document; })
    commit |= reference("Document", v.document, level.household.documents,
                        path + ".document");
  if constexpr (requires { v.actor; })
    commit |=
        reference("Actor", v.actor, level.characters.actors, path + ".actor");
  if constexpr (requires { v.route; })
    commit |=
        reference("Route", v.route, level.characters.routes, path + ".route");
  if constexpr (requires { v.event; })
    commit |=
        reference("Event", v.event, level.narrative.events, path + ".event");
  if constexpr (requires { v.source; })
    commit |=
        reference("Source", v.source, level.audio.sources, path + ".source");
  const auto boolean = [&](const char* label, const char* field, bool& value) {
    metadata(field, path + "." + field);
    commit |= EditorWidgets::Checkbox(label, &value);
  };
  if constexpr (requires { v.value; }) boolean("Value", "value", v.value);
  if constexpr (requires { v.inside; }) boolean("Inside", "inside", v.inside);
  if constexpr (requires { v.enabled; })
    boolean("Enabled", "enabled", v.enabled);
  if constexpr (requires { v.open; }) boolean("Open", "open", v.open);
  if constexpr (requires { v.locked; }) boolean("Locked", "locked", v.locked);
  if constexpr (requires { v.held; }) boolean("Held", "held", v.held);
  if constexpr (requires { v.seconds; }) {
    metadata("seconds", path + ".seconds");
    EditorWidgets::InputFloat("Seconds", &v.seconds, 0, 0, "%.3f");
    commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
  }
  if constexpr (requires { v.state; }) {
    std::size_t index = static_cast<std::size_t>(v.state);
    if constexpr (std::is_same_v<std::decay_t<decltype(v.state)>,
                                 NarrativeActorState>) {
      if (choice("State", index, narrative_actor_state_names, path + ".state")) {
        v.state = static_cast<NarrativeActorState>(index);
        commit = true;
      }
    } else if (choice("State", index, narrative_event_state_names, path + ".state")) {
      v.state = static_cast<NarrativeEventTerminalState>(index);
      commit = true;
    }
  }
  return commit;
}
// Structural operations are ordinary whole-record history commands. Break after
// a row changes so references never survive vector replacement or reordering.
bool predicates(std::vector<NarrativePredicate>& list,
                const LevelDocument& level, const std::string& path) {
  bool commit = false;
  EditorWidgets::PushID(path.c_str());
  for (std::size_t i = 0; i < list.size(); ++i) {
    EditorWidgetMetadata::Row row(path, i);
    EditorWidgets::PushID(static_cast<int>(i));
    const auto field = path + "." + std::to_string(i);
    EditorWidgets::Text("Condition %zu", i + 1);
    auto kind = list[i].index();
    if (choice("condition-kind", kind, predicateNames, field + ".kind")) {
      EditorWidgetMetadata::structureChanged(path);
      list[i] = newKind<0, NarrativePredicate>(kind);
      commit = true;
    }
    std::visit([&](auto& v) { commit |= parameters(v, level, field); },
               list[i]);
    EditorWidgets::BeginDisabled(i == 0);
    const bool up = EditorWidgets::Button("Move condition up");
    EditorWidgets::EndDisabled();
    EditorWidgets::SameLine();
    EditorWidgets::BeginDisabled(i + 1 == list.size());
    const bool down = EditorWidgets::Button("Move condition down");
    EditorWidgets::EndDisabled();
    const bool remove = EditorWidgets::Button("Remove condition");
    EditorWidgets::PopID();
    if (up || down || remove) {
      EditorWidgetMetadata::structureChanged(path);
      if (remove)
        list.erase(list.begin() + i);
      else
        std::swap(list[i], list[up ? i - 1 : i + 1]);
      commit = true;
      break;
    }
  }
  EditorWidgets::BeginDisabled(list.size() >=
                               level_maximum_narrative_predicate_count);
  EditorWidgetMetadata::next(path + "-add-condition");
  if (EditorWidgets::Button("Add condition")) {
    EditorWidgetMetadata::structureChanged(path);
    list.emplace_back(NarrativeFactPredicate{});
    commit = true;
  }
  EditorWidgets::EndDisabled();
  EditorWidgets::PopID();
  return commit;
}
}  // namespace

void EditorUi::drawNarrativeObjects(EditorDocument& document) {
  if (!EditorWidgets::CollapsingHeader("Narrative",
                                       ImGuiTreeNodeFlags_DefaultOpen))
    return;
  const std::array buttons{"Add fact", "Add region", "Add event"};
  const std::array labels{"Fact: ", "Region: ", "Event: "};
  const std::array limits{level_maximum_narrative_fact_count,
                          level_maximum_narrative_region_count,
                          level_maximum_narrative_event_count};
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    const auto kind = static_cast<EditorNarrativeKind>(i);
    EditorWidgets::BeginDisabled(document.narrativeIds(kind).size() >=
                                 limits[i]);
    const bool add = EditorWidgets::Button(buttons[i]);
    EditorWidgets::EndDisabled();
    if (add && commitSelectionDraft(document) && document.addNarrative(kind))
      placing_ = sculpting_ = false;
  }
  for (std::size_t kind = 0; kind < labels.size(); ++kind)
    for (auto id :
         document.narrativeIds(static_cast<EditorNarrativeKind>(kind))) {
      const auto value = *document.object(id);
      std::visit(
          [&](const auto& record) {
            if constexpr (requires { record.id; }) {
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
  EditorWidgets::Separator();
}

void EditorUi::drawNarrativeProperties(EditorDocument& document) {
  const auto& level = *document.document();
  bool commit = false;
  const auto text = [&](std::string& value) {
    std::array<char, 65> buffer{};
    std::memcpy(buffer.data(), value.data(),
                std::min(value.size(), buffer.size() - 1));
    metadata("narrative-id", "id");
    if (EditorWidgets::InputText("Narrative ID", buffer.data(), buffer.size()))
      value = buffer.data();
    commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
  };
  std::visit(
      [&](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, NarrativeFactDefinition>) {
          text(value.id);
          metadata("initial-value", "initial_value");
          commit |=
              EditorWidgets::Checkbox("Initial value", &value.initial_value);
        } else if constexpr (std::is_same_v<T, NarrativeRegionDefinition>) {
          text(value.id);
          const auto vector = [&](const char* label, auto& v,
                                  const char* field) {
            float xyz[]{v.x, v.y, v.z};
            metadata(field, field);
            if (EditorWidgets::DragFloat3(label, xyz, .05F, 0, 0, "%.3f"))
              v = {xyz[0], xyz[1], xyz[2]};
            commit |= EditorWidgets::IsItemDeactivatedAfterEdit();
          };
          vector("Center", value.center, "center");
          vector("Half extents", value.half_extent, "half_extent");
          EditorWidgets::TextWrapped(
              "Region bounds are editor-only. Placement puts their lower face "
              "on an upward surface.");
        } else if constexpr (std::is_same_v<T, NarrativeEventDefinition>) {
          text(value.id);
          auto trigger = value.trigger.index();
          if (choice("trigger-kind", trigger, triggerNames, "trigger.kind")) {
            EditorWidgetMetadata::structureChanged("trigger");
            value.trigger = newKind<0, NarrativeTrigger>(trigger);
            commit = true;
          }
          std::visit(
              [&](auto& v) {
                using V = std::decay_t<decltype(v)>;
                commit |= parameters(v, level, "trigger");
                if constexpr (std::is_same_v<V, NarrativeInteractionTrigger>) {
                  const std::array kinds{"Door", "Switch", "Radio", "Document",
                                         "Box"};
                  auto kind = static_cast<std::size_t>(v.target_kind);
                  if (choice("target-kind", kind, kinds,
                             "trigger.target_kind")) {
                    v.target_kind =
                        static_cast<NarrativeInteractionTarget>(kind);
                    commit = true;
                  }
                  switch (v.target_kind) {
                    case NarrativeInteractionTarget::Door:
                      commit |= reference("Target", v.target, level.doors,
                                          "trigger.target");
                      break;
                    case NarrativeInteractionTarget::Switch:
                      commit |=
                          reference("Target", v.target, level.light_switches,
                                    "trigger.target");
                      break;
                    case NarrativeInteractionTarget::Radio:
                      commit |=
                          reference("Target", v.target, level.household.radios,
                                    "trigger.target");
                      break;
                    case NarrativeInteractionTarget::Document:
                      commit |= reference("Target", v.target,
                                          level.household.documents,
                                          "trigger.target");
                      break;
                    case NarrativeInteractionTarget::Box:
                      commit |=
                          reference("Target", v.target, level.household.boxes,
                                    "trigger.target");
                      break;
                  }
                  auto action = static_cast<std::size_t>(v.action);
                  if (choice("action", action, actionNames, "trigger.action")) {
                    v.action = static_cast<NarrativeInteractionAction>(action);
                    commit = true;
                  }
                } else if constexpr (std::is_same_v<V,
                                                    NarrativeConditionTrigger>)
                  commit |=
                      predicates(v.predicates, level, "trigger.predicates");
              },
              value.trigger);
          auto repeat = static_cast<std::size_t>(value.repeat);
          const std::array repeats{"once", "rearm"};
          if (choice("repeat", repeat, repeats, "repeat")) {
            value.repeat = static_cast<NarrativeRepeat>(repeat);
            commit = true;
          }
          EditorWidgets::TextUnformatted("Start guards (all)");
          commit |= predicates(value.guards, level, "guards");
          bool cancel = value.cancel.has_value();
          metadata("cancellation-enabled", "cancel_enabled");
          if (EditorWidgets::Checkbox("Cancellation enabled", &cancel)) {
            EditorWidgetMetadata::structureChanged("cancel");
            if (cancel)
              value.cancel.emplace();
            else
              value.cancel.reset();
            commit = true;
          }
          if (value.cancel) {
            EditorWidgets::TextUnformatted("Cancel when all conditions match");
            commit |= predicates(*value.cancel, level, "cancel");
          }
          EditorWidgets::TextUnformatted("Ordered steps");
          for (std::size_t i = 0; i < value.steps.size(); ++i) {
            EditorWidgetMetadata::Row row("steps", i);
            EditorWidgets::PushID(static_cast<int>(i));
            const auto path = "steps." + std::to_string(i);
            EditorWidgets::Separator();
            EditorWidgets::Text("Step %zu", i + 1);
            auto kind = value.steps[i].index();
            if (choice("step-kind", kind, stepNames, path + ".kind")) {
              EditorWidgetMetadata::structureChanged("steps");
              value.steps[i] = newKind<0, NarrativeStep>(kind);
              commit = true;
            }
            std::visit(
                [&](auto& v) {
                  commit |= parameters(v, level, path);
                  if constexpr (std::is_same_v<std::decay_t<decltype(v)>,
                                               NarrativeWaitUntilStep>)
                    commit |=
                        predicates(v.predicates, level, path + ".predicates");
                },
                value.steps[i]);
            EditorWidgets::BeginDisabled(i == 0);
            const bool up = EditorWidgets::Button("Move step up");
            EditorWidgets::EndDisabled();
            EditorWidgets::SameLine();
            EditorWidgets::BeginDisabled(i + 1 == value.steps.size());
            const bool down = EditorWidgets::Button("Move step down");
            EditorWidgets::EndDisabled();
            const bool remove = EditorWidgets::Button("Remove step");
            EditorWidgets::BeginDisabled(value.steps.size() >=
                                         level_maximum_narrative_step_count);
            const bool insert = EditorWidgets::Button("Insert step before");
            EditorWidgets::EndDisabled();
            EditorWidgets::PopID();
            if (up || down || remove || insert) {
              EditorWidgetMetadata::structureChanged("steps");
              if (remove)
                value.steps.erase(value.steps.begin() + i);
              else if (insert)
                value.steps.insert(value.steps.begin() + i,
                                   NarrativeDelayStep{});
              else
                std::swap(value.steps[i], value.steps[up ? i - 1 : i + 1]);
              commit = true;
              break;
            }
          }
          EditorWidgets::BeginDisabled(value.steps.size() >=
                                       level_maximum_narrative_step_count);
          if (EditorWidgets::Button("Add step")) {
            EditorWidgetMetadata::structureChanged("steps");
            value.steps.emplace_back(NarrativeDelayStep{});
            commit = true;
          }
          EditorWidgets::EndDisabled();
          EditorWidgets::TextWrapped(
              "Silent authored preview. Use saved-file Play to execute events. "
              "ID order resolves simultaneous events.");
        }
      },
      *property_edit_.value());
  const auto error = editorNarrativeFieldError(*property_edit_.value());
  if (!error.empty())
    EditorWidgets::TextWrapped("Draft error: %s", error.c_str());
  if (!document.editError().empty())
    EditorWidgets::TextWrapped("Edit rejected: %s",
                               document.editError().c_str());
  if (commit) static_cast<void>(property_edit_.commit(document));
}
