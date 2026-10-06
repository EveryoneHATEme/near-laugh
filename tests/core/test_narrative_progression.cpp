#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

#include "core/gameplay/narrative_progression.hpp"

namespace {
NarrativeEventDefinition event(
    std::string id, std::vector<NarrativeStep> steps,
    NarrativeTrigger trigger = NarrativeSceneEntryTrigger{}) {
  return {std::move(id),         std::move(trigger), {}, {},
          NarrativeRepeat::Once, std::move(steps)};
}
NarrativeObservation observation(double time = 0) {
  NarrativeObservation value;
  value.active_time = time;
  value.feet = {-2, 0, 0};
  value.lights = {{"lamp", false}};
  value.radios = {{"radio", true}};
  value.boxes = {{"box", false}};
  value.documents = {{"note", false}};
  value.doors = {{"door", false, true, false}};
  value.actors = {{"actor", 0, NarrativeActorState::Idle}};
  value.cues = {{"cue", 0, CueStatus::Idle}};
  return value;
}
LevelNarrative definitions() {
  LevelNarrative value;
  value.facts = {{"done", false}, {"signal", false}};
  value.regions = {{"area", {}, {1, 1, 1}}};
  return value;
}
std::vector<NarrativeCommand> drain(
    NarrativeProgression& run,
    NarrativeCommandResult result = {NarrativeCommandStatus::Accepted, 0, {}}) {
  std::vector<NarrativeCommand> commands;
  while (auto command = run.nextCommand()) {
    commands.push_back(*command);
    run.resolve(result);
  }
  return commands;
}
const NarrativeRunSnapshot& named(const std::vector<NarrativeRunSnapshot>& runs,
                                  std::string_view id) {
  return *std::find_if(runs.begin(), runs.end(),
                       [&](const auto& run) { return run.event == id; });
}
}  // namespace

TEST(AcceptedInteractions, OverflowIsExplicitAndConsumptionPreservesIdentity) {
  AcceptedInteractions accepted;
  for (std::size_t i = 0; i < AcceptedInteractions::capacity; ++i)
    accepted.publish(NarrativeInteractionTarget::Box, "box",
                     NarrativeInteractionAction::BoxDrop,
                     AcceptedInteractionResult::Dropped);
  EXPECT_THROW(accepted.publish(NarrativeInteractionTarget::Box, "box",
                                NarrativeInteractionAction::BoxDrop,
                                AcceptedInteractionResult::Dropped),
               std::overflow_error);
  const auto last = accepted.pending().back().occurrence;
  accepted.consume();
  accepted.publish(NarrativeInteractionTarget::Box, "box",
                   NarrativeInteractionAction::BoxDrop,
                   AcceptedInteractionResult::Dropped);
  EXPECT_GT(accepted.pending()[0].occurrence, last);
}

TEST(NarrativeProgression,
     SpawnInsideAndInitiallyTrueConditionsDoNotInventTriggers) {
  auto d = definitions();
  d.events = {event("region", {NarrativeSetFactStep{"done", true}},
                    NarrativeRegionEntryTrigger{"area"}),
              event("condition", {NarrativeSetFactStep{"signal", true}},
                    NarrativeConditionTrigger{
                        {NarrativeRadioPredicate{"radio", true}}}),
              event("startup", {NarrativeSetLightStep{"lamp", true}})};
  auto state = observation();
  state.feet = {};
  NarrativeProgression run(d, state);
  EXPECT_TRUE(run.beginBoundary(state).empty());
  const auto commands = drain(run);
  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands[0].event, "startup");
  EXPECT_FALSE(run.facts().at("done"));
  EXPECT_FALSE(run.facts().at("signal"));
  run.samplePosition({2, 0, 0});
  run.samplePosition({0, 0, 0});
  run.samplePosition({2, 0, 0});
  EXPECT_TRUE(run.beginBoundary(state).empty());
  EXPECT_TRUE(drain(run).empty());
  EXPECT_TRUE(run.facts().at("done"));
  EXPECT_FALSE(run.facts().at("signal"));
}

