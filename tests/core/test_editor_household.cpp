#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <set>

#include "core/world/household.hpp"
#include "editor/editor_property_edit.hpp"

namespace {
LevelDocument householdScene() {
  LevelDocument d;
  d.solids = {{{0, -.25F, 0},
               {10, .25F, 10},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{-6, 0, 0}, 0}}};
  d.default_entry = "default";
  d.household.boxes = {{"box-1", {0, .15F, 0}, 17}, {"box-2", {1, .15F, 0}, 0}};
  d.household.documents = {
      {"document-1",
       {0, .003F, 2},
       15,
       "Заметка",
       {"Первая страница.", "Вторая страница."}},
      {"document-2", {1, .003F, 2}, 0, "Письмо", {"Текст письма."}}};
  d.props = {{"prop-1", "apartment-radio", {-3, 1, 3}, 0, 1, {}},
             {"prop-2", "apartment-radio", {3, 1, 3}, 25, 1, {}}};
  d.audio.cues = {
      {"radio-cue", "radio", "radio", AudioCueKind::Ambience, true, true},
      {"step-cue", "footsteps", {}, AudioCueKind::Ambience, false, true}};
  d.audio.sources = {{"source-1", "radio-cue"},
                     {"source-2", "radio-cue"},
                     {"step-source", "step-cue"}};
  d.household.radios = {{"radio-1", "prop-1", "source-1", false},
                        {"radio-2", "prop-2", "source-2", true}};
  d.characters.marks = {{"actor-start", {6, 0, 0}, 0}};
  d.characters.actors = {
      {"actor", "test-mannequin", "actor-start", {}, 1, "step-source", {}}};
  return d;
}

std::string bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), {}};
}

std::string repeated(std::string_view scalar, std::size_t count) {
  std::string text;
  for (std::size_t i = 0; i < count; ++i) text += scalar;
  return text;
}

bool field(const std::vector<LevelDiagnostic>& diagnostics,
           std::string_view name) {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [&](const auto& d) {
                       return d.document_path.find(name) != std::string::npos;
                     });
}

class EditorHousehold : public testing::Test {
 protected:
  EditorDocument editor;
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("editor-household-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".json");
  void SetUp() override {
    const auto saved = saveLevelDocument(path, householdScene());
    ASSERT_TRUE(saved) << formatLevelDiagnostics(saved.diagnostics);
    ASSERT_TRUE(editor.open(path));
  }
  void TearDown() override { std::filesystem::remove(path); }
  EditorObjectId first(EditorHouseholdKind kind) const {
    return editor.householdIds(kind).front();
  }
  template <typename T>
  T value(EditorObjectId id) const {
    return std::get<T>(*editor.object(id));
  }
  template <typename Action>
  void unchangedFailure(Action action) {
    const auto document = *editor.document();
    const auto selected = editor.selection();
    const auto revision = editor.revision();
    const bool dirty = editor.dirty(), undo = editor.canUndo(),
               redo = editor.canRedo();
    EXPECT_FALSE(action());
    EXPECT_FALSE(editor.editError().empty());
    EXPECT_EQ(*editor.document(), document);
    EXPECT_EQ(editor.selection(), selected);
    EXPECT_EQ(editor.revision(), revision);
    EXPECT_EQ(editor.dirty(), dirty);
    EXPECT_EQ(editor.canUndo(), undo);
    EXPECT_EQ(editor.canRedo(), redo);
  }
};
}  // namespace

TEST_F(EditorHousehold, TypedHandlesSelectEveryRecordAndResetOnNewDocuments) {
  const auto original = *editor.document();
  const auto revision = editor.revision();
  std::set<EditorObjectId> handles;
  for (const auto kind :
       {EditorHouseholdKind::Box, EditorHouseholdKind::Document,
        EditorHouseholdKind::Radio}) {
    ASSERT_EQ(editor.householdIds(kind).size(), 2U);
    for (auto id : editor.householdIds(kind)) {
      EXPECT_TRUE(handles.insert(id).second);
      ASSERT_TRUE(editor.object(id));
      EXPECT_EQ(editorHouseholdKind(*editor.object(id)), kind);
      editor.select(id);
      EXPECT_EQ(editor.selection(), id);
    }
  }
  for (const auto& ids :
       {editor.solidIds(), editor.entryIds(), editor.propIds(),
        editor.characterIds(EditorCharacterKind::Actor),
        editor.audioIds(EditorAudioKind::Source)})
    for (auto id : ids) EXPECT_FALSE(handles.contains(id));
  EXPECT_EQ(*editor.document(), original);
  EXPECT_EQ(editor.revision(), revision);
  EXPECT_FALSE(editor.dirty());
  editor.requestNewInterior();
  EXPECT_EQ(editor.document()->household, LevelHousehold{});
  for (const auto kind :
       {EditorHouseholdKind::Box, EditorHouseholdKind::Document,
        EditorHouseholdKind::Radio})
    EXPECT_TRUE(editor.householdIds(kind).empty());
  EXPECT_EQ(editor.selection(), editor_no_object);
}

