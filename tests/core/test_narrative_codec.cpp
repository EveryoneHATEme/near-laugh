#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>

#include "core/world/narrative.hpp"
#include "core/world/prototype_level.hpp"
#include "editor/editor_document.hpp"

namespace {
using Json = nlohmann::ordered_json;
std::string bytes(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), {}};
}
bool field(const std::vector<LevelDiagnostic>& diagnostics,
           std::string_view path) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [&](const auto& d) {
                       return d.document_path.find(path) != std::string::npos;
                     });
}
LevelDocument narrativeDocument() {
  auto loaded =
      loadLevelDocument("resources/levels/household-interactions.level.json");
  if (!loaded)
    throw std::runtime_error(formatLevelDiagnostics(loaded.diagnostics));
  auto d = *loaded.document;
  d.light_switches.push_back({{0, 1, 0}, 0, "table-light", "switch"});
  d.audio.cues.push_back({"sequence-cue", "invitation", "invitation",
                          AudioCueKind::Essential, false, true});
  d.audio.sources.push_back({"sequence-source", "sequence-cue"});
  d.narrative.facts = {{"done", false}, {"enabled", true}};
  d.narrative.regions = {{"region-z", {0, 1, 0}, {2, 1, 3}},
                         {"region-a", {5, 6, 7}, {1, 2, 3}}};
  d.narrative.events = {{"previous",
                         {},
                         {},
                         {},
                         NarrativeRepeat::Once,
                         {NarrativeSetFactStep{"enabled", true}}}};
  NarrativeEventDefinition event{
      "sequence",
      NarrativeRegionEntryTrigger{"region-z"},
      {NarrativeFactPredicate{"enabled", true},
       NarrativeRegionPredicate{"region-a", false},
       NarrativeLightPredicate{"table-light", true},
       NarrativeDoorEndpointPredicate{"contact-door", false},
       NarrativeDoorLockedPredicate{"contact-door", false},
       NarrativeRadioPredicate{"receiver", true},
       NarrativeBoxPredicate{"table-box", false},
       NarrativeDocumentPredicate{"letter", true}},
      std::vector<NarrativePredicate>{
          NarrativeActorPredicate{"walker", NarrativeActorState::Blocked},
          NarrativeEventPredicate{"previous",
                                  NarrativeEventTerminalState::Failed},
          NarrativeElapsedPredicate{3600}},
      NarrativeRepeat::Rearm,
      {NarrativeSetFactStep{"done", true},
       NarrativeSetLightStep{"table-light", false},
       NarrativeSetDoorOpenStep{"contact-door", true},
       NarrativeSetDoorLockedStep{"contact-door", false},
       NarrativeSetRadioStep{"receiver", true},
       NarrativePlayCueStep{"sequence-source"},
       NarrativeRunRouteStep{"walker", "walk"}, NarrativeDelayStep{1.25F},
       NarrativeWaitUntilStep{{NarrativeElapsedPredicate{0}}}}};
  d.narrative.events.push_back(event);
  event.id = "condition";
  event.trigger =
      NarrativeConditionTrigger{{NarrativeFactPredicate{"done", true}}};
  event.cancel.reset();
  d.narrative.events.push_back(event);
  const std::vector<NarrativeInteractionTrigger> triggers{
      {NarrativeInteractionTarget::Door, "contact-door",
       NarrativeInteractionAction::DoorInteract},
      {NarrativeInteractionTarget::Door, "contact-door",
       NarrativeInteractionAction::DoorLock},
      {NarrativeInteractionTarget::Door, "contact-door",
       NarrativeInteractionAction::DoorKnock},
      {NarrativeInteractionTarget::Switch, "switch",
       NarrativeInteractionAction::SwitchActivate},
      {NarrativeInteractionTarget::Radio, "receiver",
       NarrativeInteractionAction::RadioOn},
      {NarrativeInteractionTarget::Radio, "receiver",
       NarrativeInteractionAction::RadioOff},
      {NarrativeInteractionTarget::Document, "letter",
       NarrativeInteractionAction::DocumentOpen},
      {NarrativeInteractionTarget::Box, "table-box",
       NarrativeInteractionAction::BoxPickup},
      {NarrativeInteractionTarget::Box, "table-box",
       NarrativeInteractionAction::BoxDrop},
      {NarrativeInteractionTarget::Box, "table-box",
       NarrativeInteractionAction::BoxThrow}};
  for (std::size_t i = 0; i < triggers.size(); ++i) {
    event.id = "interaction-" + std::to_string(i);
    event.trigger = triggers[i];
    event.guards = {
        NarrativeActorPredicate{"walker",
                                static_cast<NarrativeActorState>(i % 7)},
        NarrativeEventPredicate{
            "previous", static_cast<NarrativeEventTerminalState>(i % 3)}};
    d.narrative.events.push_back(event);
  }
  return d;
}
class NarrativeCodec : public testing::Test {
 protected:
  const std::filesystem::path path = std::filesystem::temp_directory_path() /
                                     "near_laugh_narrative_codec.json";
  void TearDown() override { std::filesystem::remove(path); }
  void write(std::string_view content) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
  }
};
}  // namespace

