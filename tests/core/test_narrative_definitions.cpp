#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

#include "core/world/narrative.hpp"

namespace {
LevelDocument definitions() {
  LevelDocument d;
  d.environment_light.point_lights = {{{0, 2, 0}, {1, 1, 1}, 1, 5, "light"}};
  d.light_switches = {{{0, 1, 0}, 0, "light", "switch"}};
  d.doors = {{"door"}};
  d.household.boxes = {{"box"}};
  d.household.documents = {{"document"}};
  d.household.radios = {{"radio", "prop", "radio-source", true}};
  d.characters.actors = {{"actor", "test-mannequin", "mark", {}, 1, {}, {}}};
  d.characters.routes = {{"route", "actor", {"mark"}, {}}};
  d.audio.cues = {{"cue", "invitation", "invitation", AudioCueKind::Essential,
                   false, true}};
  d.audio.sources = {{"source", "cue"}};
  d.narrative.facts = {{"fact", true}};
  d.narrative.regions = {{"region", {1, 2, 3}, {1, 2, 3}}};
  d.narrative.events = {{"previous",
                         {},
                         {},
                         {},
                         NarrativeRepeat::Once,
                         {NarrativeSetFactStep{"fact", false}}},
                        {"event",
                         NarrativeRegionEntryTrigger{"region"},
                         {},
                         {},
                         NarrativeRepeat::Rearm,
                         {NarrativeDelayStep{0}}}};
  return d;
}
bool field(const std::vector<LevelDiagnostic>& diagnostics,
           std::string_view path) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [&](const auto& d) {
                       return d.document_path.find(path) != std::string::npos;
                     });
}
}  // namespace

TEST(NarrativeDefinitions, EmptyAndAllKindsAreDeviceIndependent) {
  EXPECT_TRUE(validateNarrativeDefinitions(LevelDocument{}).empty());
  auto d = definitions();
  auto& e = d.narrative.events.back();
  e.guards = {NarrativeFactPredicate{"fact", true},
              NarrativeRegionPredicate{"region", false},
              NarrativeLightPredicate{"light", true},
              NarrativeDoorEndpointPredicate{"door", false},
              NarrativeDoorLockedPredicate{"door", true},
              NarrativeRadioPredicate{"radio", false},
              NarrativeBoxPredicate{"box", true},
              NarrativeDocumentPredicate{"document", false}};
  e.cancel = {
      {NarrativeActorPredicate{"actor", NarrativeActorState::Blocked},
       NarrativeEventPredicate{"previous", NarrativeEventTerminalState::Failed},
       NarrativeElapsedPredicate{3600}}};
  e.steps = {NarrativeSetFactStep{"fact", false},
             NarrativeSetLightStep{"light", true},
             NarrativeSetDoorOpenStep{"door", true},
             NarrativeSetDoorLockedStep{"door", false},
             NarrativeSetRadioStep{"radio", true},
             NarrativePlayCueStep{"source"},
             NarrativeRunRouteStep{"actor", "route"},
             NarrativeDelayStep{3600},
             NarrativeWaitUntilStep{{NarrativeElapsedPredicate{0}}}};
  EXPECT_TRUE(validateNarrativeDefinitions(d).empty());
  EXPECT_TRUE(d.narrative.facts[0].initial_value);
  EXPECT_NE(findNarrativeFact(d.narrative, "fact"), nullptr);
  EXPECT_NE(findNarrativeRegion(d.narrative, "region"), nullptr);
  EXPECT_NE(findNarrativeEvent(d.narrative, "event"), nullptr);
  EXPECT_EQ(findNarrativeEvent(d.narrative, "missing"), nullptr);
  for (int state = 0; state <= static_cast<int>(NarrativeActorState::Canceled);
       ++state) {
    e.guards = {NarrativeActorPredicate{
        "actor", static_cast<NarrativeActorState>(state)}};
    EXPECT_TRUE(validateNarrativeDefinitions(d).empty());
  }
  for (auto state : {NarrativeEventTerminalState::Completed,
                     NarrativeEventTerminalState::Canceled,
                     NarrativeEventTerminalState::Failed}) {
    e.guards = {NarrativeEventPredicate{"previous", state}};
    EXPECT_TRUE(validateNarrativeDefinitions(d).empty());
  }
}