TEST_F(EditorHousehold,
       AddDuplicateAndRemoveUseOneHistoryEntryAndStableIdentity) {
  for (const auto kind :
       {EditorHouseholdKind::Box, EditorHouseholdKind::Document,
        EditorHouseholdKind::Radio}) {
    const auto before = *editor.document();
    const auto original_id = first(kind);
    editor.select(original_id);
    ASSERT_TRUE(editor.addHousehold(kind));
    const auto added_id = editor.selection();
    const auto added = *editor.object(added_id);
    EXPECT_NE(added_id, original_id);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.selection(), original_id);
    EXPECT_FALSE(editor.dirty());
    ASSERT_TRUE(editor.redo());
    EXPECT_EQ(editor.selection(), added_id);
    EXPECT_EQ(*editor.object(added_id), added);
    ASSERT_TRUE(editor.undo());
    ASSERT_TRUE(editor.duplicateSelected());
    const auto duplicate_id = editor.selection();
    const auto duplicate = *editor.object(duplicate_id);
    EXPECT_NE(duplicate_id, original_id);
    if (kind == EditorHouseholdKind::Document) {
      const auto& original = before.household.documents.front();
      const auto copy = std::get<HouseholdDocumentDefinition>(duplicate);
      EXPECT_NE(copy.id, original.id);
      EXPECT_EQ(copy.title, original.title);
      EXPECT_EQ(copy.pages, original.pages);
      EXPECT_EQ(copy.yaw_degrees, original.yaw_degrees);
    }
    if (kind == EditorHouseholdKind::Radio) {
      const auto& original = before.household.radios.front();
      const auto copy = std::get<HouseholdRadioDefinition>(duplicate);
      EXPECT_NE(copy.id, original.id);
      EXPECT_EQ(copy.prop, original.prop);
      EXPECT_EQ(copy.source, original.source);
      EXPECT_EQ(copy.initially_on, original.initially_on);
      EXPECT_FALSE(editor.valid());
      EXPECT_TRUE(field(editor.diagnostics(), "household.radios[2].source"));
      EXPECT_TRUE(field(editor.diagnostics(), "household.radios[2].prop"));
    }
    const auto duplicated = *editor.document();
    ASSERT_TRUE(editor.removeSelected());
    EXPECT_FALSE(editor.object(duplicate_id));
    EXPECT_EQ(*editor.document(), before);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), duplicated);
    EXPECT_EQ(editor.selection(), duplicate_id);
    EXPECT_EQ(*editor.object(duplicate_id), duplicate);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.selection(), original_id);
    EXPECT_FALSE(editor.dirty());
  }
}

