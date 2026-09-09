#include <gtest/gtest.h>

#include "core/world/characters.hpp"
#include "editor/editor_picking.hpp"

namespace {
class EditorCharacterPlacement : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(editor.open("resources/levels/scripted-characters.level.json"));
    actor_id = editor.characterIds(EditorCharacterKind::Actor).front();
    editor.select(actor_id);
    mark_id = editor.placementTarget();
    ASSERT_NE(mark_id, editor_no_object);
  }

  void addFloors() {
    ASSERT_TRUE(
        editor.addSolid({{40, -.25F, 40}, {5, .25F, 5}, {255, 255, 255, 255}}));
    lower_floor = editor.selection();
    ASSERT_TRUE(
        editor.addSolid({{40, 4.75F, 40}, {5, .25F, 5}, {255, 255, 255, 255}}));
    upper_floor = editor.selection();
    editor.select(actor_id);
  }

  CharacterMarkDefinition mark() const {
    return std::get<CharacterMarkDefinition>(*editor.object(mark_id));
  }

  EditorDocument editor;
  EditorObjectId actor_id{}, mark_id{}, lower_floor{}, upper_floor{};
};
}  // namespace

TEST_F(EditorCharacterPlacement,
       UpperFloorUsesExactFeetHitAndSharedConsumersUndoTogether) {
  addFloors();
  const auto initial_mark = mark();
  editor.select(mark_id);
  ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
  const auto second_actor = editor.selection();
  ASSERT_EQ(std::get<CharacterActorDefinition>(*editor.object(second_actor))
                .initial_mark,
            initial_mark.id);
  const auto route_id = editor.characterIds(EditorCharacterKind::Route).front();
  auto route = std::get<CharacterRouteDefinition>(*editor.object(route_id));
  route.marks = {initial_mark.id, initial_mark.id};
  ASSERT_TRUE(editor.replaceObject(route_id, route));
  editor.select(actor_id);
  const auto before = *editor.document();
  const auto selection_revision = editor.selectionRevision();
  const EditorRay ray{{40, 12, 40}, {0, -1, 0}};
  const auto hit = updateEditorPlacementViewport(
      editor, ray, false, false, true, EditorPlacementMode::SceneSurfaces,
      {3, .8F, .2F});
  ASSERT_TRUE(hit);
  EXPECT_EQ(hit->target, upper_floor);
  EXPECT_NE(hit->target, lower_floor);
  EXPECT_EQ(hit->face, EditorSurfaceFace::Top);
  EXPECT_EQ(hit->position, (WorldPosition{40, 5, 40}));
  EXPECT_EQ(mark().feet_position, hit->position);
  EXPECT_EQ(mark().yaw_degrees, initial_mark.yaw_degrees);
  EXPECT_EQ(editor.selection(), actor_id);
  EXPECT_EQ(editor.selectionRevision(), selection_revision);
  auto expected = before;
  for (auto& value : expected.characters.marks)
    if (value.id == initial_mark.id) value.feet_position = hit->position;
  EXPECT_EQ(*editor.document(), expected);
  for (const auto id : {actor_id, second_actor}) {
    const auto actor = std::get<CharacterActorDefinition>(*editor.object(id));
    const auto* shared =
        findCharacterMark(editor.document()->characters, actor.initial_mark);
    ASSERT_NE(shared, nullptr);
    EXPECT_EQ(shared->feet_position, hit->position);
  }
  for (const auto& reference :
       editor.document()->characters.routes.front().marks) {
    const auto* shared =
        findCharacterMark(editor.document()->characters, reference);
    ASSERT_NE(shared, nullptr);
    EXPECT_EQ(shared->feet_position, hit->position);
  }
  // A held gesture supplies no new press; its changing hover cannot add edits.
  const auto placed_revision = editor.revision();
  EXPECT_TRUE(updateEditorPlacementViewport(
      editor, EditorRay{{41, 12, 40}, {0, -1, 0}}, false, false, false,
      EditorPlacementMode::SceneSurfaces, {}));
  EXPECT_EQ(*editor.document(), expected);
  EXPECT_EQ(editor.revision(), placed_revision);
  EXPECT_FALSE(editor.placeSelected(*hit, {}));
  EXPECT_EQ(editor.revision(), placed_revision);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  EXPECT_EQ(editor.selection(), actor_id);
  ASSERT_TRUE(editor.redo());
  EXPECT_EQ(*editor.document(), expected);
  EXPECT_EQ(editor.selection(), actor_id);
}

