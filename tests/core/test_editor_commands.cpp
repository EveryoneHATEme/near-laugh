#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

#include "core/physics/physics_world.hpp"
#include "core/render/prototype_scene.hpp"
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

TEST_F(EditorCommands, TerrainGestureUsesPressSettingsAndOneMixedHistoryEntry) {
  const auto original = *editor.document();
  editor.select(editor_prop);
  const auto object_generation = editor.objectRevision();
  EditorTerrainBrush brush;
  brush.strength = 0.01F;
  ASSERT_TRUE(editor.setTerrainBrush(brush));
  editor.beginTerrainStroke({{10, 0, 10}});
  EXPECT_TRUE(editor.dirty());
  EXPECT_FALSE(editor.canUndo());
  const auto preview_generation = editor.revision();
  auto other = brush;
  other.mode = EditorBrushMode::Lower;
  other.strength = 1;
  ASSERT_TRUE(editor.setTerrainBrush(other));
  EXPECT_EQ(editor.terrainBrush(), brush);
  editor.extendTerrainStroke({{11, 0, 10}});
  EXPECT_GT(editor.revision(), preview_generation);
  EXPECT_EQ(editor.objectRevision(), object_generation);
  ASSERT_TRUE(editor.finishTerrainStroke());
  const auto sculpted = *editor.document();
  EXPECT_EQ(sculpted.solids, original.solids);
  EXPECT_EQ(sculpted.environment_light, original.environment_light);
  EXPECT_EQ(sculpted.player_spawn, original.player_spawn);
  EXPECT_EQ(sculpted.static_prop, original.static_prop);
  auto expected = original.terrain;
  EditorTerrainStroke replay;
  replay.brush = brush;
  ASSERT_TRUE(replay.advance(expected, {{10, 0, 10}}));
  ASSERT_TRUE(replay.advance(expected, {{11, 0, 10}}));
  EXPECT_EQ(sculpted.terrain, expected);
  ASSERT_TRUE(editor.valid()) << formatLevelDiagnostics(editor.diagnostics());
  const auto vertices = buildPrototypeSceneVertices(sculpted.terrain, {});
  const auto original_vertices =
      buildPrototypeSceneVertices(original.terrain, {});
  bool changed_normal = false, changed_height = false;
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    changed_height |=
        vertices[i].position[1] != original_vertices[i].position[1];
    changed_normal |= vertices[i].normal[1] != original_vertices[i].normal[1];
    const auto& n = vertices[i].normal;
    EXPECT_NEAR(n[0] * n[0] + n[1] * n[1] + n[2] * n[2], 1, 1e-5);
  }
  EXPECT_TRUE(changed_height);
  EXPECT_TRUE(changed_normal);

  auto spawn = original.player_spawn;
  spawn.yaw_degrees += 10;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  const auto mixed = *editor.document();
  ASSERT_TRUE(editor.saveAs(root / "sculpted.json"));
  const auto loaded = loadLevelDocument(root / "sculpted.json");
  ASSERT_TRUE(loaded);
  EXPECT_EQ(*loaded.document, mixed);
  ASSERT_TRUE(saveLevelDocument(root / "again.json", *loaded.document));
  EXPECT_EQ(bytes(root / "sculpted.json"), bytes(root / "again.json"));
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), sculpted);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_EQ(editor.selection(), editor_prop);
  EXPECT_FALSE(editor.canUndo());
  EXPECT_TRUE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(*editor.document(), mixed);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorCommands, EmptyStationaryAndZeroSmoothGesturesDoNotAddHistory) {
  const auto original = *editor.document();
  const auto generation = editor.revision();
  editor.beginTerrainStroke(std::nullopt);
  editor.extendTerrainStroke(std::nullopt);
  EXPECT_FALSE(editor.finishTerrainStroke());
  EditorTerrainBrush brush;
  brush.mode = EditorBrushMode::Smooth;
  brush.smooth_strength = 0;
  ASSERT_TRUE(editor.setTerrainBrush(brush));
  editor.beginTerrainStroke({{10, 0, 10}});
  editor.extendTerrainStroke({{20, 0, 10}});
  EXPECT_FALSE(editor.finishTerrainStroke());
  EXPECT_EQ(editor.revision(), generation);
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.canUndo());
  EXPECT_FALSE(editor.dirty());

  brush.mode = EditorBrushMode::Raise;
  ASSERT_TRUE(editor.setTerrainBrush(brush));
  editor.beginTerrainStroke({{10, 0, 10}});
  const auto first_stamp = *editor.document();
  for (int frame = 0; frame < 200; ++frame)
    editor.extendTerrainStroke({{10, 0, 10}});
  EXPECT_EQ(*editor.document(), first_stamp);
  ASSERT_TRUE(editor.finishTerrainStroke());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
  EXPECT_FALSE(editor.canUndo());
}