TEST_F(EditorHousehold, CommittedRussianPagesRestoreTogetherAndRoundTrip) {
  const auto before = *editor.document();
  const auto id = first(EditorHouseholdKind::Document);
  editor.select(id);
  EditorPropertyEdit draft;
  draft.synchronize(editor);
  auto& note = std::get<HouseholdDocumentDefinition>(*draft.value());
  note.title = "Письмо: Ёж";
  std::swap(note.pages[0], note.pages[1]);
  note.pages[0] = "Изменённая страница.\nСледующая строка.";
  note.pages.push_back("Третья страница.");
  EXPECT_EQ(*editor.document(), before);
  const auto revision = editor.revision();
  ASSERT_TRUE(draft.commit(editor));
  EXPECT_EQ(editor.revision(), revision + 1);
  const auto committed = *editor.document();
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(*editor.document(), committed);
  ASSERT_TRUE(editor.save());
  const auto saved = bytes(path);
  EditorDocument reopened;
  ASSERT_TRUE(reopened.open(path));
  EXPECT_EQ(*reopened.document(), committed);
  EXPECT_FALSE(reopened.dirty());
  EXPECT_EQ(reopened.householdIds(EditorHouseholdKind::Document).size(), 2U);
  EXPECT_EQ(bytes(path), saved);
  auto edited = value<HouseholdDocumentDefinition>(id);
  edited.pages.erase(edited.pages.begin() + 1);
  ASSERT_TRUE(editor.replaceObject(id, edited));
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), committed);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorHousehold, TextAndTransformBoundsFailWithoutDiscardingRedo) {
  const auto id = first(EditorHouseholdKind::Document);
  editor.select(id);
  auto document = value<HouseholdDocumentDefinition>(id);
  document.title = repeated("Ё", 80);
  document.pages.assign(16, repeated("я", 480));
  ASSERT_TRUE(editor.replaceObject(id, document));
  EXPECT_TRUE(editor.valid());
  ASSERT_TRUE(editor.undo());
  ASSERT_TRUE(editor.canRedo());
  auto invalid = document;
  invalid.title += "ё";
  unchangedFailure([&] { return editor.replaceObject(id, invalid); });
  invalid = document;
  invalid.pages[3] += "я";
  unchangedFailure([&] { return editor.replaceObject(id, invalid); });
  EXPECT_NE(editor.editError().find("page 4"), std::string::npos);
  invalid = document;
  invalid.pages.push_back("Страница.");
  unchangedFailure([&] { return editor.replaceObject(id, invalid); });
  invalid = document;
  invalid.title = std::string(1, char(0xc0));
  unchangedFailure([&] { return editor.replaceObject(id, invalid); });
  invalid = document;
  invalid.position.x = std::numeric_limits<float>::max();
  unchangedFailure([&] { return editor.replaceObject(id, invalid); });
  const auto box_id = first(EditorHouseholdKind::Box);
  auto box = value<HouseholdBoxDefinition>(box_id);
  box.yaw_degrees = std::numeric_limits<float>::infinity();
  unchangedFailure([&] { return editor.replaceObject(box_id, box); });
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(value<HouseholdDocumentDefinition>(id), document);
}

TEST_F(EditorHousehold, SafeEmptyPagesAndBrokenRadioLinksRemainEditable) {
  const auto before = *editor.document();
  const auto document_id = first(EditorHouseholdKind::Document);
  auto note = value<HouseholdDocumentDefinition>(document_id);
  note.title.clear();
  note.pages.clear();
  ASSERT_TRUE(editor.replaceObject(document_id, note));
  EXPECT_FALSE(editor.valid());
  EXPECT_TRUE(field(editor.diagnostics(), "household.documents[0].title"));
  EXPECT_TRUE(field(editor.diagnostics(), "household.documents[0].pages"));
  EXPECT_FALSE(editor.save());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  const auto radio_id = first(EditorHouseholdKind::Radio);
  auto radio = value<HouseholdRadioDefinition>(radio_id);
  radio.prop = "missing-prop";
  radio.source = "missing-source";
  ASSERT_TRUE(editor.replaceObject(radio_id, radio));
  editor.select(radio_id);
  EXPECT_EQ(editor.selection(), radio_id);
  EXPECT_EQ(value<HouseholdRadioDefinition>(radio_id), radio);
  EXPECT_FALSE(editor.valid());
  EXPECT_FALSE(editor.save());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorHousehold, TypedIdentityChecksAndCapacityFailuresAreAtomic) {
  for (const auto kind :
       {EditorHouseholdKind::Box, EditorHouseholdKind::Document,
        EditorHouseholdKind::Radio}) {
    const auto id = first(kind);
    editor.select(id);
    auto duplicate = *editor.object(editor.householdIds(kind)[1]);
    unchangedFailure([&] { return editor.replaceObject(id, duplicate); });
  }
  const auto note_id = first(EditorHouseholdKind::Document);
  auto note = value<HouseholdDocumentDefinition>(note_id);
  note.id = editor.document()->household.boxes[0].id;
  ASSERT_TRUE(editor.replaceObject(note_id, note));
  EXPECT_TRUE(editor.valid());
  ASSERT_TRUE(editor.undo());
  const std::array limits{level_maximum_household_box_count,
                          level_maximum_household_document_count,
                          level_maximum_household_radio_count};
  for (std::size_t slot = 0; slot < limits.size(); ++slot) {
    const auto kind = static_cast<EditorHouseholdKind>(slot);
    while (editor.householdIds(kind).size() < limits[slot])
      ASSERT_TRUE(editor.addHousehold(kind));
    unchangedFailure([&] { return editor.addHousehold(kind); });
    unchangedFailure([&] { return editor.duplicateSelected(); });
  }
}

