#include <gtest/gtest.h>

#include <set>
#include <vector>

#include "editor/automation/application_snapshot.hpp"

namespace editor_automation {
namespace {
Json find(const Json& values, std::string_view projection, std::string_view field) {
  for (const auto& entry : values)
    if (entry.at("projection") == std::string(projection) && entry.at("field") == std::string(field))
      return entry.at("value");
  return nullptr;
}

TEST(ApplicationSnapshot, ObservationCopiesAppliedValuesWithoutChangingHistory) {
  EditorDocument document;
  document.requestNewInterior();
  const auto id = document.lightIds().front();
  document.select(id);
  ObjectRefs refs{{id, "object-1"}};
  const auto before = document.revision();
  const auto dirty = document.dirty();
  const auto undo = document.canUndo();
  const auto first = captureApplication(document, refs);
  const auto second = captureApplication(document, refs);
  EXPECT_EQ(first, second);
  EXPECT_EQ(document.revision(), before);
  EXPECT_EQ(document.dirty(), dirty);
  EXPECT_EQ(document.canUndo(), undo);
  EXPECT_EQ(find(first, "selection", "object_ref")["value"], "object-1");
  auto light = std::get<PrototypePointLight>(*document.object(id));
  light.position.x += 1;
  ASSERT_TRUE(document.replaceObject(id, light));
  const auto after = captureApplication(document, refs);
  EXPECT_NE(find(first, "object", "position.x"), find(after, "object", "position.x"));
  EXPECT_EQ(find(first, "document", "revision")["value"], std::to_string(before));
  EXPECT_TRUE(document.undo());
  EXPECT_EQ(find(captureApplication(document, refs), "object", "position.x"),
            find(first, "object", "position.x"));
}

TEST(ApplicationSnapshot, EveryValueConformsToItsExplicitProjectionSchema) {
  EditorDocument document;
  document.requestNewInterior();
  ObjectRefs refs;
  for (auto id : document.solidIds()) refs[id] = std::to_string(id);
  for (auto id : document.lightIds()) refs[id] = std::to_string(id);
  for (auto id : document.entryIds()) refs[id] = std::to_string(id);
  const auto values = captureApplication(document, refs);
  const Json stamp{{"snapshot_id", "frame"}, {"frame", "1"},
      {"document_generation", "1"}, {"document_revision", "1"},
      {"selection_revision", "1"}, {"preview_revision", "1"}, {"stale", false}};
  Json response{{"protocol_version", 1}, {"session_id", "session"}, {"request_id", "read"},
                {"build_fingerprint", protocolHello().at("build_fingerprint")}, {"ok", true},
                {"snapshot", stamp}, {"values", values}, {"next_cursor", nullptr}, {"truncated", false}};
  EXPECT_NO_THROW(validateResult("app_inspect", response));
}

TEST(ApplicationSnapshot, PreviewValuesHaveSeparateProvenance) {
  EditorDocument document;
  document.requestNewInterior();
  const Json fields{{"character.time", knownValue(1.25, "float")},
                    {"resource_current", knownValue(false, "boolean")}};
  const auto values = captureApplication(document, {}, fields);
  EXPECT_EQ(find(values, "preview", "character.time")["value"], 1.25);
  EXPECT_EQ(find(values, "preview", "resource_current")["value"], false);
  EXPECT_FALSE(document.canUndo());
}

TEST(ApplicationSnapshot, EveryConcreteFixtureRecordHasSchemaValidCopiedFields) {
  std::set<std::string> kinds;
  for (const auto* path : {"resources/levels/prototype.level.json",
                           "resources/levels/interior-lighting.level.json",
                           "resources/levels/audio-captions.level.json",
                           "resources/levels/scripted-characters.level.json",
                           "resources/levels/household-interactions.level.json"}) {
    EditorDocument document;
    ASSERT_TRUE(document.open(path));
    ObjectRefs refs;
    const auto append = [&](const auto& ids) {
      for (const auto id : ids) refs[id] = "object-" + std::to_string(id);
    };
    append(document.solidIds()); append(document.entryIds()); append(document.lightIds());
    append(document.switchIds()); append(document.doorIds()); append(document.propIds());
    for (int kind = 0; kind != 4; ++kind) append(document.audioIds(static_cast<EditorAudioKind>(kind)));
    for (int kind = 0; kind != 3; ++kind) {
      append(document.characterIds(static_cast<EditorCharacterKind>(kind)));
      append(document.householdIds(static_cast<EditorHouseholdKind>(kind)));
    }
    const auto revision = document.revision();
    const auto values = captureApplication(document, refs);
    for (const auto& value : values) {
      if (value.at("projection") == "object" && value.at("field") == "record_type")
        kinds.insert(value.at("value").at("value").get<std::string>());
      const Json stamp{{"snapshot_id", "frame"}, {"frame", "1"},
          {"document_generation", "1"}, {"document_revision", "1"},
          {"selection_revision", "1"}, {"preview_revision", "1"}, {"stale", false}};
      const Json page{{"protocol_version", 1}, {"session_id", "session"}, {"request_id", "inspect"},
          {"build_fingerprint", protocolHello().at("build_fingerprint")}, {"ok", true},
          {"snapshot", stamp}, {"values", Json::array({value})}, {"next_cursor", nullptr}, {"truncated", false}};
      ASSERT_NO_THROW(validateResult("app_inspect", page)) << value.dump();
    }
    EXPECT_EQ(document.revision(), revision);
    EXPECT_FALSE(document.dirty());
    EXPECT_FALSE(document.canUndo());
  }
  EXPECT_EQ(kinds.size(), 16U);
}

TEST(ApplicationSnapshot, BoundedValuesNeverClaimCompleteOversizedCollections) {
  const auto text = knownValue(std::string(20000, 'x'), "string");
  EXPECT_EQ(text.at("availability"), "truncated");
  EXPECT_LE(text.at("value").get<std::string>().size(), 16384U);
  const auto strings = knownValue(std::vector<std::string>(300, "value"), "strings");
  EXPECT_EQ(strings.at("availability"), "truncated");
  EXPECT_EQ(strings.at("value").size(), 256U);
  EXPECT_EQ(strings.at("total_count"), 300U);
  EXPECT_EQ(knownValue(std::vector<std::string>{std::string(20000, 'x')}, "strings").at("availability"), "truncated");
}
}  // namespace
}  // namespace editor_automation
