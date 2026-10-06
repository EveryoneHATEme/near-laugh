#include "core/gameplay/narrative_progression.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "core/world/narrative.hpp"

namespace {
template <class... F>
struct Visit : F... {
  using F::operator()...;
};
template <class T>
const T* find(const std::vector<T>& values, std::string_view id) {
  const auto value =
      std::find_if(values.begin(), values.end(),
                   [&](const auto& candidate) { return candidate.id == id; });
  return value == values.end() ? nullptr : &*value;
}
bool boolean(const std::vector<NarrativeBooleanObservation>& values,
             std::string_view id, bool expected) {
  const auto* value = find(values, id);
  return value && value->value == expected;
}
NarrativeRunStatus terminalStatus(NarrativeEventTerminalState state) {
  switch (state) {
    case NarrativeEventTerminalState::Completed:
      return NarrativeRunStatus::Completed;
    case NarrativeEventTerminalState::Canceled:
      return NarrativeRunStatus::Canceled;
    case NarrativeEventTerminalState::Failed:
      return NarrativeRunStatus::Failed;
  }
  throw std::invalid_argument("Unknown narrative terminal state");
}
// Each step or predicate record carries at most one referenced definition ID.
template <class Record>
std::string referencedId(const Record& value) {
  if constexpr (requires { value.fact; })
    return value.fact;
  else if constexpr (requires { value.region; })
    return value.region;
  else if constexpr (requires { value.light; })
    return value.light;
  else if constexpr (requires { value.door; })
    return value.door;
  else if constexpr (requires { value.radio; })
    return value.radio;
  else if constexpr (requires { value.box; })
    return value.box;
  else if constexpr (requires { value.document; })
    return value.document;
  else if constexpr (requires { value.source; })
    return value.source;
  else if constexpr (requires { value.actor; })
    return value.actor;
  else if constexpr (requires { value.event; })
    return value.event;
  else
    return {};
}
std::string target(const NarrativeStep& step) {
  return std::visit([](const auto& value) { return referencedId(value); },
                    step);
}
}  // namespace

NarrativeProgression::NarrativeProgression(const LevelNarrative& definitions,
                                           const NarrativeObservation& initial)
    : definitions_(definitions),
      snapshot_(initial),
      runs_(definitions.events.size()),
      order_(definitions.events.size()),
      origin_(initial.active_time),
      last_time_(origin_) {
  if (!std::isfinite(origin_) || origin_ < 0 ||
      definitions.events.size() > level_maximum_narrative_event_count ||
      definitions.facts.size() > level_maximum_narrative_fact_count ||
      definitions.regions.size() > level_maximum_narrative_region_count)
    throw std::invalid_argument(
        "Narrative requires validated definitions and active time");
  for (const auto& fact : definitions.facts)
    facts_.emplace(fact.id, fact.initial_value);
  frozen_facts_ = facts_;
  for (const auto& region : definitions.regions)
    occupancy_[region.id] = narrativeRegionContains(region, initial.feet);
  std::iota(order_.begin(), order_.end(), 0);
  std::sort(order_.begin(), order_.end(), [&](auto a, auto b) {
    return definitions.events[a].id < definitions.events[b].id;
  });
  for (std::size_t i = 0; i < runs_.size(); ++i) {
    const auto& event = definitions.events[i];
    if (event.steps.empty() ||
        event.steps.size() > level_maximum_narrative_step_count)
      throw std::invalid_argument(
          "Narrative event requires bounded nonempty steps");
    runs_[i].view.event = event.id;
    event_indices_.emplace(event.id, i);
  }
  for (std::size_t i = 0; i < runs_.size(); ++i)
    if (const auto* trigger = std::get_if<NarrativeConditionTrigger>(
            &definitions.events[i].trigger))
      runs_[i].previous_condition = condition(trigger->predicates);
  entries_.reserve(level_maximum_narrative_region_count *
                   FixedStepAccumulator::maximum_steps_per_sample);
}

bool NarrativeProgression::condition(
    const std::vector<NarrativePredicate>& predicates) const {
  return std::all_of(predicates.begin(), predicates.end(),
                     [&](const auto& predicate) { return holds(predicate); });
}

