#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

#include "core/physics/physics_world.hpp"
#include "core/render/prototype_scene.hpp"
#include "core/world/light_switch.hpp"
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
  solid.material = "prototype-obstacle";
  ASSERT_TRUE(editor.replaceObject(id, solid));
  auto spawn = original.entries.front();
  spawn.pose.yaw_degrees += 35.0F;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  for (EditorObjectId light_id :
       {editor.lightIds().front(), editor.lightIds()[1]}) {
    auto light = std::get<PrototypePointLight>(*editor.object(light_id));
    light.position.x += 0.25F;
    light.color = {0.0F, 0.5F, 0.7F};
    light.intensity += 0.25F;
    light.radius += 1.0F;
    ASSERT_TRUE(editor.replaceObject(light_id, light));
  }
  auto prop = original.props.front();
  prop.translation.x += 0.1F;
  prop.yaw_degrees = 45.0F;
  prop.uniform_scale = 0.75F;
  prop.collision_boxes.front().center = {-0.1F, 0.6F, 0.0F};
  prop.collision_boxes.front().half_extent = {0.3F, 0.6F, 0.3F};
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
  for (int field = 0; field < 4; ++field) {
    auto s = original.solids[0];
    if (field == 0) s.center.z = nan;
    if (field == 1) s.half_extent.x = 0;
    if (field == 2) s.half_extent.y = inf;
    if (field == 3) s.kind = static_cast<PrototypeSolidKind>(100);

    reject(solid_id, s);
  }
  for (int field = 0; field < 2; ++field) {
    auto s = original.entries.front();
    if (field == 0) s.pose.foot_position.x = inf;
    if (field == 1) s.pose.yaw_degrees = nan;
    reject(editor_spawn, s);
  }
  for (int field = 0; field < 4; ++field) {
    auto l = original.environment_light.point_lights[0];
    if (field == 0) l.position.y = nan;
    if (field == 1) l.color[0] = -1;
    if (field == 2) l.color[2] = inf;
    if (field == 3) l.intensity = 0;
    if (field == 4) l.radius = -1;
    reject(editor.lightIds().front(), l);
  }
  for (int field = 0; field < 7; ++field) {
    auto p = original.props.front();
    if (field == 0) p.translation.x = nan;
    if (field == 1) p.yaw_degrees = inf;
    if (field == 2) p.uniform_scale = 0;
    if (field == 3) p.collision_boxes.front().center.z = nan;
    if (field == 4) p.collision_boxes.front().half_extent.y = -1;
    if (field == 5) p.id = "invalid ID";
    if (field == 6) p.uniform_scale = std::numeric_limits<float>::max();
    reject(editor_prop, p);
  }
  reject(editor_spawn, original.solids[0]);
  reject(999999, original.entries.front());
}

TEST_F(EditorCommands, InvalidGameplayRemainsEditableButSaveIsGated) {
  const auto original = *editor.document();
  auto solid = original.solids[0];
  solid.center = original.entries.front().pose.foot_position;
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
  auto spawn = editor.document()->entries.front();
  spawn.pose.yaw_degrees += 1;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  ASSERT_TRUE(editor.saveAs(root / "saved.json"));
  spawn.pose.yaw_degrees += 1;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  ASSERT_TRUE(editor.undo());
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.undo());
  EXPECT_TRUE(editor.dirty());
  spawn.pose.yaw_degrees += 2;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  EXPECT_TRUE(editor.dirty());
  EXPECT_FALSE(editor.canRedo());
  const auto revision = editor.revision();
  EXPECT_FALSE(editor.replaceObject(editor_spawn, spawn));
  EXPECT_EQ(editor.revision(), revision);
}

TEST_F(EditorCommands,
       HistoryRetainsNewest128OperationsAndRefreshesPreviewOnUndoRedo) {
  auto spawn = editor.document()->entries.front();
  const float initial_yaw = spawn.pose.yaw_degrees;
  for (int i = 0; i < 130; ++i) {
    spawn.pose.yaw_degrees += 1;
    ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  }
  auto generation = editor.revision();
  for (int i = 0; i < 128; ++i) {
    ASSERT_TRUE(editor.undo());
    EXPECT_GT(editor.revision(), generation);
    generation = editor.revision();
  }
  EXPECT_FALSE(editor.undo());
  EXPECT_FLOAT_EQ(editor.document()->entries.front().pose.yaw_degrees,
                  initial_yaw + 2);
  for (int i = 0; i < 128; ++i) ASSERT_TRUE(editor.redo());
  EXPECT_FALSE(editor.redo());
  EXPECT_FLOAT_EQ(editor.document()->entries.front().pose.yaw_degrees,
                  initial_yaw + 130);
}