TEST(NarrativeDefinitions, RegionBoundsAreHalfOpenAndRequireFiniteExtent) {
  auto d = definitions();
  auto& r = d.narrative.regions[0];
  EXPECT_TRUE(narrativeRegionContains(r, {0, 0, 0}));
  EXPECT_TRUE(narrativeRegionContains(r, {1, 2, 3}));
  EXPECT_FALSE(narrativeRegionContains(r, {2, 2, 3}));
  EXPECT_FALSE(narrativeRegionContains(r, {1, 4, 3}));
  EXPECT_FALSE(narrativeRegionContains(r, {1, 2, 6}));
  for (float invalid : {0.F, -1.F, std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()}) {
    r.half_extent.x = invalid;
    EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".half_extent"));
  }
  r.half_extent.x = std::numeric_limits<float>::max();
  r.center.x = std::numeric_limits<float>::max();
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".half_extent"));
  // Finite bounds that round to one value would contain no feet at runtime.
  r.center.x = 1e8F;
  r.half_extent.x = 1;
  ASSERT_EQ(r.center.x - r.half_extent.x, r.center.x + r.half_extent.x);
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".half_extent"));
  r.center.x = std::numeric_limits<float>::quiet_NaN();
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".center"));
}

TEST(NarrativeDefinitions, BoundsIdsAndRequiredConjunctionsAreValidated) {
  auto d = definitions();
  d.narrative = {};
  for (std::size_t i = 0; i < 32; ++i) {
    d.narrative.facts.push_back({"fact-" + std::to_string(i), i % 2 == 0});
    d.narrative.regions.push_back({"region-" + std::to_string(i)});
  }
  for (std::size_t i = 0; i < 64; ++i)
    d.narrative.events.push_back(
        {"event-" + std::to_string(i),
         {},
         std::vector<NarrativePredicate>(8, NarrativeElapsedPredicate{0}),
         {},
         NarrativeRepeat::Once,
         std::vector<NarrativeStep>(32, NarrativeDelayStep{0})});
  EXPECT_TRUE(validateNarrativeDefinitions(d).empty());
  d.narrative.facts.push_back({"extra"});
  d.narrative.regions.push_back({"extra"});
  d.narrative.events.push_back(d.narrative.events.back());
  auto issues = validateNarrativeDefinitions(d);
  EXPECT_TRUE(field(issues, "narrative.facts"));
  EXPECT_TRUE(field(issues, "narrative.regions"));
  EXPECT_TRUE(field(issues, "narrative.events"));
  d = definitions();
  d.narrative.facts[0].id = "Invalid";
  d.narrative.regions.push_back(d.narrative.regions[0]);
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), "facts[0].id"));
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), "regions[1].id"));
  d = definitions();
  auto& e = d.narrative.events.back();
  e.guards.resize(9);
  e.cancel = std::vector<NarrativePredicate>{};
  e.trigger = NarrativeConditionTrigger{};
  e.steps = {NarrativeWaitUntilStep{}};
  issues = validateNarrativeDefinitions(d);
  EXPECT_TRUE(field(issues, ".guards"));
  EXPECT_TRUE(field(issues, ".cancel"));
  EXPECT_TRUE(field(issues, ".trigger.predicates"));
  EXPECT_TRUE(field(issues, ".steps[0].predicates"));
  e.steps.clear();
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".steps"));
  e.steps.resize(33, NarrativeDelayStep{0});
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".steps"));
}

TEST(NarrativeDefinitions,
     TypedDanglingReferencesAndSelfReferencesHaveContext) {
  auto d = definitions();
  auto& e = d.narrative.events.back();
  const std::vector<NarrativePredicate> predicates{
      NarrativeFactPredicate{"light", true},
      NarrativeRegionPredicate{"fact", true},
      NarrativeLightPredicate{"door", true},
      NarrativeDoorEndpointPredicate{"light", true},
      NarrativeDoorLockedPredicate{"light", false},
      NarrativeRadioPredicate{"source", true},
      NarrativeBoxPredicate{"document", false},
      NarrativeDocumentPredicate{"box", true},
      NarrativeActorPredicate{"route", NarrativeActorState::Idle},
      NarrativeEventPredicate{"event", NarrativeEventTerminalState::Completed}};
  for (const auto& predicate : predicates) {
    e.guards = {predicate};
    const auto issues = validateNarrativeDefinitions(d, "scene.level.json");
    ASSERT_FALSE(issues.empty());
    EXPECT_TRUE(field(issues, "events[1].guards[0]"));
    EXPECT_EQ(issues.back().source_path, "scene.level.json");
    EXPECT_NE(issues.back().message.find("event 'event'"), std::string::npos);
  }
  e.guards.clear();
  e.steps = {NarrativeSetFactStep{"missing", true},
             NarrativeSetLightStep{"missing", false},
             NarrativeSetDoorOpenStep{"missing", true},
             NarrativeSetDoorLockedStep{"missing", true},
             NarrativeSetRadioStep{"missing", true},
             NarrativePlayCueStep{"missing"},
             NarrativeRunRouteStep{"missing", "missing"}};
  for (std::size_t i = 0; i < e.steps.size(); ++i)
    EXPECT_TRUE(field(validateNarrativeDefinitions(d),
                      ".steps[" + std::to_string(i) + "]"));
  d.characters.routes[0].actor = "someone-else";
  e.steps = {NarrativeRunRouteStep{"actor", "route"}};
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".route"));
}

