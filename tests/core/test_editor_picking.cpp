#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "core/render/prototype_scene.hpp"
#include "editor/editor_camera.hpp"
#include "editor/editor_overlay.hpp"
#include "editor/editor_picking.hpp"
#include "editor/editor_property_edit.hpp"

namespace {
class EditorPlacement : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(editor.open("resources/levels/prototype.level.json"));
  }
  EditorDocument editor;
};
}  // namespace

TEST(EditorPicking, PointerRaysMatchCameraAndLogicalPixelScaling) {
  EditorCamera camera;
  const auto frame = camera.frame(2);
  const auto center = editorPointerRay(frame, 400, 200, 800, 400);
  ASSERT_TRUE(center);
  EXPECT_NEAR(center->direction.x, 0, 1e-6);
  EXPECT_NEAR(center->direction.y, 0, 1e-6);
  EXPECT_NEAR(center->direction.z, -1, 1e-6);
  const auto top_right = editorPointerRay(frame, 800, 0, 800, 400);
  ASSERT_TRUE(top_right);
  EXPECT_GT(top_right->direction.x, 0);
  EXPECT_GT(top_right->direction.y, 0);
  const auto scaled = editorPointerRay(frame, 1600, 0, 1600, 800);
  ASSERT_TRUE(scaled);
  EXPECT_EQ(scaled->direction, top_right->direction);
  EXPECT_FALSE(editorPointerRay(frame, 0, 0, 0, 400));
  EXPECT_FALSE(editorPointerRay(frame, -1, 0, 800, 400));
  CameraFrame singular;
  singular.view_projection.fill(0);
  EXPECT_FALSE(editorPointerRay(singular, 1, 1, 2, 2));
}

TEST_F(EditorPlacement,
       NearestBoxesInsideOriginsEdgesMissesAndStableSelection) {
  ASSERT_TRUE(editor.addSolid({{0, 15, -6}, {1, 1, 1}, {255, 255, 255, 255}}));
  const auto far = editor.selection();
  ASSERT_TRUE(editor.addSolid({{0, 15, -3}, {1, 1, 1}, {255, 255, 255, 255}}));
  const auto near = editor.selection();
  EXPECT_EQ(pickEditorObject(editor, {{0, 15, 0}, {0, 0, -1}}), near);
  EXPECT_EQ(pickEditorObject(editor, {{0, 15, -3}, {0, 0, -1}}), near);
  EXPECT_EQ(pickEditorObject(editor, {{1, 16, 0}, {0, 0, -1}}), near);
  EXPECT_EQ(pickEditorObject(editor, {{1.01F, 16, 0}, {0, 0, -1}}),
            editor_no_object);
  EXPECT_EQ(pickEditorObject(editor, {{0, 15, 0}, {0, 0, 0}}),
            editor_no_object);
  editor.select(near);
  ASSERT_TRUE(editor.removeSelected());
  EXPECT_EQ(pickEditorObject(editor, {{0, 15, 0}, {0, 0, -1}}), far);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(pickEditorObject(editor, {{0, 15, 0}, {0, 0, -1}}), near);
}

TEST_F(EditorPlacement, PicksYawedScaledOffsetPropProxyAndMarkers) {
  auto prop = editor.document()->static_prop;
  prop.translation = {0, 15, -4};
  prop.yaw_degrees = 90;
  prop.uniform_scale = 2;
  prop.box_proxy_center = {-0.3F, 0, 0};
  prop.box_proxy_half_extent = {1, 0.25F, 0.1F};
  ASSERT_TRUE(editor.replaceObject(editor_prop, prop));
  EXPECT_EQ(pickEditorObject(editor, {{0, 15, 0}, {0, 0, -1}}), editor_prop);
  EXPECT_EQ(pickEditorObject(editor, {{0.3F, 15, 0}, {0, 0, -1}}),
            editor_no_object);
  auto light = editor.document()->environment_light.point_lights[0];
  light.position = {0, 20, -2};
  ASSERT_TRUE(editor.replaceObject(editor_first_light, light));
  EXPECT_EQ(pickEditorObject(editor, {{0, 20, 0}, {0, 0, -1}}),
            editor_first_light);
  EXPECT_EQ(pickEditorObject(editor, {{0.25F, 20, 0}, {0, 0, -1}}),
            editor_first_light);
  EXPECT_EQ(pickEditorObject(editor, {{0, 20, -2}, {0, 0, -1}}),
            editor_first_light);
  auto spawn = editor.document()->player_spawn;
  spawn.foot_position = {0, 20 - editor_marker_radius, -1};
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  EXPECT_EQ(pickEditorObject(editor, {{0, 20, 0}, {0, 0, -1}}), editor_spawn);
}