TEST_F(EditorHousehold, PropAndSourceRenamesUpdateAllConsumersInOneUndo) {
  const auto radio_id = first(EditorHouseholdKind::Radio);
  auto radio = value<HouseholdRadioDefinition>(radio_id);
  editor.select(radio_id);
  ASSERT_TRUE(editor.duplicateSelected());
  const auto duplicate_id = editor.selection();
  const auto actor_id = editor.characterIds(EditorCharacterKind::Actor).front();
  auto actor = value<CharacterActorDefinition>(actor_id);
  actor.footstep_source = radio.source;
  ASSERT_TRUE(editor.replaceObject(actor_id, actor));
  for (bool source : {false, true}) {
    const auto target = source
                            ? editor.audioIds(EditorAudioKind::Source).front()
                            : editor.propIds().front();
    editor.select(target);
    const auto before = *editor.document();
    auto renamed = *editor.object(target);
    std::visit(
        [](auto& v) {
          if constexpr (requires { v.id; }) v.id = "renamed";
        },
        renamed);
    ASSERT_TRUE(editor.replaceObject(target, renamed));
    for (auto id : {radio_id, duplicate_id}) {
      const auto after = value<HouseholdRadioDefinition>(id);
      EXPECT_EQ(source ? after.source : after.prop, "renamed");
    }
    if (source)
      EXPECT_EQ(editor.document()->characters.actors.front().footstep_source,
                "renamed");
    const auto after = *editor.document();
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.selection(), target);
    ASSERT_TRUE(editor.redo());
    EXPECT_EQ(*editor.document(), after);
    ASSERT_TRUE(editor.undo());
  }
}

TEST_F(EditorHousehold,
       DeletionRetainsBrokenLinksAndGeneratedIdsCannotReconnectThem) {
  for (bool source : {false, true}) {
    const auto before = *editor.document();
    const auto target = source
                            ? editor.audioIds(EditorAudioKind::Source).front()
                            : editor.propIds().front();
    editor.select(target);
    ASSERT_TRUE(editor.removeSelected());
    EXPECT_EQ(editor.document()->household, before.household);
    EXPECT_FALSE(editor.valid());
    ASSERT_TRUE(source ? editor.addAudio(EditorAudioKind::Source)
                       : editor.addProp("apartment-radio"));
    const auto added = *editor.object(editor.selection());
    std::visit(
        [&](const auto& v) {
          if constexpr (requires { v.id; })
            EXPECT_NE(v.id, source ? "source-1" : "prop-1");
        },
        added);
    EXPECT_EQ(editor.document()->household, before.household);
    EXPECT_FALSE(editor.valid());
    ASSERT_TRUE(editor.undo());
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.selection(), target);
    EXPECT_FALSE(editor.dirty());
  }
}

TEST_F(EditorHousehold, RadioDuplicateConflictsCanBeRepairedWithUnownedLinks) {
  const auto original = *editor.document();
  editor.select(first(EditorHouseholdKind::Radio));
  ASSERT_TRUE(editor.duplicateSelected());
  const auto control = editor.selection();
  EXPECT_FALSE(editor.valid());
  ASSERT_TRUE(editor.addProp("apartment-radio"));
  const auto prop = value<PrototypeStaticProp>(editor.selection());
  ASSERT_TRUE(editor.addAudio(EditorAudioKind::Source));
  const auto source = value<AudioSourceDefinition>(editor.selection());
  auto repaired = value<HouseholdRadioDefinition>(control);
  repaired.prop = prop.id;
  repaired.source = source.id;
  editor.select(control);
  ASSERT_TRUE(editor.replaceObject(control, repaired));
  EXPECT_TRUE(editor.valid()) << formatLevelDiagnostics(editor.diagnostics());
  ASSERT_TRUE(editor.undo());
  EXPECT_FALSE(editor.valid());
  ASSERT_TRUE(editor.redo());
  EXPECT_TRUE(editor.valid());
  EXPECT_EQ(editor.document()->household.radios.front(),
            original.household.radios.front());
}

TEST_F(EditorHousehold,
       LinkedResourceEditsRevalidateWithoutRewritingRadioDefinitions) {
  const auto before = *editor.document();
  const auto prop_id = editor.propIds().front();
  const auto source_id = editor.audioIds(EditorAudioKind::Source).front();
  for (int defect = 0; defect < 3; ++defect) {
    if (defect < 2) {
      auto prop = value<PrototypeStaticProp>(prop_id);
      if (defect == 0)
        prop.model = "apartment-chair";
      else
        prop.collision_boxes = {{{0, .05F, 0}, {.01F, .01F, .01F}}};
      ASSERT_TRUE(editor.replaceObject(prop_id, prop));
    } else {
      auto source = value<AudioSourceDefinition>(source_id);
      source.autoplay = true;
      ASSERT_TRUE(editor.replaceObject(source_id, source));
    }
    EXPECT_EQ(editor.document()->household, before.household);
    EXPECT_FALSE(editor.valid());
    EXPECT_TRUE(
        field(editor.diagnostics(), defect < 2 ? "household.radios[0].prop"
                                               : "household.radios[0].source"));
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_FALSE(editor.dirty());
  }
  auto prop = value<PrototypeStaticProp>(prop_id);
  prop.id = editor.document()->props[1].id;
  unchangedFailure([&] { return editor.replaceObject(prop_id, prop); });
  auto source = value<AudioSourceDefinition>(source_id);
  source.id = editor.document()->audio.sources[1].id;
  unchangedFailure([&] { return editor.replaceObject(source_id, source); });
}

