#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>

#include "core/world/characters.hpp"
#include "core/world/prototype_level.hpp"
#include "development/character_fixture.hpp"
#include "editor/editor_document.hpp"
#include "editor/editor_overlay.hpp"

namespace {
LevelDocument characterDocument() {
  LevelDocument d;
  d.solids = {{{0, -0.25F, 0},
               {10, 0.25F, 10},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{-4, 0, 0}, 0}}};
  d.default_entry = "default";
  d.characters.marks = {{"start", {0, 0, 0}, 0}, {"end", {0, 0, 3}, 90}};
  d.characters.actors = {
      {"actor", "test-mannequin", "start", "route", 1, {}, {}}};
  d.characters.routes = {
      {"route", "actor", {"start", "end", "end"}, "interact"}};
  return d;
}
bool field(const std::vector<LevelDiagnostic>& ds, std::string_view name) {
  return std::any_of(ds.begin(), ds.end(), [&](const auto& d) {
    return d.document_path.find(name) != std::string::npos;
  });
}
class CharacterDefinitions : public testing::Test {
 protected:
  std::filesystem::path path = std::filesystem::temp_directory_path() /
                               "near_laugh_character_definitions.json";
  void TearDown() override { std::filesystem::remove(path); }
  std::string bytes() {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
  }
  void write(const std::string& s) {
    std::ofstream f(path, std::ios::binary);
    f << s;
  }
};
}  // namespace

TEST_F(CharacterDefinitions, EmptyAndNullDefinitionsAreValid) {
  auto d = characterDocument();
  EXPECT_TRUE(validateLevelDocument(d).empty());
  d.characters.actors[0].initial_route.reset();
  d.characters.routes[0].final_clip.reset();
  EXPECT_TRUE(validateLevelDocument(d).empty());
  d.characters = {};
  EXPECT_TRUE(validateLevelDocument(d).empty());
}

TEST_F(CharacterDefinitions, InvalidReferencesKeepVisibleDiagnosticAnchors) {
  ASSERT_TRUE(saveLevelDocument(path, characterDocument()));
  const auto valid = nlohmann::json::parse(bytes());
  for (int defect = 0; defect < 3; ++defect) {
    auto json = valid;
    WorldPosition anchor{};
    if (defect == 0) {
      json["characters"]["actors"][0]["model"] = "absent";
    } else {
      json["characters"]["actors"] = nlohmann::json::array();
      if (defect == 2) {
        json["characters"]["marks"] = nlohmann::json::array();
        anchor = {-4, 0, 0};
      }
    }
    write(json.dump());
    const auto original_bytes = bytes();
    EditorDocument editor;
    ASSERT_TRUE(editor.open(path));
    const auto camera =
        character_fixture::camera({anchor.x, 2, 5}, {anchor.x, 0, 0}, 4.F / 3);
    const auto lines = buildEditorOverlay(editor, camera);
    EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [](const auto& line) {
      return line.color == WorldColor{255, 70, 70, 255};
    })) << defect;
    EXPECT_EQ(bytes(), original_bytes);
    EXPECT_FALSE(editor.diagnostics().empty());
  }
}