TEST_F(EditorCharacterPlacement,
       SceneMarkPlacementUsesTheSameSingleUndoableFeetEdit) {
  addFloors();
  editor.select(mark_id);
  const auto before = *editor.document();
  const auto original_mark = mark();
  const auto hit = updateEditorPlacementViewport(
      editor, EditorRay{{41, 10, 41}, {0, -1, 0}}, false, false, true,
      EditorPlacementMode::SceneSurfaces, {});
  ASSERT_TRUE(hit);
  EXPECT_EQ(mark().feet_position, (WorldPosition{41, 5, 41}));
  EXPECT_EQ(mark().id, original_mark.id);
  EXPECT_EQ(mark().yaw_degrees, original_mark.yaw_degrees);
  EXPECT_EQ(editor.selection(), mark_id);
  EXPECT_EQ(editor.document()->characters.actors, before.characters.actors);
  EXPECT_EQ(editor.document()->characters.routes, before.characters.routes);
  ASSERT_TRUE(editor.undo());
  EXPECT_EQ(*editor.document(), before);
  EXPECT_EQ(editor.selection(), mark_id);
}

TEST_F(EditorCharacterPlacement,
       NearestWallAndUndersideBlockWithoutSearchingThrough) {
  addFloors();
  ASSERT_TRUE(
      editor.addSolid({{40, 2, 38}, {2, 2, .1F}, {255, 255, 255, 255}}));
  const auto wall = editor.selection();
  for (const auto selected : {actor_id, mark_id}) {
    editor.select(selected);
    const auto before = *editor.document();
    const auto revision = editor.revision();
    const bool can_undo = editor.canUndo(), can_redo = editor.canRedo();
    const auto wall_hit = updateEditorPlacementViewport(
        editor, EditorRay{{40, 3, 35}, {0, -.5F, 1}}, false, false, true,
        EditorPlacementMode::SceneSurfaces, {});
    ASSERT_TRUE(wall_hit);
    EXPECT_EQ(wall_hit->target, wall);
    EXPECT_EQ(wall_hit->face, EditorSurfaceFace::NegativeZ);
    EXPECT_FALSE(editor.placeSelected(*wall_hit, {}));
    EXPECT_EQ(*editor.document(), before);
    const auto underside_hit = updateEditorPlacementViewport(
        editor, EditorRay{{40, 3, 40}, {0, 1, 0}}, false, false, true,
        EditorPlacementMode::SceneSurfaces, {});
    ASSERT_TRUE(underside_hit);
    EXPECT_EQ(underside_hit->target, upper_floor);
    EXPECT_EQ(underside_hit->face, EditorSurfaceFace::Bottom);
    EXPECT_FALSE(editor.placeSelected(*underside_hit, {}));
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.revision(), revision);
    EXPECT_EQ(editor.selection(), selected);
    EXPECT_EQ(editor.canUndo(), can_undo);
    EXPECT_EQ(editor.canRedo(), can_redo);
  }
}