TEST(NarrativeProgression,
     HalfOpenSamplesDoNotSweepThinRegionsAndGuardsUseCurrentState) {
  auto d = definitions();
  d.regions[0].half_extent.x = .01F;
  d.events = {event("entry", {NarrativeSetFactStep{"done", true}},
                    NarrativeRegionEntryTrigger{"area"})};
  d.events[0].guards = {NarrativeRegionPredicate{"area", false}};
  NarrativeProgression run(d, observation());
  run.samplePosition({2, 0, 0});  // Entirely crossed between observations.
  (void)run.beginBoundary(observation());
  drain(run);
  EXPECT_FALSE(run.facts().at("done"));
  run.samplePosition({.01F, 0, 0});  // Upper face excluded.
  (void)run.beginBoundary(observation());
  drain(run);
  EXPECT_FALSE(run.facts().at("done"));
  run.samplePosition({-.01F, 0, 0});  // Lower face included.
  run.samplePosition({2, 0, 0});
  (void)run.beginBoundary(observation());
  drain(run);
  EXPECT_TRUE(run.facts().at("done"));
}

TEST(NarrativeProgression, EveryPredicateUsesExplicitAcceptedState) {
  const std::vector<NarrativePredicate> predicates{
      NarrativeFactPredicate{"signal", true},
      NarrativeRegionPredicate{"area", true},
      NarrativeLightPredicate{"lamp", true},
      NarrativeDoorEndpointPredicate{"door", true},
      NarrativeDoorLockedPredicate{"door", true},
      NarrativeRadioPredicate{"radio", false},
      NarrativeBoxPredicate{"box", true},
      NarrativeDocumentPredicate{"note", true},
      NarrativeActorPredicate{"actor", NarrativeActorState::Blocked},
      NarrativeElapsedPredicate{2}};
  for (std::size_t selected = 0; selected < predicates.size(); ++selected) {
    auto d = definitions();
    d.events = {event("wait", {NarrativeWaitUntilStep{{predicates[selected]}},
                               NarrativeSetFactStep{"done", true}})};
    if (selected == 0) {
      d.events[0].steps.insert(d.events[0].steps.begin(),
                               NarrativeSetFactStep{"signal", true});
    }
    auto state = observation();
    NarrativeProgression run(d, state);
    (void)run.beginBoundary(state);
    drain(run);
    EXPECT_FALSE(run.facts().at("done")) << selected;
    state.active_time = 2;
    run.samplePosition({0, 0, 0});
    state.lights[0].value = true;
    state.doors[0] = {"door", true, false, true};
    state.radios[0].value = false;
    state.boxes[0].value = true;
    state.documents[0].value = true;
    state.actors[0].state = NarrativeActorState::Blocked;
    (void)run.beginBoundary(state);
    drain(run);
    EXPECT_TRUE(run.facts().at("done")) << selected;
  }
}

TEST(NarrativeProgression,
     WaitDiagnosticsIdentifyLaterUnmetPredicateWithoutSpam) {
  auto d = definitions();
  d.events = {event(
      "wait",
      {NarrativeWaitUntilStep{{NarrativeRadioPredicate{"radio", true},
                               NarrativeDoorEndpointPredicate{"door", true}}},
       NarrativeSetFactStep{"done", true}})};
  NarrativeProgression run(d, observation());
  (void)run.beginBoundary(observation());
  drain(run);
  ASSERT_EQ(run.runs()[0].target, "door");
  EXPECT_EQ(run.runs()[0].reason, "condition_unmet:door_endpoint");
  const auto count = run.trace().size();
  (void)run.beginBoundary(observation(1));
  drain(run);
  EXPECT_EQ(run.trace().size(), count);
  auto state = observation(2);
  state.doors[0].closed = false;  // Moving is neither endpoint.
  (void)run.beginBoundary(state);
  drain(run);
  EXPECT_FALSE(run.facts().at("done"));
  state.doors[0].open = true;
  (void)run.beginBoundary(state);
  drain(run);
  EXPECT_TRUE(run.facts().at("done"));
}

