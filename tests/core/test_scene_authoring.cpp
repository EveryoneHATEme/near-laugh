#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <limits>

#include "core/world/door.hpp"
#include "core/world/prototype_level.hpp"
#include "core/world/scene_assets.hpp"
#include "editor/editor_document.hpp"
#include "editor/editor_picking.hpp"
#include "prototype_level_fixture.hpp"

namespace {
class SceneAuthoring : public testing::Test {
 protected:
  void SetUp() override {
    root = std::filesystem::temp_directory_path() /
           ("near_laugh_scene_authoring_" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    editor.requestNewInterior();
  }
  void TearDown() override { std::filesystem::remove_all(root); }
  std::filesystem::path root;
  EditorDocument editor;
};
std::string readText(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}
void writeText(const std::filesystem::path& path, const std::string& text) {
  std::ofstream f(path, std::ios::binary);
  f << text;
}
}  // namespace

TEST_F(SceneAuthoring, EmptyInteriorSavesAndReopensWithoutUnusedPropsOrDoors) {
  ASSERT_TRUE(editor.valid());
  EXPECT_TRUE(editor.document()->props.empty());
  EXPECT_TRUE(editor.document()->doors.empty());
  ASSERT_TRUE(editor.saveAs(root / "empty.json"));
  const auto saved = *editor.document();
  ASSERT_TRUE(editor.open(root / "empty.json"));
  EXPECT_EQ(*editor.document(), saved);
  EXPECT_EQ(editor.sourceVersion(), 6U);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(SceneAuthoring,
       SharedModelsHaveIndependentIdentityAndUndoRestoresTheSameCopy) {
  ASSERT_TRUE(editor.addProp("apartment-chair"));
  const auto original_handle = editor.selection();
  const auto original =
      std::get<PrototypeStaticProp>(*editor.object(original_handle));
  ASSERT_TRUE(editor.duplicateSelected());
  const auto copy_handle = editor.selection();
  const auto copy = std::get<PrototypeStaticProp>(*editor.object(copy_handle));
  EXPECT_NE(copy.id, original.id);
  EXPECT_EQ(copy.model, original.model);
  EXPECT_EQ(copy.collision_boxes, original.collision_boxes);
  EXPECT_NE(copy.translation, original.translation);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.selection(), original_handle);
  EXPECT_FALSE(editor.object(copy_handle));
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(editor.selection(), copy_handle);
  EXPECT_EQ(std::get<PrototypeStaticProp>(*editor.object(copy_handle)), copy);
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_EQ(editor.document()->props.size(), 1U);
  EXPECT_EQ(std::get<PrototypeStaticProp>(*editor.object(original_handle)),
            original);
  editor.select(original_handle);
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_TRUE(editor.document()->props.empty());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.selection(), original_handle);
  EXPECT_EQ(std::get<PrototypeStaticProp>(*editor.object(original_handle)),
            original);
}

TEST_F(SceneAuthoring, RenamingAndChangingModelsPreservesProxyOwnership) {
  ASSERT_TRUE(editor.addProp("apartment-chair"));
  auto prop = std::get<PrototypeStaticProp>(*editor.object(editor.selection()));
  const auto boxes = prop.collision_boxes;
  prop.id = "room-chair";
  prop.model = "apartment-radio";
  ASSERT_TRUE(editor.replaceObject(editor.selection(), prop));
  EXPECT_EQ(editor.document()->props.front().collision_boxes, boxes);
  ASSERT_TRUE(editor.duplicateSelected());
  auto copy = std::get<PrototypeStaticProp>(*editor.object(editor.selection()));
  copy.id = prop.id;
  EXPECT_FALSE(editor.replaceObject(editor.selection(), copy));
  EXPECT_NE(editor.document()->props.back().id, prop.id);
  copy.id = "INVALID";
  EXPECT_FALSE(editor.replaceObject(editor.selection(), copy));
}

