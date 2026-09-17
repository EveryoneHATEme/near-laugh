#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string>

#include "core/world/household.hpp"
#include "core/world/prototype_level.hpp"

namespace {
LevelDocument householdScene() {
  LevelDocument d;
  d.solids = {{{0, -.25F, 0},
               {10, .25F, 10},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{-4, 0, 0}, 0}}};
  d.default_entry = "default";
  d.household.boxes = {{"parcel", {0, .15F, 0}, 0}};
  d.household.documents = {
      {"note",
       {1, .003F, 0},
       0,
       "Письмо Ёжика",
       {"Первая страница.", "Вторая страница: ёж и Latin."}}};
  d.props = {{"receiver", "apartment-radio", {2, 0, 0}, 0, 1, {}}};
  d.audio.cues = {
      {"loop", "radio", "radio", AudioCueKind::Ambience, true, true}};
  d.audio.sources = {{"receiver-loop", "loop", {7, 1, 7}, 1, 1, 20, false}};
  d.household.radios = {{"radio-control", "receiver", "receiver-loop", true}};
  return d;
}
bool diagnostic(const std::vector<LevelDiagnostic>& ds, std::string_view field,
                std::string_view message = {}) {
  return std::any_of(ds.begin(), ds.end(), [&](const auto& d) {
    return d.document_path.find(field) != std::string::npos &&
           d.message.find(message) != std::string::npos;
  });
}
std::string repeated(std::string_view scalar, std::size_t count) {
  std::string result;
  for (std::size_t i = 0; i < count; ++i) result += scalar;
  return result;
}
}  // namespace

TEST(HouseholdDefinitions, ClearSurfaceContactAirborneAndEmptyAreValid) {
  auto d = householdScene();
  EXPECT_TRUE(validateLevelDocument(d).empty());
  for (float y : {.15F, .14995F, 2.F}) {
    d.household.boxes[0].center.y = y;
    d.household.boxes[0].yaw_degrees = 37;
    EXPECT_TRUE(validateLevelDocument(d).empty()) << y;
  }
  d.household = {};
  EXPECT_TRUE(validateLevelDocument(d).empty());
}

TEST(HouseholdDefinitions, RejectsFloorPropAndRotatedBoxPenetration) {
  const auto base = householdScene();
  auto d = base;
  d.household.boxes[0].center.y = .149F;
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[0].center",
                         "solid[0]"));
  d = base;
  d.props.push_back({"table",
                     "prototype-chair",
                     {0, .15F, 0},
                     33,
                     1,
                     {{{0, 0, 0}, {.05F, .05F, .5F}}}});
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[0].center",
                         "table"));
  d = base;
  d.household.boxes.push_back({"second", {.34F, .15F, 0}, 45});
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[1].center",
                         "parcel"));
  d.household.boxes[1].center.x = .37F;
  EXPECT_TRUE(validateLevelDocument(d).empty());
  d.household.boxes[1] = {"second", {.3F, .15F, 0}, 0};
  EXPECT_TRUE(validateLevelDocument(d).empty());
}

TEST(HouseholdDefinitions, TerrainContactAndPenetrationUseActualSurface) {
  auto d = householdScene();
  d.terrain.emplace();
  d.terrain->origin = {-24, 0, -24};
  d.terrain->sample_spacing = prototype_terrain_sample_spacing;
  d.solids[0].center = {8, -.25F, 8};
  d.solids[0].half_extent = {1, .25F, 1};
  EXPECT_TRUE(validateLevelDocument(d).empty());
  d.household.boxes[0].center.y = .14F;
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[0].center",
                         "terrain"));
  d.household.boxes[0].center.y = .25F;
  d.terrain->heights[48 * prototype_terrain_sample_count + 48] = .2F;
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[0].center",
                         "terrain"));
  d.household.boxes[0].center.y = .4F;
  EXPECT_TRUE(validateLevelDocument(d).empty());
}

