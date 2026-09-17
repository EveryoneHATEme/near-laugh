#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "core/world/prototype_level.hpp"
#include "editor/editor_document.hpp"

namespace {
using Json = nlohmann::ordered_json;

LevelDocument householdDocument() {
  LevelDocument d;
  d.solids = {{{0, -0.25F, 0},
               {20, 0.25F, 20},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{-8, 0, 0}, 15}}};
  d.default_entry = "default";
  d.household.boxes = {{"parcel-z", {0, 0.15F, 0}, 37},
                       {"parcel-a", {2, 2, 0}, -24}};
  d.household.documents = {
      {"note-z",
       {0, 1, 3},
       90,
       "Заметка: Ёж",
       {"Первая страница.\nСтрока 2.", "Second page: \"text\"."}},
      {"note-a", {2, 1, 3}, -45, "Ещё одна заметка", {"Последняя страница."}}};
  d.props = {{"radio-prop-z", "apartment-radio", {-4, 1, 3}, 27, 1, {}},
             {"radio-prop-a", "apartment-radio", {4, 1, 3}, -17, 1, {}}};
  d.audio.cues = {
      {"radio-loop", "radio", "radio", AudioCueKind::Ambience, true, true}};
  d.audio.sources = {{"radio-source-z", "radio-loop", {8, 1, 1}},
                     {"radio-source-a", "radio-loop", {-8, 1, 1}}};
  d.household.radios = {{"radio-z", "radio-prop-z", "radio-source-z", true},
                        {"radio-a", "radio-prop-a", "radio-source-a", false}};
  return d;
}

std::string readBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

bool hasField(const std::vector<LevelDiagnostic>& diagnostics,
              std::string_view field) {
  return std::any_of(
      diagnostics.begin(), diagnostics.end(), [&](const auto& diagnostic) {
        return diagnostic.document_path.find(field) != std::string::npos;
      });
}

class HouseholdCodec : public testing::Test {
 protected:
  const std::filesystem::path path = std::filesystem::temp_directory_path() /
                                     "near_laugh_household_codec.json";
  void TearDown() override { std::filesystem::remove(path); }
  void write(std::string_view bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
  }
};
}  // namespace

TEST_F(HouseholdCodec,
       CanonicalRoundTripPreservesOrderTextLinksAndAuthoredState) {
  const auto document = householdDocument();
  const auto saved = saveLevelDocument(path, document);
  ASSERT_TRUE(saved) << formatLevelDiagnostics(saved.diagnostics);
  const auto canonical = readBytes(path);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  EXPECT_EQ(loaded.source_version, 10U);
  EXPECT_EQ(*loaded.document, document);
  EXPECT_EQ(makePrototypeLevel(document).household(), document.household);
  ASSERT_TRUE(saveLevelDocument(path, *loaded.document));
  EXPECT_EQ(readBytes(path), canonical);
  ASSERT_FALSE(canonical.empty());
  EXPECT_EQ(canonical.back(), '\n');
  EXPECT_NE(canonical[canonical.size() - 2], '\n');
  const auto json = Json::parse(canonical);
  EXPECT_EQ(json["household"].size(), 3U);
  EXPECT_EQ(json["household"]["boxes"][0].size(), 3U);
  EXPECT_EQ(json["household"]["documents"][0].size(), 5U);
  EXPECT_EQ(json["household"]["radios"][0].size(), 4U);
  EditorDocument editor;
  ASSERT_TRUE(editor.open(path));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(editor.document()->household, document.household);
  EXPECT_EQ(readBytes(path), canonical);
}

TEST_F(HouseholdCodec, MaximumCollectionsAndPageBoundsRoundTrip) {
  auto document = householdDocument();
  document.household = {};
  document.props.clear();
  document.audio.sources.clear();
  for (std::size_t i = 0; i < level_maximum_household_box_count; ++i)
    document.household.boxes.push_back(
        {"box-" + std::to_string(i), {float(i), 0.15F, 2}, float(i * 7)});
  for (std::size_t i = 0; i < level_maximum_household_document_count; ++i)
    document.household.documents.push_back(
        {"note-" + std::to_string(i),
         {float(i), 1, 3},
         float(i * 11),
         std::string(level_maximum_document_title_scalars, 'T'),
         std::vector<std::string>(
             level_maximum_document_page_count,
             std::string(level_maximum_document_page_scalars, 'P'))});
  for (std::size_t i = 0; i < level_maximum_household_radio_count; ++i) {
    const auto suffix = std::to_string(i);
    document.props.push_back(
        {"prop-" + suffix, "apartment-radio", {float(i), 1, 8}, 0, 1, {}});
    document.audio.sources.push_back({"source-" + suffix, "radio-loop"});
    document.household.radios.push_back(
        {"radio-" + suffix, "prop-" + suffix, "source-" + suffix, i % 2 == 0});
  }
  const auto saved = saveLevelDocument(path, document);
  ASSERT_TRUE(saved) << formatLevelDiagnostics(saved.diagnostics);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  EXPECT_EQ(*loaded.document, document);
}