TEST(EditorPicking, ExactTerrainTrianglesIncludeDiagonalsAndBorders) {
  PrototypeTerrain terrain;
  terrain.origin = {-24, 0, -24};
  terrain.sample_spacing = 0.5F;
  terrain.heights[49 * prototype_terrain_sample_count + 49] = 0.2F;
  for (auto p : {WorldPosition{0.1F, 0, 0.3F}, WorldPosition{0.3F, 0, 0.1F},
                 WorldPosition{0.25F, 0, 0.25F}, WorldPosition{24, 0, 24},
                 WorldPosition{-24, 0, -24}}) {
    const auto hit = pickEditorTerrain(terrain, {{p.x, 10, p.z}, {0, -1, 0}});
    ASSERT_TRUE(hit);
    EXPECT_NEAR(hit->position.y, prototypeTerrainHeightAt(terrain, p.x, p.z),
                1e-6);
    EXPECT_NEAR(hit->distance, 10 - hit->position.y, 1e-6);
  }
  EXPECT_FALSE(pickEditorTerrain(terrain, {{25, 10, 25}, {0, -1, 0}}));
  EXPECT_FALSE(pickEditorTerrain(terrain, {{0, 10, 0}, {0, 1, 0}}));
  EXPECT_FALSE(pickEditorTerrain(terrain, {{0, 10, 0}, {1, 0, 0}}));
}

TEST_F(EditorPlacement,
       PlacementPreservesPropertiesAndObjectSpecificVerticalAnchors) {
  const WorldPosition hit{8.3F, 100, 8.2F};
  const float height =
      prototypeTerrainHeightAt(editor.document()->terrain, hit.x, hit.z);
  const auto solid_id = editor.solidIds()[0];
  const auto original_solid = editor.document()->solids[0];
  editor.select(solid_id);
  ASSERT_TRUE(editor.placeSelected(hit));
  auto expected_solid = original_solid;
  expected_solid.center = {hit.x, height + original_solid.half_extent.y, hit.z};
  EXPECT_EQ(*editor.object(solid_id), EditorObjectValue(expected_solid));
  auto generation = editor.revision();
  EXPECT_FALSE(editor.placeSelected(hit));
  EXPECT_EQ(editor.revision(), generation);
  editor.select(editor_spawn);
  auto expected_spawn = editor.document()->player_spawn;
  expected_spawn.foot_position = {hit.x, height, hit.z};
  ASSERT_TRUE(editor.placeSelected(hit));
  EXPECT_EQ(editor.document()->player_spawn, expected_spawn);
  editor.select(editor_prop);
  auto expected_prop = editor.document()->static_prop;
  expected_prop.translation = {hit.x, height, hit.z};
  ASSERT_TRUE(editor.placeSelected(hit));
  EXPECT_EQ(editor.document()->static_prop, expected_prop);
  editor.select(editor_first_light);
  auto expected_light = editor.document()->environment_light.point_lights[0];
  const float offset = expected_light.position.y -
                       prototypeTerrainHeightAt(editor.document()->terrain,
                                                expected_light.position.x,
                                                expected_light.position.z);
  expected_light.position = {hit.x, height + offset, hit.z};
  ASSERT_TRUE(editor.placeSelected(hit));
  EXPECT_EQ(editor.document()->environment_light.point_lights[0],
            expected_light);
  generation = editor.revision();
  EXPECT_FALSE(editor.placeSelected(hit));
  EXPECT_EQ(editor.revision(), generation);
  EXPECT_FALSE(editor.placeSelected({100, 0, 100}));
  EXPECT_EQ(editor.revision(), generation);
}

