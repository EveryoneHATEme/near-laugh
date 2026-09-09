#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "editor/editor_character_spatial.hpp"
#include "editor/editor_overlay.hpp"
#include "editor/editor_picking.hpp"

namespace {
class EditorCharacterSpatial : public testing::Test {
 protected:
  EditorDocument editor;

  void SetUp() override {
    editor.requestNewInterior();
    ASSERT_TRUE(editor.document());
  }

  EditorObjectId addMark(WorldPosition feet, float yaw = 0) {
    EXPECT_TRUE(editor.addCharacter(EditorCharacterKind::Mark));
    const auto id = editor.selection();
    auto mark = std::get<CharacterMarkDefinition>(*editor.object(id));
    mark.feet_position = feet;
    mark.yaw_degrees = yaw;
    EXPECT_TRUE(editor.replaceObject(id, mark));
    return id;
  }

  EditorObjectId addRoute(const std::vector<EditorObjectId>& marks) {
    EXPECT_TRUE(editor.addCharacter(EditorCharacterKind::Route));
    const auto id = editor.selection();
    auto route = std::get<CharacterRouteDefinition>(*editor.object(id));
    route.marks.clear();
    for (const auto mark : marks)
      route.marks.push_back(
          std::get<CharacterMarkDefinition>(*editor.object(mark)).id);
    EXPECT_TRUE(editor.replaceObject(id, route));
    return id;
  }

  CameraFrame camera() const {
    // An orthographic inspection volume centered on the isolated test marks.
    CameraFrame frame;
    frame.view_projection[0] = 0.1F;
    frame.view_projection[5] = 0.1F;
    frame.view_projection[10] = -0.05F;
    frame.view_projection[13] = -1.5F;
    return frame;
  }
};
}  // namespace

TEST(EditorCharacterBounds, CatalogCalibrationAndYawAreSeparateFromCapsule) {
  const CharacterMarkDefinition mark{"mark", {3, 15, -4}, 90};
  const auto bounds = editorCharacterVisualBounds(test_mannequin_catalog, mark);
  EXPECT_NEAR(bounds.center.x, 3.125F, 1e-6);
  EXPECT_NEAR(bounds.center.y, 15.9F, 1e-6);
  EXPECT_NEAR(bounds.center.z, -4.F, 1e-6);
  EXPECT_FLOAT_EQ(bounds.half_extent.x, 1.05F);
  EXPECT_FLOAT_EQ(bounds.half_extent.y, 1.F);
  EXPECT_FLOAT_EQ(bounds.half_extent.z, 0.675F);
  EXPECT_GT(bounds.half_extent.x, test_mannequin_catalog.capsule_radius_m);
  EXPECT_EQ(bounds.yaw_degrees, mark.yaw_degrees);
  const auto tip = editorCharacterFacingTip(mark);
  EXPECT_NEAR(tip.x, mark.feet_position.x + 1.45F, 1e-6);
  EXPECT_NEAR(tip.z, mark.feet_position.z, 1e-6);
  auto unsafe = mark;
  unsafe.feet_position.x = std::numeric_limits<float>::quiet_NaN();
  EXPECT_FALSE(editorFiniteCharacterMark(unsafe));
  unsafe = mark;
  unsafe.yaw_degrees = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(editorFiniteCharacterMark(unsafe));
}

TEST_F(EditorCharacterSpatial, ActorPickingUsesYawedVisualBoundsAndNearestHit) {
  const auto mark_id = addMark({0, 15, -4});
  ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
  const auto actor_id = editor.selection();
  EXPECT_EQ(pickEditorObject(editor, {{0.9F, 16, 0}, {0, 0, -1}}), actor_id);
  auto mark = std::get<CharacterMarkDefinition>(*editor.object(mark_id));
  mark.yaw_degrees = 90;
  ASSERT_TRUE(editor.replaceObject(mark_id, mark));
  EXPECT_EQ(pickEditorObject(editor, {{0.9F, 16, 0}, {0, 0, -1}}),
            editor_no_object);
  EXPECT_EQ(pickEditorObject(editor, {{0.75F, 16, 0}, {0, 0, -3}}), actor_id);
  ASSERT_TRUE(editor.addSolid(
      {{0.75F, 16, -1}, {0.1F, 0.1F, 0.1F}, {255, 255, 255, 255}}));
  const auto solid_id = editor.selection();
  for (const float scale : {1.F, 3.F})
    EXPECT_EQ(pickEditorObject(editor, {{0.75F, 16, 0}, {0, 0, -scale}}),
              solid_id);
  ASSERT_TRUE(editor.removeSelected());
  const auto revision = editor.revision();
  static_cast<void>(updateEditorViewport(editor,
                                         EditorRay{{0.75F, 16, 0}, {0, 0, -1}},
                                         false, false, true, false));
  EXPECT_EQ(editor.selection(), actor_id);
  EXPECT_EQ(editor.revision(), revision);
  ASSERT_TRUE(editor.object(actor_id));
  EXPECT_EQ(editorCharacterKind(*editor.object(actor_id)),
            EditorCharacterKind::Actor);
  const auto lines = buildEditorOverlay(editor, camera());
  EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [](const auto& line) {
    return line.color == WorldColor{70, 220, 255, 255};
  }));
  EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [](const auto& line) {
    return line.color == WorldColor{255, 205, 60, 255};
  }));
}