TEST_F(EditorCharacterPlacement,
       CaptureNavigationMissAndHoverPreserveDocumentAndHistory) {
  addFloors();
  const EditorRay ray{{40, 12, 40}, {0, -1, 0}};
  for (const auto selected : {actor_id, mark_id}) {
    editor.select(selected);
    const auto before = *editor.document();
    const auto revision = editor.revision();
    const bool can_undo = editor.canUndo(), can_redo = editor.canRedo(),
               dirty = editor.dirty();
    EXPECT_FALSE(
        updateEditorPlacementViewport(editor, ray, true, false, true,
                                      EditorPlacementMode::SceneSurfaces, {}));
    EXPECT_FALSE(
        updateEditorPlacementViewport(editor, ray, false, true, true,
                                      EditorPlacementMode::SceneSurfaces, {}));
    EXPECT_FALSE(
        updateEditorPlacementViewport(editor, std::nullopt, false, false, true,
                                      EditorPlacementMode::SceneSurfaces, {}));
    EXPECT_FALSE(updateEditorPlacementViewport(
        editor, EditorRay{{40, 12, 40}, {0, 1, 0}}, false, false, true,
        EditorPlacementMode::SceneSurfaces, {}));
    EXPECT_TRUE(
        updateEditorPlacementViewport(editor, ray, false, false, false,
                                      EditorPlacementMode::SceneSurfaces, {}));
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.revision(), revision);
    EXPECT_EQ(editor.selection(), selected);
    EXPECT_EQ(editor.canUndo(), can_undo);
    EXPECT_EQ(editor.canRedo(), can_redo);
    EXPECT_EQ(editor.dirty(), dirty);
  }
}

TEST_F(EditorCharacterPlacement,
       MissingInitialMarkLeavesActorSelectableWithoutInventingPlacement) {
  addFloors();
  auto actor = std::get<CharacterActorDefinition>(*editor.object(actor_id));
  actor.initial_mark = "missing-placement";
  ASSERT_TRUE(editor.replaceObject(actor_id, actor));
  EXPECT_EQ(editor.placementTarget(), editor_no_object);
  const auto before = *editor.document();
  const auto revision = editor.revision();
  const auto hit = pickEditorSurface(editor, {{40, 12, 40}, {0, -1, 0}},
                                     EditorPlacementMode::SceneSurfaces);
  ASSERT_TRUE(hit);
  EXPECT_FALSE(editor.placeSelected(*hit, {}));
  static_cast<void>(updateEditorPlacementViewport(
      editor, EditorRay{{40, 12, 40}, {0, -1, 0}}, false, false, true,
      EditorPlacementMode::SceneSurfaces, {}));
  EXPECT_EQ(*editor.document(), before);
  EXPECT_EQ(editor.revision(), revision);
  EXPECT_EQ(editor.selection(), actor_id);
  EXPECT_EQ(editor.object(actor_id), EditorObjectValue(actor));
}

TEST_F(EditorCharacterPlacement,
       TerrainPlacesActorAndMarkAtSupportedFeetWithoutHeightOffsets) {
  ASSERT_TRUE(editor.open("resources/levels/prototype.level.json"));
  ASSERT_TRUE(editor.addCharacter(EditorCharacterKind::Actor));
  actor_id = editor.selection();
  mark_id = editor.placementTarget();
  ASSERT_NE(mark_id, editor_no_object);
  ASSERT_TRUE(editor.document()->terrain);
  for (const auto selected : {actor_id, mark_id}) {
    editor.select(selected);
    const auto before = *editor.document();
    const auto old_mark = mark();
    const EditorRay ray{{15, 20, 15}, {0, -1, 0}};
    const auto terrain_hit = pickEditorTerrain(*before.terrain, ray);
    ASSERT_TRUE(terrain_hit);
    EXPECT_GT(terrain_hit->normal.y, 0);
    const auto hit = updateEditorPlacementViewport(
        editor, ray, false, false, true, EditorPlacementMode::TerrainOnly,
        {7, 3, 2});
    ASSERT_TRUE(hit);
    EXPECT_EQ(hit->face, EditorSurfaceFace::Terrain);
    EXPECT_EQ(mark().feet_position, terrain_hit->position);
    EXPECT_EQ(mark().yaw_degrees, old_mark.yaw_degrees);
    EXPECT_EQ(editor.selection(), selected);
    auto expected = before;
    for (auto& value : expected.characters.marks)
      if (value.id == old_mark.id) value.feet_position = terrain_hit->position;
    EXPECT_EQ(*editor.document(), expected);
    ASSERT_TRUE(editor.undo());
    EXPECT_EQ(*editor.document(), before);
    EXPECT_EQ(editor.selection(), selected);
  }
}