TEST_F(EditorPlacement,
       ViewportCaptureNavigationAndMissesDoNotMutateDocuments) {
  ASSERT_TRUE(editor.addSolid({{0, 15, -3}, {1, 1, 1}, {255, 255, 255, 255}}));
  const auto id = editor.selection();
  editor.select(editor_spawn);
  const EditorRay object_ray{{0, 15, 0}, {0, 0, -1}};
  static_cast<void>(
      updateEditorViewport(editor, object_ray, true, false, true, false));
  EXPECT_EQ(editor.selection(), editor_spawn);
  static_cast<void>(
      updateEditorViewport(editor, object_ray, false, true, true, false));
  EXPECT_EQ(editor.selection(), editor_spawn);
  static_cast<void>(
      updateEditorViewport(editor, object_ray, false, false, false, false));
  EXPECT_EQ(editor.selection(), editor_spawn);
  static_cast<void>(
      updateEditorViewport(editor, object_ray, false, false, true, false));
  EXPECT_EQ(editor.selection(), id);
  const auto before = *editor.document();
  const auto generation = editor.revision();
  EXPECT_FALSE(
      updateEditorViewport(editor, object_ray, false, false, true, true));
  EXPECT_EQ(*editor.document(), before);
  EXPECT_EQ(editor.revision(), generation);
  const EditorRay terrain_ray{{8, 10, 8}, {0, -1, 0}};
  EXPECT_FALSE(
      updateEditorViewport(editor, terrain_ray, true, false, true, true));
  EXPECT_FALSE(
      updateEditorViewport(editor, terrain_ray, false, true, true, true));
  EXPECT_TRUE(
      updateEditorViewport(editor, terrain_ray, false, false, false, true));
  EXPECT_EQ(*editor.document(), before);
  EXPECT_TRUE(
      updateEditorViewport(editor, terrain_ray, false, false, true, true));
  EXPECT_NE(*editor.document(), before);
  static_cast<void>(updateEditorViewport(
      editor, EditorRay{{0, 50, 0}, {0, 1, 0}}, false, false, true, false));
  EXPECT_EQ(editor.selection(), editor_no_object);
}

TEST_F(EditorPlacement, PropertyDragCreatesOneCommandAndRejectsStaleDrafts) {
  editor.select(editor_spawn);
  const auto original = *editor.document();
  const auto generation = editor.revision();
  EditorPropertyEdit edit;
  edit.synchronize(editor);
  for (int i = 0; i < 20; ++i) {
    std::get<PrototypePlayerSpawn>(*edit.value()).yaw_degrees += 1;
    edit.synchronize(editor);
    EXPECT_EQ(editor.revision(), generation);
    EXPECT_EQ(*editor.document(), original);
  }
  ASSERT_TRUE(edit.commit(editor));
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.canUndo());
  edit.synchronize(editor);
  std::get<PrototypePlayerSpawn>(*edit.value()).yaw_degrees += 10;
  editor.select(editor_prop);
  EXPECT_FALSE(edit.commit(editor));
  EXPECT_EQ(*editor.document(), original);
  editor.select(editor_spawn);
  edit.synchronize(editor);
  std::get<PrototypePlayerSpawn>(*edit.value()).yaw_degrees =
      std::numeric_limits<float>::infinity();
  EXPECT_FALSE(edit.commit(editor));
  EXPECT_EQ(*edit.value(), EditorObjectValue(original.player_spawn));
  EXPECT_FALSE(editor.editError().empty());
}

TEST(EditorOverlay, ClipsLinesToVulkanNearFarAndSidePlanes) {
  CameraFrame frame;
  const auto line = projectEditorLine(frame, {-2, 0, 0.5F}, {2, 0, 0.5F},
                                      {255, 255, 255, 255});
  ASSERT_TRUE(line);
  EXPECT_FLOAT_EQ(line->first[0], 0);
  EXPECT_FLOAT_EQ(line->second[0], 1);
  EXPECT_FLOAT_EQ(line->first[1], 0.5F);
  EXPECT_FALSE(projectEditorLine(frame, {-1, 0, -1}, {1, 0, -1}, {}));
  EXPECT_FALSE(projectEditorLine(frame, {-1, 0, 2}, {1, 0, 2}, {}));
  EXPECT_TRUE(projectEditorLine(frame, {0, 0, -1}, {0, 0, 0.5F}, {}));
  EXPECT_FALSE(
      projectEditorLine(EditorCamera{}.frame(1), {0, 2, 10}, {1, 2, 10}, {}));
}