TEST(HouseholdDefinitions, EveryEntryInitialActorAndDoorAreProtected) {
  const auto base = householdScene();
  auto d = base;
  d.entries.push_back({"other", {{0, 0, 0}, 0}});
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[0].center",
                         "entry 'other'"));
  d = base;
  d.characters.marks = {{"start", {0, 0, 0}, 0}};
  d.characters.actors = {
      {"resident", "test-mannequin", "start", {}, 1, {}, {}}};
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[0].center",
                         "actor 'resident'"));
  d = base;
  DoorDefinition door;
  door.id = "door";
  door.hinge_position = {-.45F, 0, 0};
  d.doors.push_back(door);
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[0].center",
                         "door 'door'"));
  d.doors[0].initially_open = true;
  EXPECT_TRUE(validateLevelDocument(d).empty());
}

TEST(HouseholdDefinitions,
     BoxBelowTerrainAtPartialFootprintCannotEscapeValidation) {
  auto d = householdScene();
  d.terrain.emplace();
  d.terrain->origin = {0, 0, 0};
  d.terrain->sample_spacing = prototype_terrain_sample_spacing;
  d.solids[0].center = {8, -.25F, 8};
  d.solids[0].half_extent = {1, .25F, 1};
  d.entries[0].pose.foot_position = {2, 0, 2};
  auto& box = d.household.boxes[0];
  box.center = {-.05F, -.30F, -.05F};
  box.yaw_degrees = 45;
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[0].center",
                         "terrain"));
  box.center = {-.3F, -.30F, -.3F};
  EXPECT_TRUE(validateLevelDocument(d).empty());
  box.center = {-.05F, .15F, -.05F};
  EXPECT_TRUE(validateLevelDocument(d).empty());
}

TEST(HouseholdDefinitions, BoxesNeverProvideEntryOrMarkSupport) {
  auto d = householdScene();
  d.entries[0].pose.foot_position = {0, .3F, 0};
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "entries[0].foot_position",
                         "supporting"));
  d = householdScene();
  d.characters.marks = {{"top", {0, .3F, 0}, 0}};
  EXPECT_TRUE(diagnostic(validateLevelDocument(d),
                         "characters.marks[0].feet_position", "support"));
}

TEST(HouseholdDefinitions, CountsAndTypedIdsAreBounded) {
  auto d = householdScene();
  d.household.boxes.clear();
  for (int i = 0; i < 16; ++i)
    d.household.boxes.push_back(
        {"box-" + std::to_string(i), {0, float(i + 1), 0}, 0});
  EXPECT_TRUE(validateLevelDocument(d).empty());
  d.household.boxes.push_back({"extra", {0, 18, 0}, 0});
  EXPECT_TRUE(
      diagnostic(validateLevelDocument(d), "household.boxes", "16-record"));
  d = householdScene();
  d.household.documents.resize(33, d.household.documents.front());
  EXPECT_TRUE(
      diagnostic(validateLevelDocument(d), "household.documents", "32-record"));
  d = householdScene();
  d.household.radios.resize(9, d.household.radios.front());
  EXPECT_TRUE(
      diagnostic(validateLevelDocument(d), "household.radios", "8-record"));
  d = householdScene();
  d.household.documents[0].id = d.household.boxes[0].id;
  EXPECT_TRUE(validateLevelDocument(d).empty());
  d.household.boxes.push_back({"parcel", {1, 1, 1}, 0});
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.boxes[1].id",
                         "duplicate"));
  d.household.documents[0].id = "Bad ID";
  d.household.radios[0].id = std::string(65, 'a');
  EXPECT_TRUE(
      diagnostic(validateLevelDocument(d), "household.documents[0].id"));
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.radios[0].id"));
}

TEST(HouseholdDefinitions, TransformsMustHaveUsableFiniteDerivedBounds) {
  for (float value : {std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::max()}) {
    auto d = householdScene();
    d.household.boxes[0].center.x = value;
    d.household.documents[0].position.z = value;
    EXPECT_TRUE(
        diagnostic(validateLevelDocument(d), "household.boxes[0].center"));
    EXPECT_TRUE(diagnostic(validateLevelDocument(d),
                           "household.documents[0].position"));
  }
  auto d = householdScene();
  d.household.boxes[0].yaw_degrees = std::numeric_limits<float>::quiet_NaN();
  d.household.documents[0].yaw_degrees = std::numeric_limits<float>::infinity();
  EXPECT_TRUE(
      diagnostic(validateLevelDocument(d), "household.boxes[0].yaw_degrees"));
  EXPECT_TRUE(diagnostic(validateLevelDocument(d),
                         "household.documents[0].yaw_degrees"));
}