TEST_F(SceneAuthoring, DecorativeAndMissingModelPlacementsRemainSelectable) {
  ASSERT_TRUE(editor.addProp("apartment-phone"));
  const auto id = editor.selection();
  auto prop = std::get<PrototypeStaticProp>(*editor.object(id));
  prop.translation = {2, 1, -1};
  ASSERT_TRUE(editor.replaceObject(id, prop));
  EXPECT_TRUE(editor.document()->props.front().collision_boxes.empty());
  EXPECT_EQ(pickEditorObject(editor, {{2, 1.03F, 0}, {0, 0, -1}}), id);
  prop.model = "missing-model";
  ASSERT_TRUE(editor.replaceObject(id, prop));
  EXPECT_FALSE(editor.valid());
  EXPECT_EQ(pickEditorObject(editor, {{2, 1, 0}, {0, 0, -1}}), id);
  EXPECT_FALSE(editor.saveAs(root / "invalid.json"));
  EXPECT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
}

TEST_F(SceneAuthoring, OverflowingModelOrYawedProxyEditsPreserveTheDocument) {
  ASSERT_TRUE(editor.addProp("apartment-phone"));
  const auto id = editor.selection();
  const auto original = std::get<PrototypeStaticProp>(*editor.object(id));
  ASSERT_TRUE(original.collision_boxes.empty());
  ASSERT_TRUE(editor.saveAs(root / "safe.json"));
  const auto saved = *editor.document();
  const auto source = readText(root / "safe.json");
  const auto revision = editor.revision();
  for (const bool mesh : {false, true}) {
    auto invalid = original;
    if (mesh) {
      invalid.model = "prototype-chair";
      invalid.uniform_scale = std::numeric_limits<float>::max();
    } else {
      const float extent = std::numeric_limits<float>::max() * .49F;
      invalid.yaw_degrees = 45;
      invalid.collision_boxes = {{{0, 1, 0}, {extent, .5F, extent}}};
    }
    EXPECT_FALSE(prototypeStaticPropIsValid(invalid));
    EXPECT_FALSE(editor.replaceObject(id, invalid));
    EXPECT_EQ(*editor.document(), saved);
    EXPECT_EQ(editor.revision(), revision);
    EXPECT_EQ(editor.selection(), id);
    EXPECT_FALSE(editor.dirty());
    EXPECT_EQ(readText(root / "safe.json"), source);
  }
  auto unknown = original;
  unknown.model = "missing-model";
  ASSERT_TRUE(editor.replaceObject(id, unknown));
  EXPECT_FALSE(editor.valid());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), saved);
}

TEST_F(SceneAuthoring,
       MaterialsAreIndependentAndUnknownReferencesRemainRepairable) {
  const auto id = editor.solidIds().front();
  auto solid = std::get<PrototypeSolid>(*editor.object(id));
  solid.material = "wallpaper";
  ASSERT_TRUE(editor.replaceObject(id, solid));
  EXPECT_TRUE(editor.valid());
  solid.kind = PrototypeSolidKind::Obstacle;
  ASSERT_TRUE(editor.replaceObject(id, solid));
  EXPECT_EQ(editor.document()->solids.front().material, "wallpaper");
  EXPECT_TRUE(editor.valid());
  solid.material = "unknown";
  ASSERT_TRUE(editor.replaceObject(id, solid));
  EXPECT_FALSE(editor.valid());
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
}

TEST_F(SceneAuthoring,
       TerrainMaterialAndSculptingShareHistoryWithoutLosingContent) {
  ASSERT_TRUE(editor.open(packagedPrototypeLevelPath()));
  const auto original = *editor.document();
  ASSERT_TRUE(editor.setTerrainMaterial("wood-floor"));
  EXPECT_EQ(editor.document()->terrain->material, "wood-floor");
  const auto p =
      prototypeTerrainSamplePosition(*editor.document()->terrain, 20, 20);
  editor.beginTerrainStroke(p);
  ASSERT_TRUE(editor.finishTerrainStroke());
  EXPECT_EQ(editor.document()->terrain->material, "wood-floor");
  EXPECT_EQ(editor.document()->props, original.props);
  EXPECT_EQ(editor.document()->doors, original.doors);
  ASSERT_TRUE(editor.undo());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(SceneAuthoring, DoorIdentityAndInvalidInitialStateSurviveHistory) {
  ASSERT_TRUE(editor.addDoor());
  const auto id = editor.selection();
  auto door = std::get<DoorDefinition>(*editor.object(id));
  door.id = "lena-room";
  door.hinge_position = {-2, .02F, -1};
  ASSERT_TRUE(editor.replaceObject(id, door));
  ASSERT_TRUE(editor.valid()) << formatLevelDiagnostics(editor.diagnostics());
  ASSERT_TRUE(editor.saveAs(root / "door.json"));
  door.initially_open = true;
  door.initially_locked = true;
  ASSERT_TRUE(editor.replaceObject(id, door));
  EXPECT_FALSE(editor.valid());
  EXPECT_FALSE(editor.save());
  EXPECT_TRUE(std::get<DoorDefinition>(*editor.object(id)).initially_locked);
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.duplicateSelected());
  const auto copy_handle = editor.selection();
  const auto copy_id = std::get<DoorDefinition>(*editor.object(copy_handle)).id;
  ASSERT_TRUE(editor.undo());
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(std::get<DoorDefinition>(*editor.object(copy_handle)).id, copy_id);
  EXPECT_EQ(editor.selection(), copy_handle);
  ASSERT_TRUE(editor.removeSelected());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.selection(), copy_handle);
}