TEST_F(NarrativeCodec, EveryKindRoundTripsCanonicallyWithoutRuntimeState) {
  const auto d = narrativeDocument();
  const auto saved = saveLevelDocument(path, d);
  ASSERT_TRUE(saved) << formatLevelDiagnostics(saved.diagnostics);
  const auto canonical = bytes(path);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  EXPECT_EQ(loaded.source_version, 11U);
  EXPECT_EQ(*loaded.document, d);
  EXPECT_EQ(makePrototypeLevel(d).narrative(), d.narrative);
  EXPECT_TRUE(prototypeLevelIsValid(makePrototypeLevel(d)));
  ASSERT_TRUE(saveLevelDocument(path, *loaded.document));
  EXPECT_EQ(bytes(path), canonical);
  EXPECT_EQ(canonical.back(), '\n');
  EXPECT_NE(canonical[canonical.size() - 2], '\n');
  const auto json = Json::parse(canonical);
  EXPECT_EQ(json["narrative"].size(), 3U);
  EXPECT_EQ(json["narrative"]["events"][0].size(), 6U);
  EXPECT_TRUE(json["narrative"]["events"][0]["cancel"].is_null());
  EditorDocument editor;
  ASSERT_TRUE(editor.open(path));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(editor.document()->narrative, d.narrative);
  EXPECT_EQ(bytes(path), canonical);
}

TEST_F(NarrativeCodec, ExactKindShapesRejectMissingWrongAndRuntimeFields) {
  const auto serialized = serializeLevelDocument(narrativeDocument());
  ASSERT_TRUE(serialized) << formatLevelDiagnostics(serialized.diagnostics);
  const auto valid = Json::parse(serialized.bytes);
  const std::vector<std::pair<std::function<void(Json&)>, std::string>> defects{
      {[](Json& j) { j.erase("narrative"); }, "narrative"},
      {[](Json& j) { j["narrative"].erase("regions"); }, "narrative.regions"},
      {[](Json& j) { j["narrative"]["runtime_facts"] = Json::array(); },
       "runtime_facts"},
      {[](Json& j) { j["narrative"]["facts"][0]["initial_value"] = 1; },
       "initial_value"},
      {[](Json& j) { j["narrative"]["events"][1]["instance"] = 9; },
       "instance"},
      {[](Json& j) { j["narrative"]["events"][1].erase("cancel"); }, ".cancel"},
      {[](Json& j) {
         j["narrative"]["events"][1]["trigger"]["kind"] = "script";
       },
       ".trigger.kind"},
      {[](Json& j) {
         j["narrative"]["events"][0]["trigger"]["region"] = "region-z";
       },
       ".trigger.region"},
      {[](Json& j) {
         j["narrative"]["events"][1]["guards"][0]["light"] = "table-light";
       },
       ".guards[0].light"},
      {[](Json& j) { j["narrative"]["events"][1]["guards"][0].erase("value"); },
       ".guards[0].value"},
      {[](Json& j) {
         j["narrative"]["events"][1]["cancel"][0]["state"] = "combat";
       },
       ".cancel[0].state"},
      {[](Json& j) {
         j["narrative"]["events"][1]["steps"][0]["kind"] = "teleport";
       },
       ".steps[0].kind"},
      {[](Json& j) {
         j["narrative"]["events"][1]["steps"][0]["door"] = "contact-door";
       },
       ".steps[0].door"},
      {[](Json& j) { j["narrative"]["events"][1]["steps"][6].erase("actor"); },
       ".steps[6].actor"},
      {[](Json& j) {
         j["narrative"]["events"][3]["trigger"]["action"] = "refused";
       },
       ".trigger.action"}};
  for (const auto& [mutate, expected] : defects) {
    SCOPED_TRACE(expected);
    auto changed = valid;
    mutate(changed);
    write(changed.dump());
    const auto loaded = loadLevelDocument(path);
    EXPECT_FALSE(loaded.document);
    EXPECT_TRUE(field(loaded.diagnostics, expected));
  }
}

