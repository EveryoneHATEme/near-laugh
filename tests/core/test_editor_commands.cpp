#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>

#include "editor/editor_document.hpp"

namespace {
class EditorCommands : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(editor.open("resources/levels/prototype.level.json"));
    root = std::filesystem::temp_directory_path() /
           ("near_laugh_commands_" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
  }
  void TearDown() override { std::filesystem::remove_all(root); }
  EditorDocument editor;
  std::filesystem::path root;
};

std::string bytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}
}  // namespace

TEST_F(EditorCommands, SelectionAndIdentitySurviveInsertionRemovalUndoAndOpen) {
  const auto original = *editor.document();
  const auto ids = editor.solidIds();
  ASSERT_GT(ids.size(), 1U);
  editor.select(ids[1]);
  ASSERT_TRUE(editor.duplicateSelected());
  const auto duplicate = editor.selection();
  EXPECT_NE(duplicate, ids[1]);
  editor.select(ids[0]);
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_EQ(editor.object(ids[1]), EditorObjectValue(original.solids[1]));
  EXPECT_TRUE(editor.object(duplicate));
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.selection(), ids[0]);
  EXPECT_EQ(editor.solidIds()[0], ids[0]);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.selection(), ids[1]);
  EXPECT_EQ(*editor.document(), original);
  EXPECT_EQ(editor.solidIds(), ids);
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(editor.selection(), duplicate);
  ASSERT_TRUE(editor.open("resources/levels/prototype.level.json"));
  EXPECT_EQ(editor.solidIds(), ids);
  EXPECT_EQ(editor.selection(), editor_no_object);
  EXPECT_FALSE(editor.canUndo());
  EXPECT_FALSE(editor.canRedo());
  EXPECT_EQ(*editor.document(), original);
}

TEST_F(EditorCommands, AllPropertiesAreUndoableAndPersistSemantically) {
  const auto original = *editor.document();
  const auto id = editor.solidIds()[0];
  auto solid = original.solids[0];
  solid.center.x += 0.1F;
  solid.half_extent.y += 0.1F;
  solid.color = {20, 30, 40, 255};
  solid.kind = PrototypeSolidKind::Obstacle;
  solid.surface = PrototypeSurface::Obstacle;
  ASSERT_TRUE(editor.replaceObject(id, solid));
  auto spawn = original.player_spawn;
  spawn.yaw_degrees += 35.0F;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  for (EditorObjectId light_id : {editor_first_light, editor_first_light + 1}) {
    auto light = std::get<PrototypePointLight>(*editor.object(light_id));
    light.position.x += 0.25F;
    light.color = {0.0F, 0.5F, 0.7F};
    light.intensity += 0.25F;
    light.radius += 1.0F;
    ASSERT_TRUE(editor.replaceObject(light_id, light));
  }
  auto prop = original.static_prop;
  prop.translation.x += 0.1F;
  prop.yaw_degrees = 45.0F;
  prop.uniform_scale = 0.75F;
  prop.box_proxy_center = {-0.1F, 0.6F, 0.0F};
  prop.box_proxy_half_extent = {0.3F, 0.6F, 0.3F};
  ASSERT_TRUE(editor.replaceObject(editor_prop, prop));
  ASSERT_TRUE(editor.valid()) << formatLevelDiagnostics(editor.diagnostics());
  const auto edited = *editor.document();
  ASSERT_TRUE(editor.saveAs(root / "edited.json"));
  const auto loaded = loadLevelDocument(root / "edited.json");
  ASSERT_TRUE(loaded);
  EXPECT_EQ(*loaded.document, edited);
  ASSERT_TRUE(saveLevelDocument(root / "again.json", *loaded.document));
  EXPECT_EQ(bytes(root / "edited.json"), bytes(root / "again.json"));
  for (int i = 0; i < 5; ++i) ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_TRUE(editor.dirty());
  for (int i = 0; i < 5; ++i) ASSERT_TRUE(editor.redo());
  EXPECT_EQ(*editor.document(), edited);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorCommands,
       RejectsInvalidFieldsWithoutRevisionSelectionOrHistoryChanges) {
  const auto original = *editor.document();
  const auto revision = editor.revision();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float inf = std::numeric_limits<float>::infinity();
  const auto solid_id = editor.solidIds()[0];
  editor.select(solid_id);
  const auto reject = [&](EditorObjectId id, EditorObjectValue value) {
    EXPECT_FALSE(editor.replaceObject(id, value));
    EXPECT_FALSE(editor.editError().empty());
    EXPECT_EQ(*editor.document(), original);
    EXPECT_EQ(editor.revision(), revision);
    EXPECT_EQ(editor.selection(), solid_id);
    EXPECT_FALSE(editor.dirty());
    EXPECT_FALSE(editor.canUndo());
  };
  for (int field = 0; field < 5; ++field) {
    auto s = original.solids[0];
    if (field == 0) s.center.z = nan;
    if (field == 1) s.half_extent.x = 0;
    if (field == 2) s.half_extent.y = inf;
    if (field == 3) s.kind = static_cast<PrototypeSolidKind>(100);
    if (field == 4) s.surface = static_cast<PrototypeSurface>(100);
    reject(solid_id, s);
  }
  for (int field = 0; field < 2; ++field) {
    auto s = original.player_spawn;
    if (field == 0) s.foot_position.x = inf;
    if (field == 1) s.yaw_degrees = nan;
    reject(editor_spawn, s);
  }
  for (int field = 0; field < 5; ++field) {
    auto l = original.environment_light.point_lights[0];
    if (field == 0) l.position.y = nan;
    if (field == 1) l.color[0] = -1;
    if (field == 2) l.color[2] = inf;
    if (field == 3) l.intensity = 0;
    if (field == 4) l.radius = -1;
    reject(editor_first_light, l);
  }
  for (int field = 0; field < 7; ++field) {
    auto p = original.static_prop;
    if (field == 0) p.translation.x = nan;
    if (field == 1) p.yaw_degrees = inf;
    if (field == 2) p.uniform_scale = 0;
    if (field == 3) p.box_proxy_center.z = nan;
    if (field == 4) p.box_proxy_half_extent.y = -1;
    if (field == 5) p.surface = PrototypeSurface::Floor;
    if (field == 6) p.uniform_scale = std::numeric_limits<float>::max();
    reject(editor_prop, p);
  }
  reject(editor_spawn, original.solids[0]);
  reject(999999, original.player_spawn);
}