TEST_F(CharacterDefinitions,
       CapacityIdentityAndReferencesHaveFieldDiagnostics) {
  auto d = characterDocument();
  for (int i = 1; i < 4; ++i) {
    auto a = d.characters.actors[0];
    a.id += std::to_string(i);
    a.initial_route.reset();
    d.characters.actors.push_back(a);
  }
  EXPECT_TRUE(validateCharacterDefinitions(d.characters, d.audio).empty());
  d.characters.actors.push_back(d.characters.actors[0]);
  EXPECT_TRUE(field(validateCharacterDefinitions(d.characters, d.audio),
                    "characters.actors"));
  d = characterDocument();
  d.characters.actors[0].model = "unknown";
  d.characters.actors[0].initial_mark = "missing";
  d.characters.actors[0].speed = std::numeric_limits<float>::quiet_NaN();
  d.characters.routes[0].actor = "missing";
  d.characters.routes[0].marks.push_back("absent");
  d.characters.routes[0].final_clip = "walk";
  const auto ds = validateLevelDocument(d);
  for (auto name : {".model", ".initial_mark", ".initial_route", ".speed",
                    ".actor", ".marks[3]", ".final_clip"})
    EXPECT_TRUE(field(ds, name)) << name;
  d = characterDocument();
  d.characters.marks.resize(33, d.characters.marks[0]);
  d.characters.routes.resize(17, d.characters.routes[0]);
  d.characters.actors[0].id = "Bad/id";
  EXPECT_TRUE(field(validateLevelDocument(d), "characters.marks"));
  EXPECT_TRUE(field(validateLevelDocument(d), "characters.routes"));
  EXPECT_TRUE(field(validateLevelDocument(d), "actors[0].id"));
  d.characters.routes[0].marks.clear();
  EXPECT_TRUE(field(validateLevelDocument(d), "routes[0].marks"));
  d.characters.routes[0].marks.resize(33, "start");
  EXPECT_TRUE(field(validateLevelDocument(d), "routes[0].marks"));
}

TEST_F(CharacterDefinitions,
       SourcesAreExclusiveSpatialOneShotsWithRequiredKinds) {
  auto d = characterDocument();
  d.audio.cues = {
      {"step", "footsteps", {}, AudioCueKind::Ambience, false, true},
      {"action", "phone-ring", "phone-ring", AudioCueKind::Essential, false,
       true}};
  d.audio.sources = {{"step-source", "step"}, {"action-source", "action"}};
  d.characters.actors[0].footstep_source = "step-source";
  d.characters.actors[0].interaction_source = "action-source";
  EXPECT_TRUE(validateLevelDocument(d).empty());
  auto copy = d;
  copy.characters.actors[0].interaction_source = "step-source";
  EXPECT_TRUE(field(validateLevelDocument(copy), ".interaction_source"));
  copy = d;
  auto actor = copy.characters.actors[0];
  actor.id = "second";
  actor.initial_route.reset();
  copy.characters.actors.push_back(actor);
  EXPECT_TRUE(field(validateLevelDocument(copy), "actors[1].footstep_source"));
  for (int mode = 0; mode < 5; ++mode) {
    copy = d;
    if (mode == 0) copy.audio.sources[0].autoplay = true;
    if (mode == 1) copy.audio.cues[0].loop = true;
    if (mode == 2) copy.audio.cues[0].spatial = false;
    if (mode == 3) copy.audio.cues[0].kind = AudioCueKind::Essential;
    if (mode == 4) copy.audio.sources.erase(copy.audio.sources.begin());
    EXPECT_TRUE(field(validateLevelDocument(copy), ".footstep_source"));
  }
}