TEST_F(EditorHousehold,
       AddRadioPrefersSelectedAvailableResourcesAndAvoidsOwnedSources) {
  editor.select(first(EditorHouseholdKind::Radio));
  ASSERT_TRUE(editor.removeSelected());
  const auto before = *editor.document();
  editor.select(editor.propIds().front());
  ASSERT_TRUE(editor.addHousehold(EditorHouseholdKind::Radio));
  const auto added = value<HouseholdRadioDefinition>(editor.selection());
  EXPECT_EQ(added.prop, "prop-1");
  EXPECT_EQ(added.source, "source-1");
  EXPECT_FALSE(added.initially_on);
  EXPECT_TRUE(editor.valid());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  ASSERT_TRUE(editor.addHousehold(EditorHouseholdKind::Radio));
  ASSERT_TRUE(editor.addHousehold(EditorHouseholdKind::Radio));
  const auto unavailable = value<HouseholdRadioDefinition>(editor.selection());
  EXPECT_EQ(unavailable.prop, "missing-prop");
  EXPECT_EQ(unavailable.source, "missing-source");
  EXPECT_FALSE(editor.valid());
}

TEST_F(EditorHousehold,
       UnrelatedEditsAndHistoryEvictionPreserveOtherHouseholdState) {
  const auto initial = *editor.document();
  const auto id = first(EditorHouseholdKind::Document);
  editor.select(id);
  for (int i = 0; i < 130; ++i) {
    auto note = value<HouseholdDocumentDefinition>(id);
    note.title = "Revision " + std::to_string(i);
    ASSERT_TRUE(editor.replaceObject(id, note));
  }
  for (int i = 0; i < 128; ++i) ASSERT_TRUE(editor.undo());
  EXPECT_FALSE(editor.canUndo());
  EXPECT_EQ(value<HouseholdDocumentDefinition>(id).title, "Revision 1");
  EXPECT_EQ(editor.document()->household.boxes, initial.household.boxes);
  EXPECT_EQ(editor.document()->household.radios, initial.household.radios);
  EXPECT_EQ(editor.document()->household.documents[1],
            initial.household.documents[1]);
  const auto household = editor.document()->household;
  const auto solid_id = editor.solidIds().front();
  auto solid = value<PrototypeSolid>(solid_id);
  solid.color = {20, 30, 40, 255};
  ASSERT_TRUE(editor.replaceObject(solid_id, solid));
  EXPECT_EQ(editor.document()->household, household);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.document()->household, household);
  EXPECT_EQ(editor.selection(), id);
}

TEST_F(EditorHousehold,
       PlacementCommandsPreserveIdentityAndRejectUnsuitableFaces) {
  EditorSurfaceHit hit{
      {2, 3, 4}, {0, 1, 0}, 1, editor_no_object, EditorSurfaceFace::Top};
  for (const auto kind :
       {EditorHouseholdKind::Box, EditorHouseholdKind::Document}) {
    const auto id = first(kind);
    editor.select(id);
    const auto before = *editor.document();
    ASSERT_TRUE(editor.placeSelected(hit, {}));
    EXPECT_EQ(editor.selection(), id);
    if (kind == EditorHouseholdKind::Box)
      EXPECT_FLOAT_EQ(value<HouseholdBoxDefinition>(id).center.y,
                      3 + household_box_half_extent);
    else
      EXPECT_FLOAT_EQ(value<HouseholdDocumentDefinition>(id).position.y,
                      3 + household_document_half_extent.y);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    auto wall = hit;
    wall.face = EditorSurfaceFace::NegativeX;
    wall.normal = {-1, 0, 0};
    EXPECT_FALSE(editor.placeSelected(wall, {}));
    EXPECT_EQ(*editor.document(), before);
  }
  editor.select(first(EditorHouseholdKind::Radio));
  const auto before = *editor.document();
  EXPECT_FALSE(editor.placeSelected(hit, {}));
  EXPECT_EQ(*editor.document(), before);
}