bool NarrativeProgression::holds(const NarrativePredicate& predicate) const {
  return std::visit(
      Visit{[&](const NarrativeFactPredicate& p) {
              const auto value = frozen_facts_.find(p.fact);
              return value != frozen_facts_.end() && value->second == p.value;
            },
            [&](const NarrativeRegionPredicate& p) {
              const auto value = occupancy_.find(p.region);
              return value != occupancy_.end() && value->second == p.inside;
            },
            [&](const NarrativeLightPredicate& p) {
              return boolean(snapshot_.lights, p.light, p.enabled);
            },
            [&](const NarrativeDoorEndpointPredicate& p) {
              const auto* door = find(snapshot_.doors, p.door);
              return door && (p.open ? door->open : door->closed);
            },
            [&](const NarrativeDoorLockedPredicate& p) {
              const auto* door = find(snapshot_.doors, p.door);
              return door && door->locked == p.locked;
            },
            [&](const NarrativeRadioPredicate& p) {
              return boolean(snapshot_.radios, p.radio, p.enabled);
            },
            [&](const NarrativeBoxPredicate& p) {
              return boolean(snapshot_.boxes, p.box, p.held);
            },
            [&](const NarrativeDocumentPredicate& p) {
              return boolean(snapshot_.documents, p.document, p.open);
            },
            [&](const NarrativeActorPredicate& p) {
              const auto* actor = find(snapshot_.actors, p.actor);
              return actor && actor->state == p.state;
            },
            [&](const NarrativeEventPredicate& p) {
              const auto value = event_indices_.find(p.event);
              return value != event_indices_.end() &&
                     runs_[value->second].frozen_status ==
                         terminalStatus(p.state);
            },
            [&](const NarrativeElapsedPredicate& p) {
              return snapshot_.active_time - origin_ >= p.seconds;
            }},
      predicate);
}

void NarrativeProgression::samplePosition(WorldPosition feet) {
  if (closed_) return;
  if (boundary_)
    throw std::logic_error(
        "Cannot change narrative observations during dispatch");
  if (!std::isfinite(feet.x) || !std::isfinite(feet.y) ||
      !std::isfinite(feet.z))
    throw std::invalid_argument("Narrative feet must be finite");
  for (const auto& region : definitions_.regions) {
    const bool current = narrativeRegionContains(region, feet);
    if (current && !occupancy_.at(region.id)) {
      if (entries_.size() == level_maximum_narrative_region_count *
                                 FixedStepAccumulator::maximum_steps_per_sample)
        overflow_ = overflow_pending_ = true;
      else
        entries_.push_back(region.id);
    }
    occupancy_[region.id] = current;
  }
}

void NarrativeProgression::transition(std::size_t event, std::string reason,
                                      std::string destination) {
  auto& view = runs_[event].view;
  if (view.reason == reason && view.target == destination) return;
  view.reason = std::move(reason);
  view.target = std::move(destination);
  if (trace_.size() == 256) {
    trace_.pop_front();
    ++dropped_;
  }
  trace_.push_back({view, snapshot_.active_time - origin_});
}
void NarrativeProgression::terminal(std::size_t event,
                                    NarrativeRunStatus status,
                                    std::string reason) {
  auto& run = runs_[event];
  run.view.status = status;
  transition(event, std::move(reason), run.view.target);
  run.view.owned.reset();
  run.deadline.reset();
}
void NarrativeProgression::advanceStep(std::size_t event) {
  auto& run = runs_[event];
  transition(event, "step_completed",
             target(definitions_.events[event].steps[run.view.step]));
  ++run.view.step;
  run.view.owned.reset();
  run.deadline.reset();
  run.owned_complete = false;
  // Distinct adjacent steps remain distinct trace transitions.
  run.view.reason.clear();
}

