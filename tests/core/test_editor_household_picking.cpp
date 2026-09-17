#include <gtest/gtest.h>

#include <chrono>

#include "core/world/household.hpp"
#include "editor/editor_picking.hpp"

namespace {
class EditorHouseholdPicking : public testing::Test {
 protected:
  EditorDocument editor;
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("household-picking-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".json");
  void SetUp() override {
    LevelDocument d;
    d.solids = {{{0, -.25F, 0},
                 {10, .25F, 10},
                 {255, 255, 255, 255},
                 PrototypeSolidKind::Floor},
                {{0, 3.8F, 0},
                 {2, .2F, 2},
                 {255, 255, 255, 255},
                 PrototypeSolidKind::Floor}};
    d.entries = {{"default", {{-6, 0, 0}, 0}}};
    d.default_entry = "default";
    d.household.boxes = {{"box", {0, 4.15F, 0}, 45}};
    d.household.documents = {
        {"note", {1, 4.003F, 0}, 31, "Записка", {"Текст"}}};
    d.props = {{"receiver", "apartment-radio", {4, 2, 0}, 25, 1, {}}};
    d.audio.cues = {
        {"loop", "radio", "radio", AudioCueKind::Ambience, true, true}};
    d.audio.sources = {{"radio-loop", "loop", {7, 0, 7}}};
    d.household.radios = {{"radio", "receiver", "radio-loop", false}};
    const auto saved = saveLevelDocument(path, d);
    ASSERT_TRUE(saved) << formatLevelDiagnostics(saved.diagnostics);
    ASSERT_TRUE(editor.open(path));
  }
  void TearDown() override { std::filesystem::remove(path); }
  EditorObjectId id(EditorHouseholdKind kind) {
    return editor.householdIds(kind).front();
  }
};
}  // namespace

TEST_F(EditorHouseholdPicking, BoxesAndDocumentsUseTheirInitialVisibleBounds) {
  const auto original = *editor.document();
  EXPECT_EQ(pickEditorObject(editor, {{0, 4.15F, -2}, {0, 0, 2}}),
            id(EditorHouseholdKind::Box));
  EXPECT_EQ(pickEditorObject(editor, {{1, 6, 0}, {0, -1, 0}}),
            id(EditorHouseholdKind::Document));
  EXPECT_EQ(pickEditorObject(editor, {{.22F, 4.15F, -2}, {0, 0, 1}}),
            editor_no_object);
  EXPECT_EQ(*editor.document(), original);
}

TEST_F(EditorHouseholdPicking, ControlIndicatorAndPropHaveDistinctSelection) {
  const auto& prop = editor.document()->props.front();
  const auto indicator = householdRadioPresentation(prop, false);
  const auto bounds = householdRadioBounds(prop);
  EXPECT_EQ(
      pickEditorObject(
          editor, {{indicator.center[0], indicator.center[1], -2}, {0, 0, 1}}),
      id(EditorHouseholdKind::Radio));
  EXPECT_EQ(pickEditorObject(
                editor, {{bounds.center[0], bounds.center[1], -2}, {0, 0, 1}}),
            editor.propIds().front());
  auto broken = editor.document()->household.radios.front();
  broken.prop = "missing-prop";
  ASSERT_TRUE(editor.replaceObject(id(EditorHouseholdKind::Radio), broken));
  EXPECT_NE(
      pickEditorObject(
          editor, {{indicator.center[0], indicator.center[1], -2}, {0, 0, 1}}),
      id(EditorHouseholdKind::Radio));
  editor.select(id(EditorHouseholdKind::Radio));
  EXPECT_TRUE(editor.object(editor.selection()));
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(
      pickEditorObject(
          editor, {{indicator.center[0], indicator.center[1], -2}, {0, 0, 1}}),
      id(EditorHouseholdKind::Radio));
}

TEST_F(EditorHouseholdPicking,
       UpperFloorAndLowerFloorPlacementUseNearestSurface) {
  for (const auto kind :
       {EditorHouseholdKind::Box, EditorHouseholdKind::Document}) {
    editor.select(id(kind));
    const auto original = *editor.object(editor.selection());
    const auto top = pickEditorSurface(editor, {{-.7F, 6, 0}, {0, -1, 0}},
                                       EditorPlacementMode::SceneSurfaces);
    ASSERT_TRUE(top);
    EXPECT_EQ(top->face, EditorSurfaceFace::Top);
    EXPECT_FLOAT_EQ(top->position.y, 4);
    ASSERT_TRUE(editor.placeSelected(*top, {}));
    auto placed = *editor.object(editor.selection());
    if (kind == EditorHouseholdKind::Box)
      EXPECT_FLOAT_EQ(std::get<HouseholdBoxDefinition>(placed).center.y, 4.15F);
    else
      EXPECT_FLOAT_EQ(std::get<HouseholdDocumentDefinition>(placed).position.y,
                      4.003F);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.object(editor.selection()), original);
    const auto lower = pickEditorSurface(editor, {{-.7F, 3, 0}, {0, -1, 0}},
                                         EditorPlacementMode::SceneSurfaces);
    ASSERT_TRUE(lower);
    EXPECT_FLOAT_EQ(lower->position.y, 0);
    ASSERT_TRUE(editor.placeSelected(*lower, {}));
    placed = *editor.object(editor.selection());
    if (kind == EditorHouseholdKind::Box)
      EXPECT_FLOAT_EQ(std::get<HouseholdBoxDefinition>(placed).center.y, .15F);
    else
      EXPECT_FLOAT_EQ(std::get<HouseholdDocumentDefinition>(placed).position.y,
                      .003F);
    ASSERT_TRUE(editor.undo());
  }
}

TEST_F(EditorHouseholdPicking, UnsuitableNearerFaceDoesNotFallThroughOrCommit) {
  editor.select(id(EditorHouseholdKind::Box));
  const auto before = *editor.document();
  const auto revision = editor.revision();
  const auto underside = pickEditorSurface(editor, {{0, 3, 0}, {0, 1, 0}},
                                           EditorPlacementMode::SceneSurfaces);
  ASSERT_TRUE(underside);
  EXPECT_EQ(underside->face, EditorSurfaceFace::Bottom);
  EXPECT_FALSE(editor.placeSelected(*underside, {}));
  EXPECT_EQ(*editor.document(), before);
  EXPECT_EQ(editor.revision(), revision);
  EXPECT_FALSE(updateEditorPlacementViewport(
      editor, EditorRay{{0, 6, 0}, {0, -1, 0}}, true, false, true,
      EditorPlacementMode::SceneSurfaces, {}));
  EXPECT_EQ(*editor.document(), before);
}