TEST_F(CharacterDefinitions,
       MarksRequireSupportAndStartsProtectEntriesActorsAndDoors) {
  auto d = characterDocument();
  d.solids.push_back({{3, 2.75F, 0},
                      {1, 0.25F, 1},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::Floor});
  d.characters.marks.push_back({"upstairs", {3, 3, 0}, 0});
  EXPECT_TRUE(validateLevelDocument(d).empty());
  d.characters.marks.back().feet_position.y = 2;
  EXPECT_TRUE(field(validateLevelDocument(d), "marks[2].feet_position"));
  d = characterDocument();
  d.characters.marks[0].feet_position.x = -4;
  EXPECT_TRUE(field(validateLevelDocument(d), "actors[0].initial_mark"));
  EXPECT_TRUE(field(validateLevelDocument(d), "entries[0].foot_position"));
  d = characterDocument();
  d.doors = {
      {"door", {-0.5F, 0.02F, 3}, 0, 1, 2, 0.06F, 90, 90, DoorLockSide::None}};
  EXPECT_TRUE(validateLevelDocument(d).empty())
      << formatLevelDiagnostics(validateLevelDocument(d));
  d.characters.actors[0].initial_mark = "end";
  EXPECT_TRUE(field(validateLevelDocument(d), "actors[0].initial_mark"));
  EXPECT_TRUE(field(validateLevelDocument(d), "doors[0]"));
  d = characterDocument();
  d.solids.push_back({{0, 1, 3}, {0.1F, 1, 0.1F}, {255, 255, 255, 255}});
  EXPECT_TRUE(field(validateLevelDocument(d), "marks[1].feet_position"));
  d = characterDocument();
  auto a = d.characters.actors[0];
  a.id = "second";
  a.initial_route.reset();
  d.characters.actors.push_back(a);
  EXPECT_TRUE(field(validateLevelDocument(d), "actors[0].initial_mark"));
  EXPECT_TRUE(field(validateLevelDocument(d), "actors[1].initial_mark"));
}

TEST_F(CharacterDefinitions,
       CanonicalRoundTripAndEditorOpenPreserveDefinitions) {
  auto d = characterDocument();
  for (int i = 1; i < 4; ++i) {
    const auto id = std::to_string(i);
    d.characters.marks.push_back(
        {"start-" + id, {float(i * 2), 0, 0}, float(i * 30)});
    d.characters.actors.push_back({"actor-" + id,
                                   "test-mannequin",
                                   "start-" + id,
                                   {},
                                   0.25F + float(i) * 0.25F,
                                   {},
                                   {}});
    d.characters.routes.push_back(
        {"route-" + id, "actor-" + id, {"end", "start"}, {}});
  }
  ASSERT_TRUE(saveLevelDocument(path, d));
  const auto original = bytes();
  auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded);
  EXPECT_EQ(loaded.source_version, 9U);
  EXPECT_EQ(*loaded.document, d);
  EXPECT_EQ(makePrototypeLevel(d).characters(), d.characters);
  ASSERT_TRUE(saveLevelDocument(path, *loaded.document));
  EXPECT_EQ(bytes(), original);
  EditorDocument editor;
  ASSERT_TRUE(editor.open(path));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(bytes(), original);
  EXPECT_EQ(editor.document()->characters, d.characters);
}

TEST_F(CharacterDefinitions,
       ExactOlderVersionsNormalizeWithoutAcceptingCharacters) {
  for (const auto* name : {"prototype-v3", "prototype-v4", "prototype-v5",
                           "prototype-v6", "audio-captions-v7"}) {
    std::ifstream f(std::string("tests/fixtures/levels/") + name +
                    ".level.json");
    auto json = nlohmann::ordered_json::parse(f);
    write(json.dump());
    auto loaded = loadLevelDocument(path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.document->characters, LevelCharacters{});
    if (json["version"] == 3) {
      auto v2 = json;
      v2["version"] = 2;
      v2.erase("light_switch");
      write(v2.dump());
      loaded = loadLevelDocument(path);
      ASSERT_TRUE(loaded);
      EXPECT_EQ(loaded.document->characters, LevelCharacters{});
      v2["characters"] = {{"actors", nlohmann::json::array()},
                          {"marks", nlohmann::json::array()},
                          {"routes", nlohmann::json::array()}};
      write(v2.dump());
      EXPECT_FALSE(loadLevelDocument(path));
    }
    json["characters"] = {{"actors", nlohmann::json::array()},
                          {"marks", nlohmann::json::array()},
                          {"routes", nlohmann::json::array()}};
    write(json.dump());
    EXPECT_FALSE(loadLevelDocument(path));
  }
}