TEST_F(EditorCharacterSpatial,
       MarksFacingAndRoutePointsHaveDistinctListHandles) {
  const auto first = addMark({-3, 15, -4});
  const auto second = addMark({3, 15, -4});
  const auto route_id = addRoute({first, second});
  EXPECT_EQ(pickEditorObject(editor, {{-3, 15.25F, 0}, {0, 0, -1}}), first);
  EXPECT_EQ(pickEditorObject(editor, {{-5, 15.25F, -2.55F}, {1, 0, 0}}), first);
  EXPECT_EQ(pickEditorObject(editor, {{-3, 15.65F, 0}, {0, 0, -1}}), route_id);
  EXPECT_EQ(pickEditorObject(editor, {{0, 15.65F, 0}, {0, 0, -2}}), route_id);
  static_cast<void>(updateEditorViewport(editor,
                                         EditorRay{{0, 15.65F, 0}, {0, 0, -1}},
                                         false, false, true, false));
  EXPECT_EQ(editor.selection(), route_id);
  const auto labels = buildEditorCharacterOverlayLabels(editor, camera());
  const auto route =
      std::get<CharacterRouteDefinition>(*editor.object(route_id));
  for (std::size_t i = 0; i < route.marks.size(); ++i)
    EXPECT_TRUE(
        std::any_of(labels.begin(), labels.end(), [&](const auto& label) {
          return label.object == route_id &&
                 label.text.find(std::to_string(i + 1) + ": " +
                                 route.marks[i]) != std::string::npos;
        }));
  const auto a = editorCharacterRouteHandle(
      std::get<CharacterMarkDefinition>(*editor.object(first)));
  const auto b = editorCharacterRouteHandle(
      std::get<CharacterMarkDefinition>(*editor.object(second)));
  const auto expected = projectEditorLine(camera(), a, b, {255, 205, 60, 255});
  ASSERT_TRUE(expected);
  const auto lines = buildEditorOverlay(editor, camera());
  EXPECT_TRUE(std::any_of(lines.begin(), lines.end(), [&](const auto& line) {
    return line.first == expected->first && line.second == expected->second &&
           line.color == expected->color;
  }));
}

TEST_F(EditorCharacterSpatial,
       MissingEndpointBreaksOnlyItsSegmentsAndStaysRepairable) {
  const auto a = addMark({-3, 15, -4});
  const auto b = addMark({-1, 15, -4});
  const auto c = addMark({1, 15, -4});
  const auto d = addMark({3, 15, -4});
  const auto route_id = addRoute({a, b, c, d});
  const auto deleted_id =
      std::get<CharacterMarkDefinition>(*editor.object(b)).id;
  editor.select(b);
  ASSERT_TRUE(editor.removeSelected());
  editor.select(route_id);
  EXPECT_EQ(editor.selection(), route_id);
  EXPECT_EQ(pickEditorObject(editor, {{0, 15.65F, 0}, {0, 0, -1}}),
            editor_no_object);
  EXPECT_EQ(pickEditorObject(editor, {{2, 15.65F, 0}, {0, 0, -1}}), route_id);
  const auto labels = buildEditorCharacterOverlayLabels(editor, camera());
  const auto missing =
      std::find_if(labels.begin(), labels.end(), [&](const auto& label) {
        return label.object == route_id &&
               label.text.find("2: missing mark '" + deleted_id + "'") !=
                   std::string::npos;
      });
  ASSERT_NE(missing, labels.end());
  const auto anchor = editorCharacterRouteHandle(
      std::get<CharacterMarkDefinition>(*editor.object(a)));
  const auto projected = projectEditorLine(camera(), anchor, anchor, {});
  ASSERT_TRUE(projected);
  EXPECT_EQ(missing->position, projected->first);
  auto route = std::get<CharacterRouteDefinition>(*editor.object(route_id));
  route.marks = {"missing-one", "missing-two"};
  ASSERT_TRUE(editor.replaceObject(route_id, route));
  EXPECT_EQ(editor.selection(), route_id);
  const auto unresolved = buildEditorCharacterOverlayLabels(editor, camera());
  EXPECT_TRUE(std::none_of(
      unresolved.begin(), unresolved.end(),
      [&](const auto& label) { return label.object == route_id; }));
  EXPECT_EQ(pickEditorObject(editor, {{2, 15.65F, 0}, {0, 0, -1}}),
            editor_no_object);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(pickEditorObject(editor, {{2, 15.65F, 0}, {0, 0, -1}}), route_id);
}

TEST_F(EditorCharacterSpatial,
       MissingModelUsesResolvedMarkAndNeverInventsStart) {
  static_cast<void>(addMark({0, 15, -4}));
  ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
  const auto actor_id = editor.selection();
  auto actor = std::get<CharacterActorDefinition>(*editor.object(actor_id));
  actor.model = "unknown-model";
  ASSERT_TRUE(editor.replaceObject(actor_id, actor));
  EXPECT_EQ(pickEditorObject(editor, {{0, 16.1F, 0}, {0, 0, -1}}), actor_id);
  const auto labels = buildEditorCharacterOverlayLabels(editor, camera());
  EXPECT_TRUE(std::any_of(labels.begin(), labels.end(), [&](const auto& label) {
    return label.object == actor_id &&
           label.text.find("missing model 'unknown-model'") !=
               std::string::npos;
  }));
  actor.initial_mark = "unresolved-start";
  ASSERT_TRUE(editor.replaceObject(actor_id, actor));
  EXPECT_EQ(pickEditorObject(editor, {{0, 16.1F, 0}, {0, 0, -1}}),
            editor_no_object);
  EXPECT_EQ(editor.selection(), actor_id);
  EXPECT_TRUE(editor.object(actor_id));
  const auto unresolved = buildEditorCharacterOverlayLabels(editor, camera());
  EXPECT_TRUE(std::none_of(
      unresolved.begin(), unresolved.end(),
      [&](const auto& label) { return label.object == actor_id; }));
}