TEST_F(HouseholdCodec,
       StrictShapeRejectsRuntimeFieldsWrongTypesAndUnsafeCounts) {
  ASSERT_TRUE(saveLevelDocument(path, householdDocument()));
  const auto canonical = Json::parse(readBytes(path));
  struct Case {
    std::string field;
    std::function<void(Json&)> change;
  };
  const std::vector<Case> cases{
      {"household", [](auto& j) { j.erase("household"); }},
      {"household", [](auto& j) { j["household"] = nullptr; }},
      {"household.held_box",
       [](auto& j) { j["household"]["held_box"] = "parcel-z"; }},
      {"household.boxes", [](auto& j) { j["household"].erase("boxes"); }},
      {"household.documents",
       [](auto& j) { j["household"].erase("documents"); }},
      {"household.radios", [](auto& j) { j["household"].erase("radios"); }},
      {"household.boxes",
       [](auto& j) { j["household"]["boxes"] = Json::object(); }},
      {"household.boxes[0].velocity",
       [](auto& j) { j["household"]["boxes"][0]["velocity"] = {0, 0, 0}; }},
      {"household.boxes[0].mass",
       [](auto& j) { j["household"]["boxes"][0]["mass"] = 1; }},
      {"household.boxes[0].center",
       [](auto& j) { j["household"]["boxes"][0]["center"] = nullptr; }},
      {"household.boxes[0].yaw_degrees",
       [](auto& j) { j["household"]["boxes"][0]["yaw_degrees"] = "zero"; }},
      {"household.boxes[0].id",
       [](auto& j) {
         j["household"]["boxes"][0]["id"] = std::string(65, 'a');
       }},
      {"household.documents[0].title",
       [](auto& j) { j["household"]["documents"][0]["title"] = false; }},
      {"household.documents[0].pages",
       [](auto& j) { j["household"]["documents"][0]["pages"] = "page"; }},
      {"household.documents[0].pages[0]",
       [](auto& j) { j["household"]["documents"][0]["pages"][0] = nullptr; }},
      {"household.documents[0].current_page",
       [](auto& j) { j["household"]["documents"][0]["current_page"] = 1; }},
      {"household.radios[0].source",
       [](auto& j) { j["household"]["radios"][0]["source"] = nullptr; }},
      {"household.radios[0].initially_on",
       [](auto& j) { j["household"]["radios"][0]["initially_on"] = 1; }},
      {"household.radios[0].action",
       [](auto& j) { j["household"]["radios"][0]["action"] = "start"; }},
      {"household.boxes",
       [](auto& j) {
         j["household"]["boxes"] =
             std::vector<Json>(17, j["household"]["boxes"][0]);
       }},
      {"household.documents",
       [](auto& j) {
         j["household"]["documents"] =
             std::vector<Json>(33, j["household"]["documents"][0]);
       }},
      {"household.radios",
       [](auto& j) {
         j["household"]["radios"] =
             std::vector<Json>(9, j["household"]["radios"][0]);
       }},
      {"household.documents[0].pages", [](auto& j) {
         j["household"]["documents"][0]["pages"] =
             std::vector<std::string>(17, "page");
       }}};
  for (const auto& test : cases) {
    SCOPED_TRACE(test.field);
    auto json = canonical;
    test.change(json);
    const auto source = json.dump();
    write(source);
    const auto loaded = loadLevelDocument(path);
    EXPECT_FALSE(loaded.document);
    EXPECT_TRUE(hasField(loaded.diagnostics, test.field))
        << formatLevelDiagnostics(loaded.diagnostics);
    EXPECT_EQ(readBytes(path), source);
  }
}