TEST_F(CharacterDefinitions,
       PropsDoorsAndMissingGroundCannotSupplyMarkSupport) {
  auto d = characterDocument();
  d.characters.marks[1].feet_position = {0, 2, 3};
  d.props = {
      {"prop", "prototype-chair", {0, 0, 3}, 0, 1, {{{0, 1, 0}, {1, 1, 1}}}}};
  EXPECT_TRUE(field(validateLevelDocument(d), "marks[1].feet_position"));
  d.props.clear();
  d.doors = {{"door",
              {-0.5F, 0.02F, 3},
              0,
              1,
              1.98F,
              0.06F,
              90,
              90,
              DoorLockSide::None}};
  EXPECT_TRUE(field(validateLevelDocument(d), "marks[1].feet_position"));
  d.doors.clear();
  d.characters.marks[1].feet_position = {11, 0, 3};
  EXPECT_TRUE(field(validateLevelDocument(d), "marks[1].feet_position"));
  d = characterDocument();
  d.characters.marks[0].feet_position.y = 0.5F;
  EXPECT_TRUE(field(validateLevelDocument(d), "actors[0].initial_mark"));
}

TEST_F(CharacterDefinitions,
       ExactV8NormalizesWithoutWritingAndRejectsCharacterFields) {
  for (const auto* legacy :
       {"prototype", "audio-captions", "interior-lighting-capacity"}) {
    std::ifstream f(
        std::string("tests/fixtures/levels/") + legacy + "-v8.level.json",
        std::ios::binary);
    const std::string original{std::istreambuf_iterator<char>(f), {}};
    ASSERT_FALSE(original.empty());
    write(original);
    const auto loaded = loadLevelDocument(path);
    ASSERT_TRUE(loaded);
    EXPECT_EQ(loaded.source_version, 8U);
    EXPECT_EQ(loaded.document->characters, LevelCharacters{});
    EXPECT_EQ(bytes(), original);
    EditorDocument editor;
    ASSERT_TRUE(editor.open(path));
    EXPECT_FALSE(editor.dirty());
    EXPECT_EQ(editor.sourceVersion(), 8U);
    ASSERT_TRUE(editor.save());
    auto old_json = nlohmann::ordered_json::parse(original);
    auto new_json = nlohmann::ordered_json::parse(bytes());
    new_json.erase("characters");
    new_json["version"] = 8;
    EXPECT_EQ(new_json, old_json);
    old_json["characters"] = {{"actors", nlohmann::json::array()},
                              {"marks", nlohmann::json::array()},
                              {"routes", nlohmann::json::array()}};
    write(old_json.dump());
    EXPECT_FALSE(loadLevelDocument(path));
  }
}

TEST_F(CharacterDefinitions,
       StrictShapesRejectUnsafeDataButKeepRepairableReferences) {
  ASSERT_TRUE(saveLevelDocument(path, characterDocument()));
  const auto good = nlohmann::ordered_json::parse(bytes());
  for (int mode = 0; mode < 8; ++mode) {
    auto json = good;
    if (mode == 0) json.erase("characters");
    if (mode == 1) json["characters"] = nullptr;
    if (mode == 2) json["characters"].erase("routes");
    if (mode == 3) json["characters"]["extra"] = true;
    if (mode == 4) json["characters"]["actors"][0]["scale"] = 1;
    if (mode == 5) json["characters"]["actors"][0]["speed"] = "fast";
    if (mode == 6) json["characters"]["actors"][0]["initial_route"] = false;
    if (mode == 7) json["characters"]["routes"][0]["marks"] = nullptr;
    write(json.dump());
    EXPECT_FALSE(loadLevelDocument(path)) << mode;
  }
  auto repairable = good;
  repairable["characters"]["actors"][0]["initial_mark"] = "missing";
  write(repairable.dump());
  const auto loaded = loadLevelDocument(path);
  ASSERT_TRUE(loaded.document);
  EXPECT_TRUE(field(validateLevelDocument(*loaded.document), ".initial_mark"));
  EXPECT_FALSE(saveLevelDocument(path, *loaded.document));
}