TEST_F(EditorPlacement, SafeInvalidPreviewAndOverlayReflectEditedGeometry) {
  ASSERT_TRUE(editor.addSolid(
      {{0, 0, 0.5F}, {0.1F, 0.1F, 0.1F}, {255, 255, 255, 255}}));
  const auto lines = buildEditorOverlay(editor, CameraFrame{});
  std::size_t selected_edges = 0;
  for (const auto& line : lines) {
    if (line.color == WorldColor{255, 205, 60, 255}) {
      ++selected_edges;
      EXPECT_GE(line.first[0], 0.44F);
      EXPECT_LE(line.first[0], 0.56F);
    }
  }
  EXPECT_EQ(selected_edges, 12U);
  auto spawn = editor.document()->player_spawn;
  spawn.foot_position.y += 5;
  ASSERT_TRUE(editor.replaceObject(editor_spawn, spawn));
  EXPECT_FALSE(editor.valid());
  EXPECT_THROW(static_cast<void>(makePrototypeLevel(*editor.document())),
               std::invalid_argument);
  const auto vertices = buildPrototypeSceneVertices(editor.document()->terrain,
                                                    editor.document()->solids);
  EXPECT_EQ(vertices.size(),
            editor.document()->solids.size() * 36 + 96 * 96 * 6);
  for (const auto& vertex : vertices)
    for (const float component : vertex.position)
      EXPECT_TRUE(std::isfinite(component));
  editor.requestClose();
  ASSERT_TRUE(editor.resolvePending(EditorPendingDecision::Discard));
  EXPECT_TRUE(buildEditorOverlay(editor, CameraFrame{}).empty());
}

TEST_F(EditorPlacement,
       TerrainToolSuppressesUiAndNavigationAndDoesNotBridgeMisses) {
  const auto original = *editor.document();
  const EditorRay a{{10, 50, 10}, {0, -1, 0}};
  const EditorRay b{{12, 50, 10}, {0, -1, 0}};
  const auto update = [&](const auto& ray, bool owned, bool nav, bool press,
                          bool down) {
    return updateEditorTerrainViewport(editor, ray, owned, nav, press, down);
  };
  EXPECT_TRUE(update(a, false, false, false, false));
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(update(a, true, false, true, true));
  EXPECT_TRUE(update(b, false, false, false, true));
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(update(a, false, true, true, true));
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(update(std::optional<EditorRay>{}, false, false, true, true));
  EXPECT_FALSE(update(std::optional<EditorRay>{}, false, false, false, false));
  EXPECT_FALSE(editor.canUndo());

  EXPECT_TRUE(update(a, false, false, true, true));
  EXPECT_FALSE(update(std::optional<EditorRay>{}, false, false, false, true));
  EXPECT_TRUE(update(b, false, false, false, true));
  EXPECT_TRUE(update(b, false, false, false, false));
  auto expected = original.terrain;
  for (auto p : {WorldPosition{10, 0, 10}, WorldPosition{12, 0, 10}})
    for (auto e : editorTerrainStamp(expected, {}, p))
      expected.heights[e.index] = e.after;
  EXPECT_EQ(editor.document()->terrain, expected);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), original);
  EXPECT_FALSE(editor.canUndo());
}

TEST_F(EditorPlacement, StationaryPointerDoesNotStampAgainAsIntersectionMoves) {
  const EditorRay ray{{10, 20, 10}, {0.1F, -1, 0}};
  ASSERT_TRUE(
      updateEditorTerrainViewport(editor, ray, false, false, true, true));
  const auto stamped = *editor.document();
  for (int i = 0; i < 100; ++i)
    ASSERT_TRUE(updateEditorTerrainViewport(editor, ray, false, false, false,
                                            true, false));
  EXPECT_EQ(*editor.document(), stamped);
  static_cast<void>(updateEditorTerrainViewport(editor, ray, false, false,
                                                false, false, false));
  EXPECT_TRUE(editor.canUndo());
}