TEST_F(EditorCommands, SolidCountBoundAndRequiredEntryAreProtected) {
  editor.select(editor_spawn);
  EXPECT_FALSE(editor.removeSelected());
  for (const auto id : {editor.lightIds().front(), editor.lightIds()[1]}) {
    editor.select(id);
    ASSERT_TRUE(editor.removeSelected());
    ASSERT_TRUE(editor.undo());
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
  EXPECT_EQ(sculpted.entries.front(), original.entries.front());
  EXPECT_EQ(sculpted.props.front(), original.props.front());
  auto expected = *original.terrain;
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

  auto spawn = original.entries.front();
  spawn.pose.yaw_degrees += 10;
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
  const auto& terrain = *original.terrain;
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
  invalid.terrain->heights[40 * prototype_terrain_sample_count + 41] = NAN;
  const auto diagnostics = validateLevelDocument(invalid);
  ASSERT_FALSE(diagnostics.empty());
  ASSERT_TRUE(diagnostics.front().terrain_location);
  EXPECT_EQ(diagnostics.front().terrain_location->x, 41U);
  EXPECT_EQ(diagnostics.front().terrain_location->z, 40U);
  EXPECT_FALSE(diagnostics.front().terrain_location->triangle);
}

TEST_F(EditorCommands, TerrainSpawnSupportAndCloseRefreshEvenDuringAStroke) {
  const auto original = *editor.document();
  editor.beginTerrainStroke(original.entries.front().pose.foot_position);
  editor.requestClose();
  EXPECT_FALSE(editor.terrainStrokeActive());
  EXPECT_EQ(editor.pendingAction().kind, EditorPendingActionKind::Close);
  EXPECT_FALSE(editor.valid());
  EXPECT_TRUE(std::any_of(editor.diagnostics().begin(),
                          editor.diagnostics().end(), [](const auto& d) {
                            return d.document_path ==
                                   "entries[0].foot_position";
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
    const auto preview = buildPrototypeSceneVertices(
        authored.terrain, authored.solids, authored.light_switches);
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
      physics.advanceWorld(1.0F / 60);
      state = physics.stepCharacter(
          {{0, vertical - 0.3F, 0}, {0, -18, 0}, false}, 1.0F / 60);
    }
    EXPECT_TRUE(state.supported());
    EXPECT_NEAR(
        state.foot_position.y,
        prototypeTerrainHeightAt(*authored.terrain, state.foot_position.x,
                                 state.foot_position.z),
        0.03F);
    EXPECT_GT(state.foot_position.y, 0.2F);
  }
}

TEST_F(EditorCommands, LightingCollectionsKeepIdentityHistoryAndDirtyState) {
  const auto original = *editor.document();
  const auto original_switch = editor.switchIds().front();
  editor.select(original_switch);
  ASSERT_TRUE(editor.duplicateSelected());
  const auto copy = editor.selection();
  EXPECT_NE(copy, original_switch);
  EXPECT_NE(editor.document()->light_switches[1].id,
            original.light_switches[0].id);
  EXPECT_EQ(editor.document()->light_switches[1].light_id,
            original.light_switches[0].light_id);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(editor.selection(), copy);
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_FALSE(editor.object(copy));
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.selection(), copy);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.dirty());
  ASSERT_TRUE(editor.addPointLight());
  const auto light =
      std::get<PrototypePointLight>(*editor.object(editor.selection()));
  EXPECT_TRUE(light.initially_on);
  EXPECT_FALSE(light.casts_shadows);
  EXPECT_FALSE(light.id.empty());
  while (editor.lightIds().size() < 8) ASSERT_TRUE(editor.addPointLight());
  const auto full = *editor.document();
  EXPECT_FALSE(editor.addPointLight());
  EXPECT_FALSE(editor.duplicateSelected());
  EXPECT_EQ(*editor.document(), full);
  while (editor.switchIds().size() < 16) ASSERT_TRUE(editor.addLightSwitch());
  EXPECT_FALSE(editor.addLightSwitch());
  EXPECT_FALSE(editor.duplicateSelected());
  ASSERT_TRUE(editor.saveAs(root / "capacity.json"));
  const auto saved = *editor.document();
  ASSERT_TRUE(editor.open(root / "capacity.json"));
  EXPECT_EQ(*editor.document(), saved);
  EXPECT_FALSE(editor.dirty());
}

TEST_F(EditorCommands,
       SharedLinksRenameAtomicallyAndDeletionLeavesRepairableLinks) {
  const auto light_handle = editor.lightIds().front();
  editor.select(editor.switchIds().front());
  ASSERT_TRUE(editor.duplicateSelected());
  const auto before = *editor.document();
  auto light = before.environment_light.point_lights.front();
  light.id = "room-ceiling";
  ASSERT_TRUE(editor.replaceObject(light_handle, light));
  for (const auto& value : editor.document()->light_switches)
    EXPECT_EQ(value.light_id, light.id);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  ASSERT_TRUE(editor.redo());
  const auto renamed = *editor.document();
  light.id = renamed.environment_light.point_lights[1].id;
  EXPECT_FALSE(editor.replaceObject(light_handle, light));
  EXPECT_EQ(*editor.document(), renamed);
  editor.select(light_handle);
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_FALSE(editor.valid());
  EXPECT_EQ(editor.document()->light_switches, renamed.light_switches);
  EXPECT_FALSE(editor.saveAs(root / "broken.json"));
  auto repaired = editor.document()->light_switches.front();
  repaired.light_id =
      editor.document()->environment_light.point_lights.front().id;
  ASSERT_TRUE(editor.replaceObject(editor.switchIds().front(), repaired));
  EXPECT_FALSE(editor.valid());  // The second incoming link is still broken.
  ASSERT_TRUE(editor.undo());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), renamed);
  EXPECT_TRUE(editor.valid());
}