TEST_F(NarrativeCodec, DanglingTypedLinksRemainRepairableButCannotSaveOrPlay) {
  const auto serialized = serializeLevelDocument(narrativeDocument());
  ASSERT_TRUE(serialized);
  auto json = Json::parse(serialized.bytes);
  json["narrative"]["events"][1]["steps"][5]["source"] = "deleted-source";
  write(json.dump());
  const auto original = bytes(path);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded.document);
  EXPECT_TRUE(loaded);
  EXPECT_TRUE(field(validateLevelDocument(*loaded.document, path), ".steps[5].source"));
  EXPECT_FALSE(serializeLevelDocument(*loaded.document));
  EXPECT_THROW((void)makePrototypeLevel(*loaded.document), std::invalid_argument);
  EditorDocument editor;
  ASSERT_TRUE(editor.open(path));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(std::get<NarrativePlayCueStep>(
                editor.document()->narrative.events[1].steps[5])
                .source,
            "deleted-source");
  EXPECT_FALSE(editor.save());
  EXPECT_EQ(bytes(path), original);
}

TEST_F(NarrativeCodec, EveryHistoricalVersionNormalizesEmptyWithoutRewriting) {
  for (const auto* fixture :
       {"prototype-v3", "prototype-v4", "prototype-v5", "prototype-v6",
        "audio-captions-v7", "prototype-v8", "household-v9",
        "v10/household-interactions"}) {
    auto json = Json::parse(
        bytes(std::string("tests/fixtures/levels/") + fixture + ".level.json"));
    const unsigned version = json["version"];
    for (unsigned candidate = version == 3 ? 2 : version; candidate <= version;
         ++candidate) {
      SCOPED_TRACE(candidate);
      auto legacy = json;
      legacy["version"] = candidate;
      if (candidate == 2) legacy.erase("light_switch");
      const auto original = legacy.dump();
      write(original);
      const auto loaded = loadLevelDocument(path);
      ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
      EXPECT_EQ(loaded.source_version, candidate);
      EXPECT_EQ(loaded.document->version, 11U);
      EXPECT_EQ(loaded.document->narrative, LevelNarrative{});
      EXPECT_EQ(bytes(path), original);
      EditorDocument editor;
      ASSERT_TRUE(editor.open(path));
      EXPECT_FALSE(editor.dirty());
      EXPECT_EQ(editor.sourceVersion(), candidate);
      ASSERT_TRUE(editor.save());
      const auto canonical = bytes(path);
      auto saved_json = Json::parse(canonical);
      EXPECT_EQ(saved_json["version"], 11);
      if (candidate == 10) {
        saved_json.erase("narrative");
        saved_json["version"] = 10;
        EXPECT_EQ(nlohmann::json(saved_json), nlohmann::json(legacy));
      }
      const auto reloaded = loadLevelDocument(path);
      ASSERT_TRUE(reloaded);
      EXPECT_EQ(*reloaded.document, *loaded.document);
      ASSERT_TRUE(saveLevelDocument(path, *reloaded.document));
      EXPECT_EQ(bytes(path), canonical);
      legacy["narrative"] = {{"facts", Json::array()},
                             {"regions", Json::array()},
                             {"events", Json::array()}};
      write(legacy.dump());
      const auto rejected = loadLevelDocument(path);
      EXPECT_FALSE(rejected.document);
      EXPECT_TRUE(field(rejected.diagnostics, "narrative"));
    }
  }
}

TEST_F(NarrativeCodec, AllPackagedOriginalsRetainPriorContentAndOrder) {
  for (const auto& entry :
       std::filesystem::directory_iterator("tests/fixtures/levels/v10")) {
    if (entry.path().extension() != ".json") continue;
    const auto original = nlohmann::json::parse(bytes(entry.path()));
    auto current = nlohmann::json::parse(bytes(
        std::filesystem::path("resources/levels") / entry.path().filename()));
    ASSERT_EQ(original["version"], 10);
    ASSERT_EQ(current["version"], 11);
    EXPECT_EQ(current["narrative"]["events"].size(), 0U);
    current.erase("narrative");
    current["version"] = 10;
    EXPECT_EQ(current, original) << entry.path();
    const auto loaded = loadLevelDocument(
        std::filesystem::path("resources/levels") / entry.path().filename());
    EXPECT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  }
}