TEST(NarrativeProgression,
     EventTerminalIsFalseInitiallyAndChangesOnNextBoundary) {
  auto d = definitions();
  d.events = {event("a", {NarrativeSetFactStep{"signal", true}}),
              event("b", {NarrativeSetFactStep{"done", true}},
                    NarrativeConditionTrigger{{NarrativeEventPredicate{
                        "a", NarrativeEventTerminalState::Completed}}})};
  NarrativeProgression run(d, observation());
  (void)run.beginBoundary(observation());
  drain(run);
  EXPECT_FALSE(run.facts().at("done"));
  (void)run.beginBoundary(observation());
  drain(run);
  EXPECT_TRUE(run.facts().at("done"));
}

TEST(NarrativeProgression,
     FrozenFactsAndStableIdsResolveConflictsIndependentlyOfStorage) {
  auto d = definitions();
  d.events = {event("z", {NarrativeSetFactStep{"signal", true},
                          NarrativeSetLightStep{"lamp", true}}),
              event("a", {NarrativeSetFactStep{"signal", false},
                          NarrativeSetLightStep{"lamp", false}}),
              event("b", {NarrativeSetFactStep{"done", true}},
                    NarrativeConditionTrigger{
                        {NarrativeFactPredicate{"signal", true}}})};
  for (int permutation = 0; permutation < 2; ++permutation) {
    NarrativeProgression run(d, observation());
    (void)run.beginBoundary(observation());
    const auto commands = drain(run);
    ASSERT_EQ(commands.size(), 2U);
    EXPECT_EQ(commands[0].event, "a");
    EXPECT_EQ(commands[1].event, "z");
    EXPECT_TRUE(run.facts().at("signal"));
    EXPECT_FALSE(run.facts().at("done"));
    (void)run.beginBoundary(observation());
    drain(run);
    EXPECT_TRUE(run.facts().at("done"));
    std::reverse(d.events.begin(), d.events.end());
  }
}

TEST(NarrativeProgression,
     CancellationDominatesCompletionAndNewStartWithoutRollback) {
  for (const bool owned : {false, true}) {
    auto d = definitions();
    d.events = {
        event("sequence", {NarrativeSetFactStep{"signal", true},
                           owned ? NarrativeStep{NarrativePlayCueStep{"cue"}}
                                 : NarrativeStep{NarrativeDelayStep{2}},
                           NarrativeSetFactStep{"done", true}})};
    d.events[0].cancel = std::vector<NarrativePredicate>{
        NarrativeRadioPredicate{"radio", false}};
    auto state = observation();
    NarrativeProgression run(d, state);
    (void)run.beginBoundary(state);
    drain(run, {NarrativeCommandStatus::Started, 7, {}});
    state.active_time = 2;
    state.cues[0] = {"cue", 7, CueStatus::Completed};
    state.radios[0].value = false;
    const auto canceled = run.beginBoundary(state);
    EXPECT_EQ(canceled.size(), owned ? 1U : 0U);
    EXPECT_TRUE(drain(run).empty());
    EXPECT_EQ(run.runs()[0].status, NarrativeRunStatus::Canceled);
    EXPECT_TRUE(run.facts().at("signal"));
    EXPECT_FALSE(run.facts().at("done"));
    state.radios[0].value = true;
    (void)run.beginBoundary(state);
    drain(run);
    EXPECT_EQ(run.runs()[0].run, 1U);
  }
}