TEST_F(EditorCommands, TerrainDiagnosticsIdentifySlopeCellsAndRefreshOnRepair) {
  const auto original = *editor.document();
  EditorTerrainBrush brush;
  brush.radius = 0.5F;
  brush.strength = 1;
  ASSERT_TRUE(editor.setTerrainBrush(brush));
  const auto& terrain = original.terrain;
  const auto hit = prototypeTerrainSamplePosition(terrain, 40, 40);
  editor.beginTerrainStroke(hit);
  ASSERT_TRUE(editor.finishTerrainStroke());
  EXPECT_FALSE(editor.valid());
  EXPECT_TRUE(editor.dirty());
  unsigned triangles = 0;
  for (const auto& d : editor.diagnostics()) {
    if (d.terrain_location && d.terrain_location->triangle) {
      EXPECT_GE(d.terrain_location->x, 39U);
      EXPECT_LE(d.terrain_location->x, 40U);
      EXPECT_GE(d.terrain_location->z, 39U);
      EXPECT_LE(d.terrain_location->z, 40U);
      triangles |= 1U << *d.terrain_location->triangle;
    }
  }
  EXPECT_EQ(triangles, 3U);
  EXPECT_FALSE(editor.saveAs(root / "invalid.json"));
  EXPECT_FALSE(std::filesystem::exists(root / "invalid.json"));
  editor.requestClose();
  EXPECT_EQ(editor.pendingAction().kind, EditorPendingActionKind::Close);
  ASSERT_TRUE(editor.resolvePending(EditorPendingDecision::Cancel));
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
  EXPECT_TRUE(editor.diagnostics().empty());
  ASSERT_TRUE(editor.redo());
  EXPECT_FALSE(editor.valid());
  brush.mode = EditorBrushMode::Lower;
  ASSERT_TRUE(editor.setTerrainBrush(brush));
  editor.beginTerrainStroke(hit);
  ASSERT_TRUE(editor.finishTerrainStroke());
  EXPECT_TRUE(editor.valid()) << formatLevelDiagnostics(editor.diagnostics());

  auto invalid = original;
  invalid.terrain.heights[40 * prototype_terrain_sample_count + 41] = NAN;
  const auto diagnostics = validateLevelDocument(invalid);
  ASSERT_FALSE(diagnostics.empty());
  ASSERT_TRUE(diagnostics.front().terrain_location);
  EXPECT_EQ(diagnostics.front().terrain_location->x, 41U);
  EXPECT_EQ(diagnostics.front().terrain_location->z, 40U);
  EXPECT_FALSE(diagnostics.front().terrain_location->triangle);
}

TEST_F(EditorCommands, TerrainSpawnSupportAndCloseRefreshEvenDuringAStroke) {
  const auto original = *editor.document();
  editor.beginTerrainStroke(original.player_spawn.foot_position);
  editor.requestClose();
  EXPECT_FALSE(editor.terrainStrokeActive());
  EXPECT_EQ(editor.pendingAction().kind, EditorPendingActionKind::Close);
  EXPECT_FALSE(editor.valid());
  EXPECT_TRUE(std::any_of(editor.diagnostics().begin(),
                          editor.diagnostics().end(), [](const auto& d) {
                            return d.document_path ==
                                   "player_spawn.foot_position.y";
                          }));
  EXPECT_FALSE(editor.resolvePending(EditorPendingDecision::Save));
  ASSERT_TRUE(editor.resolvePending(EditorPendingDecision::Cancel));
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.valid());
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  EXPECT_FALSE(editor.valid());
}

TEST_F(EditorCommands,
       SavedSculptedTerrainFeedsMatchingRuntimeMeshAndCollision) {
  EditorTerrainBrush brush;
  brush.radius = 3;
  brush.strength = 0.25F;
  ASSERT_TRUE(editor.setTerrainBrush(brush));
  editor.beginTerrainStroke({{10, 0, 10}});
  ASSERT_TRUE(editor.finishTerrainStroke());
  editor.select(editor_spawn);
  ASSERT_TRUE(editor.placeSelected({10, 0, 10}));
  ASSERT_TRUE(editor.saveAs(root / "runtime.json"))
      << formatLevelDiagnostics(editor.diagnostics());
  const auto authored = *editor.document();
  // Each startup constructs fresh immutable consumers from the saved file.
  for (int startup = 0; startup < 2; ++startup) {
    const auto level = loadPrototypeLevel(root / "runtime.json");
    EXPECT_EQ(level.terrain(), authored.terrain);
    const auto vertices = buildPrototypeSceneVertices(level);
    const auto preview =
        buildPrototypeSceneVertices(authored.terrain, authored.solids);
    ASSERT_EQ(vertices.size(), preview.size());
    for (std::size_t i = 0; i < vertices.size(); ++i)
      for (int axis = 0; axis < 3; ++axis) {
        EXPECT_EQ(vertices[i].position[axis], preview[i].position[axis]);
        EXPECT_EQ(vertices[i].normal[axis], preview[i].normal[axis]);
      }
    PhysicsWorld physics(level);
    auto state = physics.characterState();
    for (int step = 0; step < 180; ++step) {
      float vertical = state.supported() ? 0 : state.linear_velocity.y;
      state = physics.stepCharacter(
          {{0, vertical - 0.3F, 0}, {0, -18, 0}, false}, 1.0F / 60);
    }
    EXPECT_TRUE(state.supported());
    EXPECT_NEAR(
        state.foot_position.y,
        prototypeTerrainHeightAt(authored.terrain, state.foot_position.x,
                                 state.foot_position.z),
        0.03F);
    EXPECT_GT(state.foot_position.y, 0.2F);
  }
}