TEST_F(EditorCommands, InvalidGameplayRemainsEditableButSaveIsGated) {
  const auto original = *editor.document();
  auto solid = original.solids[0];
  solid.center = original.player_spawn.foot_position;
  solid.center.y += 0.5F;
  solid.half_extent = {1, 1, 1};
  ASSERT_TRUE(editor.replaceObject(editor.solidIds()[0], solid));
  EXPECT_FALSE(editor.valid());
  EXPECT_TRUE(editor.dirty());
  EXPECT_FALSE(editor.diagnostics().empty());
  EXPECT_FALSE(editor.saveAs(root / "invalid.json"));
  EXPECT_FALSE(std::filesystem::exists(root / "invalid.json"));
  EXPECT_EQ(editor.document()->solids[0], solid);
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
  EXPECT_FALSE(editor.dirty());
  EXPECT_TRUE(editor.diagnostics().empty());
  EXPECT_TRUE(editor.saveAs(root / "valid.json"));
}

TEST_F(EditorCommands,
       SavedRevisionSurvivesUndoAndBranchesNeverBecomeFalselyClean) {
  auto spawn = editor.document()->player_spawn;
  spawn.yaw_degrees += 1;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  ASSERT_TRUE(editor.saveAs(root / "saved.json"));
  spawn.yaw_degrees += 1;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  ASSERT_TRUE(editor.undo());
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.dirty());
  spawn.yaw_degrees += 2;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  EXPECT_TRUE(editor.dirty());
  EXPECT_FALSE(editor.canRedo());
  const auto revision = editor.revision();
  EXPECT_FALSE(editor.replaceObject(editor_spawn, spawn));
  EXPECT_EQ(editor.revision(), revision);
}

TEST_F(EditorCommands,
       HistoryRetainsNewest128OperationsAndRefreshesPreviewOnUndoRedo) {
  auto spawn = editor.document()->player_spawn;
  const float initial_yaw = spawn.yaw_degrees;
  for (int i = 0; i < 130; ++i) {
    spawn.yaw_degrees += 1;
    ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  }
  auto generation = editor.revision();
  for (int i = 0; i < 128; ++i) {
    ASSERT_TRUE(editor.undo());
    EXPECT_GT(editor.revision(), generation);
    generation = editor.revision();
  }
  EXPECT_FALSE(editor.undo());
  EXPECT_FLOAT_EQ(editor.document()->player_spawn.yaw_degrees, initial_yaw + 2);
  for (int i = 0; i < 128; ++i) ASSERT_TRUE(editor.redo());
  EXPECT_FALSE(editor.redo());
  EXPECT_FLOAT_EQ(editor.document()->player_spawn.yaw_degrees,
                  initial_yaw + 130);
}

TEST_F(EditorCommands, SolidCountBoundAndFixedObjectsAreProtected) {
  for (const auto id : {editor_spawn, editor_first_light,
                        editor_first_light + 1, editor_prop}) {
    editor.select(id);
    EXPECT_FALSE(editor.removeSelected());
    EXPECT_FALSE(editor.duplicateSelected());
  }
  EXPECT_FALSE(editor.dirty());
  const PrototypeSolid solid{
      {10, 1, 10}, {0.2F, 0.2F, 0.2F}, {255, 255, 255, 255}};
  while (editor.solidIds().size() < level_maximum_solid_count)
    ASSERT_TRUE(editor.addSolid(solid));
  EXPECT_FALSE(editor.addSolid(solid));
  EXPECT_FALSE(editor.duplicateSelected());
  ASSERT_TRUE(editor.removeSelected());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.solidIds().size(), level_maximum_solid_count);
}