TEST(NarrativeProgression,
     RearmRequiresFalseAfterTerminalAndConsumesActiveTriggers) {
  auto d = definitions();
  d.events = {event(
      "repeat", {NarrativeDelayStep{1}},
      NarrativeConditionTrigger{{NarrativeLightPredicate{"lamp", true}}})};
  d.events[0].repeat = NarrativeRepeat::Rearm;
  auto state = observation();
  NarrativeProgression run(d, state);
  (void)run.beginBoundary(state);
  drain(run);
  state.lights[0].value = true;
  (void)run.beginBoundary(state);
  drain(run);
  EXPECT_EQ(run.runs()[0].run, 1U);
  state.lights[0].value = false;
  (void)run.beginBoundary(state);
  drain(run);
  state.lights[0].value = true;
  state.active_time = 1;
  (void)run.beginBoundary(state);
  drain(run);
  EXPECT_EQ(run.runs()[0].status, NarrativeRunStatus::Completed);
  for (int i = 0; i < 3; ++i) {
    (void)run.beginBoundary(state);
    drain(run);
  }
  EXPECT_EQ(run.runs()[0].run, 1U);
  state.lights[0].value = false;
  (void)run.beginBoundary(state);
  drain(run);
  state.lights[0].value = true;
  (void)run.beginBoundary(state);
  drain(run);
  EXPECT_EQ(run.runs()[0].run, 2U);
}

TEST(NarrativeProgression,
     GuardRejectedInteractionIsConsumedAndLaterOccurrencesMayRearm) {
  auto d = definitions();
  d.events = {event(
      "repeat", {NarrativeSetLightStep{"lamp", true}},
      NarrativeInteractionTrigger{NarrativeInteractionTarget::Box, "box",
                                  NarrativeInteractionAction::BoxPickup})};
  d.events[0].repeat = NarrativeRepeat::Rearm;
  d.events[0].guards = {NarrativeRadioPredicate{"radio", false}};
  auto state = observation();
  NarrativeProgression run(d, state);
  AcceptedInteractions actions;
  actions.publish(NarrativeInteractionTarget::Box, "box",
                  NarrativeInteractionAction::BoxPickup,
                  AcceptedInteractionResult::PickedUp);
  (void)run.beginBoundary(state, actions.pending());
  EXPECT_TRUE(drain(run).empty());
  state.radios[0].value = false;
  (void)run.beginBoundary(state, actions.pending());
  EXPECT_TRUE(drain(run).empty());
  actions.consume();
  actions.publish(NarrativeInteractionTarget::Box, "box",
                  NarrativeInteractionAction::BoxPickup,
                  AcceptedInteractionResult::PickedUp);
  (void)run.beginBoundary(state, actions.pending());
  EXPECT_EQ(drain(run).size(), 1U);
  (void)run.beginBoundary(state, actions.pending());
  EXPECT_TRUE(drain(run).empty());
  actions.consume();
  actions.publish(NarrativeInteractionTarget::Box, "box",
                  NarrativeInteractionAction::BoxPickup,
                  AcceptedInteractionResult::PickedUp);
  (void)run.beginBoundary(state, actions.pending());
  EXPECT_EQ(drain(run).size(), 1U);
  EXPECT_EQ(run.runs()[0].run, 2U);
}

TEST(NarrativeProgression, DelaysStartWhenReachedAndNeverBackdateNewWork) {
  auto d = definitions();
  d.events = {
      event("sequence", {NarrativeDelayStep{2}, NarrativeDelayStep{3},
                         NarrativeDelayStep{0}, NarrativePlayCueStep{"cue"}})};
  NarrativeProgression run(d, observation());
  (void)run.beginBoundary(observation());
  EXPECT_TRUE(drain(run).empty());
  (void)run.beginBoundary(observation(100));
  EXPECT_TRUE(drain(run).empty());
  (void)run.beginBoundary(observation(102.9));
  EXPECT_TRUE(drain(run).empty());
  (void)run.beginBoundary(observation(103));
  const auto commands = drain(run, {NarrativeCommandStatus::Started, 1, {}});
  ASSERT_EQ(commands.size(), 1U);
  EXPECT_EQ(commands[0].step, 3U);
  EXPECT_DOUBLE_EQ(run.trace().back().active_time, 103);
  EXPECT_THROW((void)run.beginBoundary(observation(102)),
               std::invalid_argument);
}