TEST_F(SceneAuthoring, DoorPlacementUsesBottomHingeAndRefusesWallOrUnderside) {
  ASSERT_TRUE(editor.addDoor());
  const auto door =
      std::get<DoorDefinition>(*editor.object(editor.selection()));
  const EditorSurfaceHit hit{
      {1, 3, 4}, {0, 1, 0}, 1, editor_no_object, EditorSurfaceFace::Top};
  const auto placed = editorPlacedObject(door, hit, {});
  ASSERT_TRUE(placed);
  const auto result = std::get<DoorDefinition>(*placed);
  EXPECT_FLOAT_EQ(result.hinge_position.y, 3.02F);
  EXPECT_EQ(result.closed_yaw_degrees, door.closed_yaw_degrees);
  EXPECT_EQ(result.open_angle_degrees, door.open_angle_degrees);
  auto wall = hit;
  wall.face = EditorSurfaceFace::NegativeX;
  wall.normal = {-1, 0, 0};
  EXPECT_FALSE(editorPlacedObject(door, wall, {}));
  auto underside = hit;
  underside.face = EditorSurfaceFace::Bottom;
  EXPECT_FALSE(editorPlacedObject(door, underside, {}));
}

TEST_F(SceneAuthoring, AllLegacyVersionsNormalizeWithoutChangingSourceBytes) {
  for (int version : {2, 3, 4, 5}) {
    std::string source =
        readText("tests/fixtures/levels/prototype-v" +
                 std::to_string(version == 2 ? 3 : version) + ".level.json");
    if (version == 2) {
      source.replace(source.find("\"version\": 3"), 12, "\"version\": 2");
      source.erase(source.find(",\n  \"light_switch\""));
      source += "\n}\n";
    }
    const auto path = root / (std::to_string(version) + ".json");
    writeText(path, source);
    ASSERT_TRUE(editor.open(path));
    EXPECT_EQ(editor.sourceVersion(), static_cast<unsigned>(version));
    ASSERT_EQ(editor.document()->props.size(), 1U);
    EXPECT_EQ(editor.document()->props.front().model, "prototype-chair");
    EXPECT_TRUE(editor.document()->doors.empty());
    EXPECT_EQ(readText(path), source);
    EXPECT_FALSE(editor.dirty());
    ASSERT_TRUE(editor.save());
    EXPECT_EQ(loadLevelDocument(path).source_version, 6U);
  }
}

TEST_F(SceneAuthoring, MultipleProxyBoxesGateAllEntriesAndInitialDoors) {
  auto level = *editor.document();
  level.props.push_back({"table", "apartment-table", {0, 0, 0}, 0, 1, {}});
  EXPECT_TRUE(validateLevelDocument(level).empty());
  level.props.front().collision_boxes.push_back({{0, .9F, 2}, {.5F, .9F, .5F}});
  EXPECT_FALSE(validateLevelDocument(level).empty());
  level.props.front().collision_boxes.front().center = {-2, .9F, -1};
  DoorDefinition door;
  door.id = "test";
  door.hinge_position = {-2, .02F, -1};
  level.doors.push_back(door);
  EXPECT_FALSE(validateLevelDocument(level).empty());
  level.props.front().collision_boxes.clear();
  EXPECT_TRUE(validateLevelDocument(level).empty());
  level.props.front().uniform_scale = std::numeric_limits<float>::max();
  EXPECT_FALSE(validateLevelDocument(level).empty());
}