std::vector<NarrativeOwnedAction> NarrativeProgression::beginBoundary(
    NarrativeObservation observation,
    std::span<const AcceptedInteraction> interactions) {
  if (closed_) return {};
  if (boundary_ || pending_)
    throw std::logic_error("Previous narrative boundary is unfinished");
  if (!std::isfinite(observation.active_time) ||
      observation.active_time < last_time_)
    throw std::invalid_argument(
        "Narrative active time must be finite and monotonic");
  snapshot_ = std::move(observation);
  last_time_ = snapshot_.active_time;
  // Copy facts only after a step changed one since the previous snapshot.
  if (std::exchange(facts_changed_, false)) frozen_facts_ = facts_;
  for (auto& run : runs_) run.frozen_status = run.view.status;
  // Dropped occurrences fail live runs and block starts for this boundary
  // only; terminal history of other events is retained.
  const bool overflow = std::exchange(overflow_pending_, false);
  std::vector<NarrativeOwnedAction> cancellations;
  boundary_ = true;
  cursor_ = 0;
  // All cancellations and existing completions are classified before new
  // starts.
  for (const auto i : order_) {
    auto& run = runs_[i];
    const auto& event = definitions_.events[i];
    const bool cancel = event.cancel && condition(*event.cancel);
    const bool was_active = run.view.status == NarrativeRunStatus::Running;
    if (was_active && (overflow || cancel)) {
      if (run.view.owned) cancellations.push_back(*run.view.owned);
      terminal(
          i,
          overflow ? NarrativeRunStatus::Failed : NarrativeRunStatus::Canceled,
          overflow ? "observation_overflow" : "cancellation_condition");
    } else if (overflow) {
      transition(i, "observation_overflow", run.view.target);
    } else if (was_active && run.view.owned) {
      const auto& owned = *run.view.owned;
      bool match = false, failed = false;
      if (owned.kind == NarrativeOwnedKind::Cue) {
        const auto* cue = find(snapshot_.cues, owned.target);
        match = cue && cue->instance == owned.instance;
        run.owned_complete =
            match && cue->state == CueStatus::Completed;
        failed = match && (cue->state == CueStatus::Cancelled ||
                           cue->state == CueStatus::Idle);
      } else {
        const auto* actor = find(snapshot_.actors, owned.target);
        match = actor && actor->instance == owned.instance;
        run.owned_complete =
            match && actor->state == NarrativeActorState::Completed;
        failed = match && (actor->state == NarrativeActorState::Canceled ||
                           actor->state == NarrativeActorState::Idle);
      }
      if (!match || failed) {
        cancellations.push_back(owned);
        terminal(i, NarrativeRunStatus::Failed,
                 !match ? "ownership_lost" : "owned_action_canceled");
      }
    }
    const bool triggered = std::visit(
        Visit{[&](const NarrativeSceneEntryTrigger&) { return first_; },
              [&](const NarrativeRegionEntryTrigger& trigger) {
                return std::find(entries_.begin(), entries_.end(),
                                 trigger.region) != entries_.end();
              },
              [&](const NarrativeInteractionTrigger& trigger) {
                return std::any_of(
                    interactions.begin(), interactions.end(),
                    [&](const auto& action) {
                      return action.occurrence > consumed_interaction_ &&
                             action.target_kind == trigger.target_kind &&
                             action.target == trigger.target &&
                             action.action == trigger.action;
                    });
              },
              [&](const NarrativeConditionTrigger& trigger) {
                const bool current = condition(trigger.predicates);
                if (!was_active && !current) run.condition_rearmed = true;
                const bool edge =
                    current && !run.previous_condition && run.condition_rearmed;
                run.previous_condition = current;
                return edge;
              }},
        event.trigger);
    run.eligible =
        !overflow && !was_active && !cancel && triggered &&
        (event.repeat == NarrativeRepeat::Rearm || run.view.run == 0) &&
        condition(event.guards);
  }
  for (const auto& action : interactions)
    consumed_interaction_ = std::max(consumed_interaction_, action.occurrence);
  entries_.clear();
  first_ = false;
  return cancellations;
}