TEST(NarrativeProgression,
     BusyRetriesOncePerBoundaryAndUnchangedWaitsDoNotFloodTrace) {
  auto d = definitions();
  d.events = {event("sequence", {NarrativePlayCueStep{"cue"},
                                 NarrativeSetFactStep{"done", true}})};
  NarrativeProgression run(d, observation());
  (void)run.beginBoundary(observation());
  EXPECT_EQ(drain(run, {NarrativeCommandStatus::Busy, 0, "foreign"}).size(),
            1U);
  const auto count = run.trace().size();
  for (int i = 0; i < 100; ++i) {
    (void)run.beginBoundary(observation());
    EXPECT_EQ(drain(run, {NarrativeCommandStatus::Busy, 0, "foreign"}).size(),
              1U);
  }
  EXPECT_EQ(run.trace().size(), count);
  EXPECT_FALSE(run.runs()[0].owned);
  EXPECT_FALSE(run.facts().at("done"));
  EXPECT_TRUE(run.close().empty());
}

TEST(NarrativeProgression, CompletedCueAndRouteReuseSucceedsInBothIdOrders) {
  for (const bool cue : {false, true})
    for (const bool contender_first : {false, true}) {
      auto d = definitions();
      const auto step =
          cue ? NarrativeStep{NarrativePlayCueStep{"cue"}}
              : NarrativeStep{NarrativeRunRouteStep{"actor", "route"}};
      const std::string owner = contender_first ? "z-owner" : "a-owner";
      const std::string contender = contender_first ? "a-next" : "z-next";
      d.events = {
          event(owner, {step, NarrativeSetFactStep{"done", true}}),
          event(contender, {step},
                NarrativeConditionTrigger{{NarrativeElapsedPredicate{1}}})};
      auto state = observation();
      NarrativeProgression run(d, state);
      (void)run.beginBoundary(state);
      drain(run, {NarrativeCommandStatus::Started, 1, {}});
      state.active_time = 1;
      state.cues[0] = {"cue", 1, CueStatus::Completed};
      state.actors[0] = {"actor", 1, NarrativeActorState::Completed};
      EXPECT_TRUE(run.beginBoundary(state).empty());
      const auto commands =
          drain(run, {NarrativeCommandStatus::Started, 2, {}});
      ASSERT_EQ(commands.size(), 1U);
      EXPECT_EQ(commands[0].event, contender);
      EXPECT_TRUE(run.facts().at("done"));
      EXPECT_EQ(named(run.runs(), owner).status, NarrativeRunStatus::Completed);
      ASSERT_TRUE(named(run.runs(), contender).owned);
      EXPECT_EQ(named(run.runs(), contender).owned->instance, 2U);
      const auto stops = run.close();
      ASSERT_EQ(stops.size(), 1U);
      EXPECT_EQ(stops[0].instance, 2U);
    }
}

TEST(NarrativeProgression,
     MissingReplacedAndExternallyCanceledWorkFailWithoutAdoption) {
  for (const bool cue : {false, true})
    for (const bool replaced : {false, true}) {
      auto d = definitions();
      const auto step =
          cue ? NarrativeStep{NarrativePlayCueStep{"cue"}}
              : NarrativeStep{NarrativeRunRouteStep{"actor", "route"}};
      d.events = {
          event("sequence", {step, NarrativeSetFactStep{"done", true}})};
      auto state = observation();
      NarrativeProgression run(d, state);
      (void)run.beginBoundary(state);
      drain(run, {NarrativeCommandStatus::Started, 1, {}});
      state.cues[0] = {
          "cue", replaced ? 2U : 1U,
          replaced ? CueStatus::Playing : CueStatus::Cancelled};
      state.actors[0] = {"actor", replaced ? 2U : 1U,
                         replaced ? NarrativeActorState::Walking
                                  : NarrativeActorState::Canceled};
      const auto stops = run.beginBoundary(state);
      ASSERT_EQ(stops.size(), 1U);
      EXPECT_EQ(stops[0].instance, 1U);
      EXPECT_TRUE(drain(run).empty());
      EXPECT_EQ(run.runs()[0].status, NarrativeRunStatus::Failed);
      EXPECT_EQ(run.runs()[0].reason,
                replaced ? "ownership_lost" : "owned_action_canceled");
      EXPECT_FALSE(run.facts().at("done"));
    }
}