TEST_F(EditorPlacement, BrushFootprintAndInvalidTriangleOverlaysFollowTerrain) {
  // Top-down orthographic view containing the entire terrain.
  CameraFrame camera;
  camera.view_projection = {1.0F / 30, 0,         0, 0, 0, 0, -1.0F / 100, 0,
                            0,         1.0F / 30, 0, 0, 0, 0, 0.5F,        1};
  EditorTerrainBrush brush;
  const auto center =
      prototypeTerrainSamplePosition(editor.document()->terrain, 40, 40);
  const auto footprint = buildEditorOverlay(editor, camera, center, &brush);
  EXPECT_GT(footprint.size(), buildEditorOverlay(editor, camera).size() + 300);
  brush.falloff = 0;
  const auto hard = buildEditorOverlay(editor, camera, center, &brush);
  EXPECT_NE(footprint.front().color, hard.front().color);
  const auto edge = buildEditorOverlay(
      editor, camera, editor.document()->terrain.origin, &brush);
  EXPECT_LT(edge.size(), hard.size());
  brush.radius = 0.5F;
  brush.strength = 1;
  ASSERT_TRUE(editor.setTerrainBrush(brush));
  editor.beginTerrainStroke(center);
  ASSERT_TRUE(editor.finishTerrainStroke());
  const auto count_red = [&](const auto& lines) {
    return std::count_if(lines.begin(), lines.end(), [](const auto& line) {
      return line.color == WorldColor{255, 70, 70, 255};
    });
  };
  const auto invalid = buildEditorOverlay(editor, camera);
  const auto diagnostic_count =
      std::count_if(editor.diagnostics().begin(), editor.diagnostics().end(),
                    [](const auto& d) {
                      return d.terrain_location && d.terrain_location->triangle;
                    });
  EXPECT_GT(diagnostic_count, 0);
  EXPECT_EQ(count_red(invalid), 3 * diagnostic_count);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(count_red(buildEditorOverlay(editor, camera)), 0);
}

TEST_F(EditorPlacement, SwitchYawPickingPlacementAndDraftValidation) {
  auto value = *editor.document()->light_switch;
  value.position = {0, 15, 0};
  value.yaw_degrees = 90;
  value.point_light_index = 1;
  value.initially_on = false;
  ASSERT_TRUE(editor.replaceObject(editor_light_switch, value));
  const EditorRay ray{{1, 15, 0}, {-2, 0, 0}};
  EXPECT_EQ(pickEditorObject(editor, ray), editor_light_switch);
  EXPECT_NE(pickEditorObject(editor, {{1, 15, 0.1F}, {-1, 0, 0}}),
            editor_light_switch);
  ASSERT_TRUE(editor.addSolid(
      {{0.6F, 15, 0}, {0.1F, 0.1F, 0.1F}, {255, 255, 255, 255}}));
  EXPECT_EQ(pickEditorObject(editor, ray), editor.selection());
  ASSERT_TRUE(editor.undo());
  static_cast<void>(
      updateEditorViewport(editor, ray, false, false, true, false));
  ASSERT_EQ(editor.selection(), editor_light_switch);
  const float height =
      value.position.y - prototypeTerrainHeightAt(editor.document()->terrain,
                                                  value.position.x,
                                                  value.position.z);
  const auto before = *editor.document();
  EXPECT_FALSE(updateEditorViewport(editor, EditorRay{{30, 10, 30}, {0, -1, 0}},
                                    false, false, true, true));
  EXPECT_EQ(*editor.document(), before);
  const auto placed = updateEditorViewport(
      editor, EditorRay{{-15, 10, -8}, {0, -1, 0}}, false, false, true, true);
  ASSERT_TRUE(placed);
  const auto moved = *editor.document()->light_switch;
  EXPECT_NEAR(moved.position.y - placed->y, height, 0.00001F);
  EXPECT_EQ(moved.yaw_degrees, value.yaw_degrees);
  EXPECT_EQ(moved.point_light_index, value.point_light_index);
  EXPECT_EQ(moved.initially_on, value.initially_on);
  const auto revision = editor.revision();
  EXPECT_FALSE(editor.placeSelected(*placed));
  EXPECT_EQ(editor.revision(), revision);
  EditorPropertyEdit draft;
  draft.synchronize(editor);
  std::get<PrototypeLightSwitch>(*draft.value()).position.y =
      std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(draft.commit(editor));
  EXPECT_EQ(editor.document()->light_switch, moved);
  EXPECT_FALSE(editor.editError().empty());
  std::get<PrototypeLightSwitch>(*draft.value()).yaw_degrees = 12;
  EXPECT_TRUE(draft.commit(editor));
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(editor.document()->light_switch, moved);
}