std::optional<NarrativeCommand> NarrativeProgression::nextCommand() {
  if (closed_ || !boundary_) return {};
  if (pending_) throw std::logic_error("Narrative command result is missing");
  while (cursor_ < order_.size()) {
    const auto i = order_[cursor_];
    auto& run = runs_[i];
    const auto& event = definitions_.events[i];
    if (run.eligible) {
      run.eligible = false;
      run.view.status = NarrativeRunStatus::Running;
      ++run.view.run;
      run.view.step = 0;
      run.condition_rearmed = false;
      transition(i, "started");
    }
    if (run.view.status != NarrativeRunStatus::Running) {
      ++cursor_;
      continue;
    }
    if (run.view.owned) {
      if (!run.owned_complete) {
        std::string reason = "waiting_cue";
        if (run.view.owned->kind == NarrativeOwnedKind::Route) {
          const auto* actor = find(snapshot_.actors, run.view.owned->target);
          reason = actor && actor->state == NarrativeActorState::Blocked
                       ? "route_blocked"
                       : "waiting_route";
        }
        transition(i, reason, run.view.owned->target);
        ++cursor_;
        continue;
      }
      advanceStep(i);
    }
    if (run.view.step == event.steps.size()) {
      terminal(i, NarrativeRunStatus::Completed, "completed");
      ++cursor_;
      continue;
    }
    const auto& step = event.steps[run.view.step];
    if (const auto* fact = std::get_if<NarrativeSetFactStep>(&step)) {
      auto& value = facts_.at(fact->fact);
      facts_changed_ |= value != fact->value;
      value = fact->value;
      advanceStep(i);
    } else if (const auto* delay = std::get_if<NarrativeDelayStep>(&step)) {
      if (!run.deadline) run.deadline = snapshot_.active_time + delay->seconds;
      if (snapshot_.active_time >= *run.deadline)
        advanceStep(i);
      else {
        transition(i, "delay");
        ++cursor_;
      }
    } else if (const auto* wait = std::get_if<NarrativeWaitUntilStep>(&step)) {
      if (condition(wait->predicates))
        advanceStep(i);
      else {
        for (const auto& predicate : wait->predicates) {
          if (holds(predicate)) continue;
          const auto destination = std::visit(
              [](const auto& value) -> std::string {
                if constexpr (requires { value.seconds; })
                  return std::to_string(value.seconds);
                else
                  return referencedId(value);
              },
              predicate);
          transition(i,
                     std::string("condition_unmet:")
                         .append(narrative_predicate_kind_names[predicate.index()]),
                     destination);
          break;
        }
        ++cursor_;
      }
    } else {
      run.view.target = target(step);
      pending_ = i;
      return NarrativeCommand{event.id, run.view.run, run.view.step, step};
    }
  }
  boundary_ = false;
  return {};
}

void NarrativeProgression::resolve(const NarrativeCommandResult& result) {
  if (!pending_) throw std::logic_error("No narrative command awaits a result");
  const auto i = *pending_;
  pending_.reset();
  auto& run = runs_[i];
  const auto& step = definitions_.events[i].steps[run.view.step];
  const bool cue = std::holds_alternative<NarrativePlayCueStep>(step);
  const bool route = std::holds_alternative<NarrativeRunRouteStep>(step);
  if (result.status == NarrativeCommandStatus::Busy) {
    if (!cue && !route)
      terminal(i, NarrativeRunStatus::Failed, "unexpected_busy_state_command");
    else
      transition(i, result.reason.empty() ? "resource_busy" : result.reason,
                 target(step));
    ++cursor_;
  } else if (result.status == NarrativeCommandStatus::Refused) {
    terminal(i, NarrativeRunStatus::Failed,
             result.reason.empty() ? "command_refused" : result.reason);
    ++cursor_;
  } else if (cue || route) {
    if (result.status != NarrativeCommandStatus::Started || !result.instance) {
      terminal(i, NarrativeRunStatus::Failed, "new_instance_required");
    } else {
      run.view.owned = NarrativeOwnedAction{
          cue ? NarrativeOwnedKind::Cue : NarrativeOwnedKind::Route,
          target(step), result.instance};
      transition(i, cue ? "waiting_cue" : "waiting_route", target(step));
    }
    ++cursor_;
  } else if (result.status == NarrativeCommandStatus::Accepted) {
    advanceStep(i);
  } else {
    terminal(i, NarrativeRunStatus::Failed, "unexpected_instance");
    ++cursor_;
  }
}

std::vector<NarrativeOwnedAction> NarrativeProgression::close() {
  std::vector<NarrativeOwnedAction> cancellations;
  if (closed_) return cancellations;
  closed_ = true;
  pending_.reset();
  for (const auto i : order_) {
    if (runs_[i].view.status != NarrativeRunStatus::Running) continue;
    if (runs_[i].view.owned) cancellations.push_back(*runs_[i].view.owned);
    terminal(i, NarrativeRunStatus::Canceled, "scene_closed");
  }
  return cancellations;
}
std::vector<NarrativeRunSnapshot> NarrativeProgression::runs() const {
  std::vector<NarrativeRunSnapshot> result;
  for (const auto i : order_) result.push_back(runs_[i].view);
  return result;
}