TEST_F(EditorCommands,
       ShadowBudgetInvalidityIsUndoableAndLightlessSwitchesStayEditable) {
  while (editor.lightIds().size() < 4) ASSERT_TRUE(editor.addPointLight());
  for (auto handle : editor.lightIds()) {
    auto light = std::get<PrototypePointLight>(*editor.object(handle));
    light.casts_shadows = true;
    ASSERT_TRUE(editor.replaceObject(handle, light));
  }
  EXPECT_TRUE(editor.valid());
  const auto before = *editor.document();
  editor.select(editor.lightIds().front());
  ASSERT_TRUE(editor.duplicateSelected());
  EXPECT_FALSE(editor.valid());
  EXPECT_TRUE(
      editor.document()->environment_light.point_lights.back().casts_shadows);
  const auto duplicate = editor.selection();
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  EXPECT_TRUE(editor.valid());
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(editor.selection(), duplicate);
  while (!editor.lightIds().empty()) {
    editor.select(editor.lightIds().back());
    ASSERT_TRUE(editor.removeSelected());
  }
  ASSERT_TRUE(editor.addLightSwitch());
  EXPECT_TRUE(std::get<PrototypeLightSwitch>(*editor.object(editor.selection()))
                  .light_id.empty());
  EXPECT_FALSE(editor.valid());
  EXPECT_FALSE(editor.saveAs(root / "lightless-broken.json"));
}

TEST_F(EditorCommands,
       InitialValuesAreIndependentOfSwitchLinksAndTerrainHistory) {
  auto light = editor.document()->environment_light.point_lights[0];
  light.initially_on = false;
  ASSERT_TRUE(editor.replaceObject(editor.lightIds()[0], light));
  const auto initial =
      initialPointLightEnabled(editor.document()->environment_light);
  EXPECT_EQ(initial, (std::vector<std::uint8_t>{0, 1}));
  auto value = editor.document()->light_switches.front();
  value.light_id = "point-light-1";
  ASSERT_TRUE(editor.replaceObject(editor.switchIds().front(), value));
  EXPECT_EQ(initialPointLightEnabled(editor.document()->environment_light),
            initial);
  editor.select(editor.switchIds().front());
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_EQ(initialPointLightEnabled(editor.document()->environment_light),
            initial);
  ASSERT_TRUE(editor.undo());
  const auto before_stroke = *editor.document();
  editor.beginTerrainStroke({{15, 0, 15}});
  editor.extendTerrainStroke({{16, 0, 15}});
  ASSERT_TRUE(editor.finishTerrainStroke());
  EXPECT_EQ(editor.document()->light_switches, before_stroke.light_switches);
  EXPECT_EQ(initialPointLightEnabled(editor.document()->environment_light),
            initial);
  EXPECT_GT(buildPrototypeSceneVertices(editor.document()->terrain,
                                        editor.document()->solids,
                                        editor.document()->light_switches)
                .size(),
            buildPrototypeSceneVertices(editor.document()->terrain,
                                        editor.document()->solids)
                .size());
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before_stroke);
  ASSERT_TRUE(editor.setAmbient(0));
  EXPECT_FLOAT_EQ(editor.document()->environment_light.ambient_intensity, 0);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before_stroke);
  ASSERT_TRUE(editor.setAmbient(.2F));
  EXPECT_FALSE(editor.setAmbient(.201F));
  value.yaw_degrees = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(editor.replaceObject(editor.switchIds().front(), value));
  value.yaw_degrees = 0;
  value.id = "Invalid-ID";
  EXPECT_FALSE(editor.replaceObject(editor.switchIds().front(), value));
}

TEST_F(EditorCommands, VersionTwoOpensCleanAndExplicitSaveWritesV8) {
  auto old = *editor.document();
  old.light_switches.clear();
  const auto path = root / "old.json";
  auto source = bytes("tests/fixtures/levels/prototype-v3.level.json");
  source.replace(source.find("\"version\": 3"), 12, "\"version\": 2");
  source.erase(source.find(",\n  \"light_switch\""));
  source += "\n}\n";
  {
    std::ofstream output(path, std::ios::binary);
    output << source;
  }
  ASSERT_TRUE(editor.open(path));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(*editor.document(), old);
  EXPECT_EQ(bytes(path), source);
  ASSERT_TRUE(editor.addLightSwitch());
  auto value = editor.document()->light_switches.front();
  value.yaw_degrees = -31;
  value.light_id = "point-light-1";
  ASSERT_TRUE(editor.replaceObject(editor.switchIds().front(), value));
  ASSERT_TRUE(editor.save());
  EXPECT_NE(bytes(path).find("\"version\": 9"), std::string::npos);
  ASSERT_TRUE(editor.open(path));
  EXPECT_FALSE(editor.dirty());
  EXPECT_EQ(editor.document()->light_switches.front(), value);
}