TEST(HouseholdDefinitions, TextLimitsCountScalarsAndPreserveOrderedPages) {
  auto d = householdScene();
  auto& note = d.household.documents[0];
  note.title = repeated("Ё", 80);
  note.pages.assign(16, repeated("ё", 480));
  EXPECT_TRUE(validateLevelDocument(d).empty());
  note.title += "A";
  note.pages[7] += "Б";
  EXPECT_TRUE(
      diagnostic(validateLevelDocument(d), "household.documents[0].title"));
  EXPECT_TRUE(diagnostic(validateLevelDocument(d),
                         "household.documents[0].pages[7]", "page 8"));
  note.title = "Title";
  note.pages.assign(17, "Page");
  EXPECT_TRUE(diagnostic(validateLevelDocument(d),
                         "household.documents[0].pages", "16 ordered"));
  note.pages.clear();
  EXPECT_TRUE(
      diagnostic(validateLevelDocument(d), "household.documents[0].pages"));
}

TEST(HouseholdDefinitions, InvalidUtf8AndEmptyTextStayDiagnosable) {
  for (const std::string& bad :
       {std::string{}, std::string("\xc0\x80"), std::string("\xed\xa0\x80"),
        std::string("\xf4\x90\x80\x80"), std::string("\xd0"),
        std::string("\xd0x")}) {
    auto d = householdScene();
    d.household.documents[0].pages[1] = bad;
    EXPECT_TRUE(diagnostic(validateLevelDocument(d),
                           "household.documents[0].pages[1]"));
    EXPECT_EQ(d.household.documents[0].pages[1], bad);
  }
}

TEST(HouseholdDefinitions, RadioRequiresExclusivePropAndSourceOwnership) {
  auto d = householdScene();
  auto copy = d.household.radios[0];
  copy.id = "second";
  d.household.radios.push_back(copy);
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.radios[1].prop",
                         "owned"));
  EXPECT_TRUE(diagnostic(validateLevelDocument(d), "household.radios[1].source",
                         "owned"));
  for (bool footstep : {false, true}) {
    d = householdScene();
    CharacterActorDefinition actor;
    actor.id = "resident";
    (footstep ? actor.footstep_source : actor.interaction_source) =
        "receiver-loop";
    d.characters.actors.push_back(actor);
    EXPECT_TRUE(diagnostic(validateLevelDocument(d),
                           "household.radios[0].source", "actor 'resident'"));
  }
}

TEST(HouseholdDefinitions, BrokenRadioLinksAreRetainedWithFieldContext) {
  for (int defect = 0; defect < 9; ++defect) {
    auto d = householdScene();
    switch (defect) {
      case 0:
        d.props.clear();
        break;
      case 1:
        d.props[0].model = "prototype-chair";
        break;
      case 2:
        d.props[0].collision_boxes.push_back({{0, .1F, 0}, {.1F, .1F, .1F}});
        break;
      case 3:
        d.audio.sources.clear();
        break;
      case 4:
        d.audio.sources[0].autoplay = true;
        break;
      case 5:
        d.audio.cues[0].loop = false;
        break;
      case 6:
        d.audio.cues[0].spatial = false;
        break;
      case 7:
        d.audio.cues[0].kind = AudioCueKind::Essential;
        break;
      case 8:
        d.audio.cues[0].caption.reset();
        break;
    }
    const auto before = d;
    EXPECT_TRUE(diagnostic(
        validateLevelDocument(d),
        defect < 3 ? "household.radios[0].prop" : "household.radios[0].source"))
        << defect;
    EXPECT_EQ(before, d);
  }
}

TEST(HouseholdDefinitions, RuntimeHandoffCopiesInitialDefinitions) {
  auto d = householdScene();
  const auto expected = d.household;
  const auto level = makePrototypeLevel(d);
  d.household.boxes[0].center.y = 4;
  d.household.documents[0].pages.clear();
  d.household.radios[0].initially_on = false;
  EXPECT_EQ(level.household(), expected);
  EXPECT_TRUE(prototypeLevelIsValid(level));
  EXPECT_THROW((void)makePrototypeLevel(d), std::invalid_argument);
}