TEST_F(HouseholdCodec, SafelyInvalidDefinitionsRemainEditableAndCannotBeSaved) {
  ASSERT_TRUE(saveLevelDocument(path, householdDocument()));
  auto json = Json::parse(readBytes(path));
  json["household"]["boxes"][0]["id"] = "Bad/id";
  json["household"]["documents"][0]["title"] = std::string(81, 't');
  json["household"]["documents"][0]["pages"] = {"", std::string(481, 'p')};
  json["household"]["documents"][1]["pages"] = Json::array();
  json["household"]["radios"][0]["prop"] = "missing-prop";
  json["household"]["radios"][0]["source"] = "missing-source";
  const auto source = json.dump();
  write(source);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  const auto diagnostics = validateLevelDocument(*loaded.document);
  for (const auto* field :
       {"household.boxes[0].id", "household.documents[0].title",
        "household.documents[0].pages[0]", "household.documents[0].pages[1]",
        "household.documents[1].pages", "household.radios[0].prop",
        "household.radios[0].source"})
    EXPECT_TRUE(hasField(diagnostics, field)) << field;
  EXPECT_FALSE(saveLevelDocument(path, *loaded.document));
  EXPECT_EQ(readBytes(path), source);
  EditorDocument editor;
  ASSERT_TRUE(editor.open(path));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(*editor.document(), *loaded.document);
  EXPECT_FALSE(editor.diagnostics().empty());
  EXPECT_EQ(readBytes(path), source);
}

TEST_F(HouseholdCodec, ExactV9PreservesEveryFieldAndOrderUntilExplicitSave) {
  const auto original =
      readBytes("tests/fixtures/levels/household-v9.level.json");
  ASSERT_FALSE(original.empty());
  // Object key order is not authored state; array order remains significant.
  const auto old_json = nlohmann::json::parse(original);
  ASSERT_EQ(old_json["version"], 9);
  ASSERT_GT(old_json["characters"]["actors"].size(), 1U);
  ASSERT_GT(old_json["characters"]["routes"].size(), 1U);
  write(original);
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
  EXPECT_EQ(loaded.source_version, 9U);
  EXPECT_EQ(loaded.document->household, LevelHousehold{});
  EXPECT_EQ(readBytes(path), original);
  EditorDocument editor;
  ASSERT_TRUE(editor.open(path));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(editor.sourceVersion(), 9U);
  EXPECT_EQ(readBytes(path), original);
  ASSERT_TRUE(editor.save());
  const auto saved = readBytes(path);
  auto new_json = nlohmann::json::parse(saved);
  EXPECT_EQ(new_json["version"], 10);
  new_json.erase("household");
  new_json["version"] = 9;
  EXPECT_EQ(new_json, old_json);
  const auto reloaded = loadLevelDocument(path);
  ASSERT_TRUE(reloaded);
  EXPECT_EQ(*reloaded.document, *loaded.document);
  ASSERT_TRUE(saveLevelDocument(path, *reloaded.document));
  EXPECT_EQ(readBytes(path), saved);
  auto missing_characters = old_json;
  missing_characters.erase("characters");
  write(missing_characters.dump());
  EXPECT_TRUE(hasField(loadLevelDocument(path).diagnostics, "characters"));
}

TEST_F(HouseholdCodec, EveryLegacyVersionNormalizesEmptyAndRejectsHousehold) {
  for (const auto* fixture :
       {"prototype-v3", "prototype-v4", "prototype-v5", "prototype-v6",
        "audio-captions-v7", "prototype-v8", "household-v9"}) {
    auto json = Json::parse(readBytes(std::string("tests/fixtures/levels/") +
                                      fixture + ".level.json"));
    const unsigned version = json["version"];
    for (unsigned candidate = version == 3 ? 2 : version; candidate <= version;
         ++candidate) {
      SCOPED_TRACE(candidate);
      auto legacy = json;
      legacy["version"] = candidate;
      if (candidate == 2) legacy.erase("light_switch");
      const auto source = legacy.dump();
      write(source);
      const auto loaded = loadLevelDocument(path);
      ASSERT_TRUE(loaded) << formatLevelDiagnostics(loaded.diagnostics);
      EXPECT_EQ(loaded.source_version, candidate);
      EXPECT_EQ(loaded.document->version, 10U);
      EXPECT_EQ(loaded.document->household, LevelHousehold{});
      EXPECT_EQ(readBytes(path), source);
      ASSERT_TRUE(saveLevelDocument(path, *loaded.document));
      const auto migrated = loadLevelDocument(path);
      ASSERT_TRUE(migrated);
      EXPECT_EQ(*migrated.document, *loaded.document);
      legacy["household"] = {{"boxes", Json::array()},
                             {"documents", Json::array()},
                             {"radios", Json::array()}};
      write(legacy.dump());
      const auto rejected = loadLevelDocument(path);
      EXPECT_FALSE(rejected.document);
      EXPECT_TRUE(hasField(rejected.diagnostics, "household"));
    }
  }
}