TEST(NarrativeDefinitions, InteractionKindsRequireMatchingTargetsAndActions) {
  auto d = definitions();
  auto& e = d.narrative.events.back();
  const std::vector<NarrativeInteractionTrigger> triggers{
      {NarrativeInteractionTarget::Door, "door",
       NarrativeInteractionAction::DoorInteract},
      {NarrativeInteractionTarget::Door, "door",
       NarrativeInteractionAction::DoorLock},
      {NarrativeInteractionTarget::Door, "door",
       NarrativeInteractionAction::DoorKnock},
      {NarrativeInteractionTarget::Switch, "switch",
       NarrativeInteractionAction::SwitchActivate},
      {NarrativeInteractionTarget::Radio, "radio",
       NarrativeInteractionAction::RadioOn},
      {NarrativeInteractionTarget::Radio, "radio",
       NarrativeInteractionAction::RadioOff},
      {NarrativeInteractionTarget::Document, "document",
       NarrativeInteractionAction::DocumentOpen},
      {NarrativeInteractionTarget::Box, "box",
       NarrativeInteractionAction::BoxPickup},
      {NarrativeInteractionTarget::Box, "box",
       NarrativeInteractionAction::BoxDrop},
      {NarrativeInteractionTarget::Box, "box",
       NarrativeInteractionAction::BoxThrow}};
  for (auto trigger : triggers) {
    e.trigger = trigger;
    EXPECT_TRUE(validateNarrativeDefinitions(d).empty());
    trigger.action = static_cast<NarrativeInteractionAction>(100);
    e.trigger = trigger;
    EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".trigger.action"));
    trigger.action = NarrativeInteractionAction::DoorInteract;
    trigger.target = "absent";
    e.trigger = trigger;
    EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".trigger.target"));
  }
  e.trigger = NarrativeSceneEntryTrigger{};
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".repeat"));
  e.repeat = NarrativeRepeat::Once;
  EXPECT_TRUE(validateNarrativeDefinitions(d).empty());
}

TEST(NarrativeDefinitions,
     InvalidDurationsAndEnumValuesAreRejectedBeforeSerialization) {
  auto d = definitions();
  auto& e = d.narrative.events.back();
  for (float value : {-1.F, 3600.1F, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN()}) {
    e.guards = {NarrativeElapsedPredicate{value}};
    e.steps = {NarrativeDelayStep{value}};
    const auto issues = validateNarrativeDefinitions(d);
    EXPECT_TRUE(field(issues, ".guards[0].seconds"));
    EXPECT_TRUE(field(issues, ".steps[0].seconds"));
  }
  e.guards = {
      NarrativeActorPredicate{"actor", static_cast<NarrativeActorState>(99)},
      NarrativeEventPredicate{"previous",
                              static_cast<NarrativeEventTerminalState>(99)}};
  e.repeat = static_cast<NarrativeRepeat>(99);
  e.trigger = NarrativeInteractionTrigger{
      static_cast<NarrativeInteractionTarget>(99), "door"};
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".guards[0].state"));
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".guards[1].state"));
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".repeat"));
  EXPECT_TRUE(field(validateNarrativeDefinitions(d), ".target_kind"));
}

TEST(NarrativeDefinitions, CueReservationAndRouteOwnershipDoNotTransfer) {
  auto d = definitions();
  d.narrative.events.back().steps = {NarrativePlayCueStep{"source"}};
  auto contender = d.narrative.events.back();
  contender.id = "contender";
  d.narrative.events.push_back(contender);
  EXPECT_TRUE(validateNarrativeDefinitions(d).empty());
  for (int conflict = 0; conflict < 5; ++conflict) {
    auto changed = d;
    if (conflict == 0) changed.audio.sources[0].autoplay = true;
    if (conflict == 1) changed.audio.cues[0].loop = true;
    if (conflict == 2) changed.characters.actors[0].footstep_source = "source";
    if (conflict == 3)
      changed.characters.actors[0].interaction_source = "source";
    if (conflict == 4) changed.household.radios[0].source = "source";
    EXPECT_TRUE(
        field(validateNarrativeDefinitions(changed), ".steps[0].source"));
  }
}