TEST(NarrativeProgression,
     RefusedCommandFailsAndFreshLaunchRestoresDefinitions) {
  auto d = definitions();
  d.events = {event("sequence", {NarrativeSetFactStep{"signal", true},
                                 NarrativeSetDoorLockedStep{"door", true},
                                 NarrativeSetFactStep{"done", true}})};
  const auto original = d;
  NarrativeProgression run(d, observation());
  (void)run.beginBoundary(observation());
  EXPECT_EQ(drain(run, {NarrativeCommandStatus::Refused, 0, "door_not_closed"})
                .size(),
            1U);
  EXPECT_TRUE(run.facts().at("signal"));
  EXPECT_FALSE(run.facts().at("done"));
  EXPECT_EQ(run.runs()[0].status, NarrativeRunStatus::Failed);
  EXPECT_EQ(run.runs()[0].target, "door");
  EXPECT_TRUE(run.close().empty());
  EXPECT_FALSE(run.nextCommand());
  NarrativeProgression fresh(d, observation());
  EXPECT_FALSE(fresh.facts().at("signal"));
  EXPECT_TRUE(fresh.trace().empty());
  EXPECT_EQ(d, original);
}

TEST(NarrativeProgression,
     BoundedTraceRetainsRecentTransitionsAndOverflowFailsExplicitly) {
  auto d = definitions();
  for (std::size_t i = 0; i < level_maximum_narrative_event_count; ++i) {
    auto value = event("event-" + std::to_string(i), {});
    value.steps.assign(level_maximum_narrative_step_count,
                       NarrativeSetFactStep{"done", true});
    d.events.push_back(value);
  }
  NarrativeProgression run(d, observation());
  (void)run.beginBoundary(observation());
  drain(run);
  EXPECT_EQ(run.trace().size(), 256U);
  EXPECT_GT(run.droppedTraceRecords(), 0U);
  EXPECT_EQ(run.trace().back().event.status, NarrativeRunStatus::Completed);
  d.events.resize(1);
  d.events[0].trigger = NarrativeRegionEntryTrigger{"area"};
  NarrativeProgression overflow(d, observation());
  for (int i = 0; i < 193; ++i) {
    overflow.samplePosition({0, 0, 0});
    overflow.samplePosition({2, 0, 0});
  }
  ASSERT_TRUE(overflow.observationOverflow());
  (void)overflow.beginBoundary(observation());
  drain(overflow);
  // Reported without fabricating a failed run for an event that never ran.
  EXPECT_EQ(overflow.runs()[0].status, NarrativeRunStatus::Dormant);
  EXPECT_EQ(overflow.runs()[0].run, 0U);
  EXPECT_EQ(overflow.runs()[0].reason, "observation_overflow");
  EXPECT_FALSE(overflow.facts().at("done"));
  // The overflow affects one boundary; a later entry still starts the event.
  overflow.samplePosition({0, 0, 0});
  (void)overflow.beginBoundary(observation());
  drain(overflow);
  EXPECT_TRUE(overflow.observationOverflow());
  EXPECT_EQ(overflow.runs()[0].status, NarrativeRunStatus::Completed);
  EXPECT_TRUE(overflow.facts().at("done"));
}
