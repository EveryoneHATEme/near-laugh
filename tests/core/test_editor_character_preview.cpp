#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

#include "core/world/characters.hpp"
#include "editor/editor_character_preview.hpp"

namespace {
void expectPoseNear(const CharacterPose& actual, const CharacterPose& expected,
                    double tolerance = 0.00001) {
  ASSERT_EQ(actual.size(), expected.size());
  for (std::size_t joint = 0; joint < actual.size(); ++joint)
    for (std::size_t component = 0; component < actual[joint].size();
         ++component)
      EXPECT_NEAR(actual[joint][component], expected[joint][component],
                  tolerance)
          << "joint " << joint << ", component " << component;
}

class EditorCharacterPreviewTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(
        document.open("resources/levels/scripted-characters.level.json"));
    actor_id = document.characterIds(EditorCharacterKind::Actor).front();
    route_id = document.characterIds(EditorCharacterKind::Route).front();
    document.select(actor_id);
    initial_mark_id = document.placementTarget();
    asset = loadCharacterAsset("resources/characters/test-mannequin.glb");
  }

  EditorCharacterPreviewRequest clipRequest(std::string clip = "idle") const {
    return {EditorCharacterPreviewMode::Clip, actor_id, 0, std::move(clip)};
  }

  EditorCharacterPreviewRequest routeRequest() const {
    return {EditorCharacterPreviewMode::Route, actor_id};
  }

  void configureRoute(bool final_action = true) {
    auto actor = std::get<CharacterActorDefinition>(*document.object(actor_id));
    ASSERT_EQ(actor.speed, 1);
    auto initial =
        std::get<CharacterMarkDefinition>(*document.object(initial_mark_id));
    initial.feet_position = {0, 0, 0};
    initial.yaw_degrees = -90;
    ASSERT_TRUE(document.replaceObject(initial_mark_id, initial));
    std::vector<std::string> marks;
    for (const auto& endpoint :
         {CharacterMarkDefinition{"", {1, 0, 0}, 0},
          CharacterMarkDefinition{"", {1, .3F, 1}, 180}}) {
      ASSERT_TRUE(document.addCharacter(EditorCharacterKind::Mark));
      const auto id = document.selection();
      auto mark = std::get<CharacterMarkDefinition>(*document.object(id));
      mark.feet_position = endpoint.feet_position;
      mark.yaw_degrees = endpoint.yaw_degrees;
      ASSERT_TRUE(document.replaceObject(id, mark));
      marks.push_back(mark.id);
    }
    auto route = std::get<CharacterRouteDefinition>(*document.object(route_id));
    route.actor = actor.id;
    route.marks = marks;
    route.final_clip =
        final_action ? std::optional<std::string>("interact") : std::nullopt;
    ASSERT_TRUE(document.replaceObject(route_id, route));
    document.select(route_id);
  }

  EditorDocument document;
  EditorCharacterPreview preview;
  std::shared_ptr<const CharacterAsset> asset;
  EditorObjectId actor_id{}, route_id{}, initial_mark_id{};
};
}  // namespace

TEST_F(EditorCharacterPreviewTest,
       ClipsPauseSeekRestartAndInterruptedBlendsMatchSharedSampler) {
  ASSERT_TRUE(preview.start(document, clipRequest(), asset));
  CharacterPlayback reference(asset);
  expectPoseNear(preview.pose(), reference.pose());
  for (const auto& clip : {"walk", "interact", "idle"}) {
    const auto before_transition = preview.pose();
    preview.selectClip(clip);
    reference.selectClip(clip);
    expectPoseNear(preview.pose(), before_transition);
    preview.advance(.04);
    static_cast<void>(reference.advance(.04));
    EXPECT_EQ(preview.clip(), reference.clip());
    EXPECT_DOUBLE_EQ(preview.time(), reference.time());
    expectPoseNear(preview.pose(), reference.pose());
  }
  preview.pause();
  reference.setPaused(true);
  const auto paused_pose = preview.pose();
  const auto paused_time = preview.time();
  preview.advance(10);
  EXPECT_TRUE(preview.paused());
  EXPECT_DOUBLE_EQ(preview.time(), paused_time);
  expectPoseNear(preview.pose(), paused_pose);
  preview.selectClip("interact");
  reference.selectClip("interact");
  preview.seek(preview.duration() * .63);
  reference.seek(characterClip(*asset, "interact").duration * .63);
  expectPoseNear(preview.pose(), reference.pose());
  preview.restart();
  reference.restart();
  EXPECT_TRUE(preview.paused());
  EXPECT_DOUBLE_EQ(preview.time(), 0);
  expectPoseNear(preview.pose(), reference.pose());
  preview.pause();
  reference.setPaused(false);
  preview.advance(.23);
  static_cast<void>(reference.advance(.23));
  expectPoseNear(preview.pose(), reference.pose());
}

TEST_F(EditorCharacterPreviewTest,
       ExplicitInspectionMarkAndClipCommandsPreserveAuthoredState) {
  ASSERT_TRUE(document.duplicateSelected());
  const auto other_actor_id = document.selection();
  const auto inspection_mark_id = document.placementTarget();
  const auto inspection_mark =
      std::get<CharacterMarkDefinition>(*document.object(inspection_mark_id));
  document.select(actor_id);
  const auto before = *document.document();
  const auto revision = document.revision();
  const auto selection_revision = document.selectionRevision();
  const bool dirty = document.dirty(), undo = document.canUndo(),
             redo = document.canRedo();
  auto request = clipRequest("walk");
  request.mark = inspection_mark_id;
  ASSERT_TRUE(preview.start(document, request, asset));
  EXPECT_EQ(preview.placement().position,
            (std::array<float, 3>{inspection_mark.feet_position.x,
                                  inspection_mark.feet_position.y,
                                  inspection_mark.feet_position.z}));
  EXPECT_EQ(preview.placement().yaw_degrees, inspection_mark.yaw_degrees);
  EXPECT_EQ(preview.actor(), actor_id);
  preview.advance(4);
  preview.pause();
  preview.selectClip("interact");
  preview.seek(preview.duration());
  preview.pause();
  preview.advance(100);
  preview.restart();
  preview.stop();
  EXPECT_EQ(*document.document(), before);
  EXPECT_EQ(document.document()->audio, before.audio);
  EXPECT_EQ(document.object(other_actor_id),
            EditorObjectValue(before.characters.actors.back()));
  EXPECT_EQ(document.revision(), revision);
  EXPECT_EQ(document.selectionRevision(), selection_revision);
  EXPECT_EQ(document.selection(), actor_id);
  EXPECT_EQ(document.dirty(), dirty);
  EXPECT_EQ(document.canUndo(), undo);
  EXPECT_EQ(document.canRedo(), redo);
}

TEST_F(EditorCharacterPreviewTest,
       OrderedRouteTurnsWhileStandingAndUsesCalibratedHorizontalTravel) {
  configureRoute();
  const auto route =
      std::get<CharacterRouteDefinition>(*document.object(route_id));
  const auto before = *document.document();
  ASSERT_TRUE(preview.start(document, routeRequest(), asset));
  EXPECT_EQ(preview.targetMark(), route.marks[0]);
  EXPECT_EQ(preview.finalClip(), "interact");
  preview.advance(.5);
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Heading);
  EXPECT_EQ(preview.placement().position, (std::array<float, 3>{0, 0, 0}));
  EXPECT_NEAR(preview.placement().yaw_degrees, -30, .0001);
  EXPECT_EQ(preview.clip(), "idle");
  preview.advance(1.5);
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Travel);
  EXPECT_NEAR(preview.placement().position[0], .5, .0001);
  EXPECT_EQ(preview.placement().position[1], 0);
  EXPECT_NEAR(preview.placement().yaw_degrees, 90, .0001);
  const auto walk_time = .5 / test_mannequin_catalog.walk_cycle_distance_m *
                         characterClip(*asset, "walk").duration;
  EXPECT_NEAR(preview.time(), walk_time, .00001);
  expectPoseNear(preview.pose(),
                 evaluateCharacterPose(
                     *asset, sampleCharacterPose(*asset, "walk", walk_time)));
  preview.advance(.875);
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Facing);
  EXPECT_EQ(preview.placement().position, (std::array<float, 3>{1, 0, 0}));
  EXPECT_NEAR(preview.placement().yaw_degrees, 45, .0001);
  EXPECT_EQ(preview.segment(), 0U);
  preview.advance(.875);
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Travel);
  EXPECT_EQ(preview.segment(), 1U);
  EXPECT_EQ(preview.targetMark(), route.marks[1]);
  EXPECT_NEAR(preview.placement().position[0], 1, .0001);
  EXPECT_NEAR(preview.placement().position[1], .15, .0001);
  EXPECT_NEAR(preview.placement().position[2], .5, .0001);
  EXPECT_NEAR(preview.placement().yaw_degrees, 0, .0001);
  const auto stair_time =
      std::fmod(1.5 / test_mannequin_catalog.walk_cycle_distance_m *
                    characterClip(*asset, "walk").duration,
                characterClip(*asset, "walk").duration);
  EXPECT_NEAR(preview.time(), stair_time, .00001);
  EXPECT_EQ(*document.document(), before);
}

TEST_F(EditorCharacterPreviewTest,
       RouteFinalActionCompletesSilentlyAndDoesNotRestart) {
  configureRoute();
  const auto before = *document.document();
  const auto revision = document.revision();
  ASSERT_TRUE(preview.start(document, routeRequest(), asset));
  preview.advance(5.85);
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Interaction);
  EXPECT_EQ(preview.clip(), "interact");
  EXPECT_NEAR(preview.time(), .1, .00001);
  EXPECT_EQ(preview.placement().position, (std::array<float, 3>{1, .3F, 1}));
  EXPECT_NEAR(preview.placement().yaw_degrees, 180, .0001);
  preview.advance(characterClip(*asset, "interact").duration + .2);
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Completed);
  const auto final_position = preview.placement().position;
  preview.advance(100);
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Completed);
  EXPECT_EQ(preview.placement().position, final_position);
  EXPECT_EQ(*document.document(), before);
  EXPECT_EQ(document.revision(), revision);
}

TEST_F(EditorCharacterPreviewTest,
       RouteWithoutFinalActionSupportsPauseAndExplicitRestartOnly) {
  configureRoute(false);
  ASSERT_TRUE(preview.start(document, routeRequest(), asset));
  EXPECT_EQ(preview.finalClip(), "None");
  preview.advance(2);
  preview.pause();
  const auto frozen = preview.pose();
  const auto position = preview.placement().position;
  const auto time = preview.time();
  preview.advance(50);
  preview.seek(10);
  preview.selectClip("interact");
  EXPECT_EQ(preview.clip(), "walk");
  EXPECT_EQ(preview.placement().position, position);
  EXPECT_DOUBLE_EQ(preview.time(), time);
  expectPoseNear(preview.pose(), frozen);
  preview.pause();
  preview.advance(10);
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Completed);
  EXPECT_EQ(preview.clip(), "idle");
  EXPECT_EQ(preview.placement().position, (std::array<float, 3>{1, .3F, 1}));
  preview.restart();
  EXPECT_EQ(preview.stage(), EditorRoutePreviewStage::Heading);
  EXPECT_EQ(preview.segment(), 0U);
  EXPECT_EQ(preview.placement().position, (std::array<float, 3>{0, 0, 0}));
  EXPECT_EQ(preview.placement().yaw_degrees, -90);
  EXPECT_DOUBLE_EQ(preview.time(), 0);
  expectPoseNear(preview.pose(), CharacterPlayback(asset).pose());
}

TEST_F(EditorCharacterPreviewTest,
       RouteBatchingRetainsPlacementFacingAndPoseAcrossStageBoundaries) {
  configureRoute();
  for (const double elapsed : {2.1, 3.8, 5.85, 12.}) {
    EditorCharacterPreview batched;
    ASSERT_TRUE(preview.start(document, routeRequest(), asset));
    ASSERT_TRUE(batched.start(document, routeRequest(), asset));
    preview.advance(elapsed);
    const int steps = static_cast<int>(std::round(elapsed * 100));
    for (int i = 0; i < steps; ++i) batched.advance(.01);
    EXPECT_EQ(batched.stage(), preview.stage());
    EXPECT_EQ(batched.segment(), preview.segment());
    EXPECT_EQ(batched.targetMark(), preview.targetMark());
    EXPECT_EQ(batched.clip(), preview.clip());
    for (std::size_t axis = 0; axis < 3; ++axis)
      EXPECT_NEAR(batched.placement().position[axis],
                  preview.placement().position[axis], .0001);
    EXPECT_NEAR(batched.placement().yaw_degrees,
                preview.placement().yaw_degrees, .001);
    EXPECT_NEAR(batched.time(), preview.time(), .0001);
    expectPoseNear(batched.pose(), preview.pose(), .0001);
  }
}

TEST_F(EditorCharacterPreviewTest,
       InvalidSelectedReferencesRefuseStartWithAffectedIdentity) {
  configureRoute();
  const auto original_route =
      std::get<CharacterRouteDefinition>(*document.object(route_id));
  const auto original_actor =
      std::get<CharacterActorDefinition>(*document.object(actor_id));
  const auto refuse = [&](std::string_view identity) {
    EXPECT_FALSE(preview.start(document, routeRequest(), asset));
    EXPECT_FALSE(preview.active());
    EXPECT_TRUE(preview.pose().empty());
    EXPECT_NE(preview.error().find(identity), std::string_view::npos);
  };
  auto route = original_route;
  route.actor = "missing-owner";
  ASSERT_TRUE(document.replaceObject(route_id, route));
  refuse("missing-owner");
  route = original_route;
  route.marks.push_back("missing-endpoint");
  ASSERT_TRUE(document.replaceObject(route_id, route));
  refuse("missing-endpoint");
  route = original_route;
  route.final_clip = "unsupported-final";
  ASSERT_TRUE(document.replaceObject(route_id, route));
  refuse("unsupported-final");
  ASSERT_TRUE(document.replaceObject(route_id, original_route));
  auto actor = original_actor;
  actor.model = "missing-model";
  ASSERT_TRUE(document.replaceObject(actor_id, actor));
  refuse("missing-model");
  actor = original_actor;
  actor.initial_mark = "missing-initial";
  ASSERT_TRUE(document.replaceObject(actor_id, actor));
  refuse("missing-initial");
  ASSERT_TRUE(document.replaceObject(actor_id, original_actor));
  ASSERT_TRUE(preview.start(document, routeRequest(), asset));
  EXPECT_FALSE(preview.start(document, routeRequest(), {}));
  EXPECT_FALSE(preview.active());
  EXPECT_FALSE(preview.error().empty());
  document.select(actor_id);
  EXPECT_FALSE(preview.start(document, clipRequest("missing-clip"), asset));
  EXPECT_NE(preview.error().find("missing-clip"), std::string_view::npos);
}

TEST_F(EditorCharacterPreviewTest,
       EditsUndoRedoAndRestoredDefinitionsStillInvalidateSnapshot) {
  auto mark =
      std::get<CharacterMarkDefinition>(*document.object(initial_mark_id));
  ASSERT_TRUE(preview.start(document, clipRequest(), asset));
  mark.yaw_degrees += 5;
  ASSERT_TRUE(document.replaceObject(initial_mark_id, mark));
  preview.synchronize(document);
  EXPECT_FALSE(preview.active());
  ASSERT_TRUE(document.undo());
  preview.synchronize(document);
  EXPECT_FALSE(preview.active());
  ASSERT_TRUE(preview.start(document, clipRequest(), asset));
  ASSERT_TRUE(document.redo());
  preview.synchronize(document);
  EXPECT_FALSE(preview.active());
  ASSERT_TRUE(preview.start(document, clipRequest(), asset));
  const auto before = *document.document();
  ASSERT_TRUE(document.undo());
  ASSERT_TRUE(document.redo());
  ASSERT_EQ(*document.document(), before);
  preview.synchronize(document);
  EXPECT_FALSE(preview.active());
}

TEST_F(EditorCharacterPreviewTest,
       SelectionRoundTripInvalidatesEvenWhenDocumentAndSelectionMatch) {
  const auto before = *document.document();
  const auto revision = document.revision();
  ASSERT_TRUE(preview.start(document, clipRequest(), asset));
  document.select(initial_mark_id);
  document.select(actor_id);
  EXPECT_EQ(*document.document(), before);
  EXPECT_EQ(document.revision(), revision);
  preview.synchronize(document);
  EXPECT_FALSE(preview.active());
  preview.advance(1);
  EXPECT_FALSE(preview.active());
}

TEST_F(EditorCharacterPreviewTest,
       ReplacementAndCanceledPendingCloseNeverResumeOldSnapshot) {
  ASSERT_TRUE(preview.start(document, clipRequest(), asset));
  ASSERT_TRUE(document.open("resources/levels/scripted-characters.level.json"));
  preview.synchronize(document);
  EXPECT_FALSE(preview.active());
  actor_id = document.characterIds(EditorCharacterKind::Actor).front();
  document.select(actor_id);
  auto actor = std::get<CharacterActorDefinition>(*document.object(actor_id));
  actor.speed = actor.speed == 1 ? .75F : 1;
  ASSERT_TRUE(document.replaceObject(actor_id, actor));
  ASSERT_TRUE(preview.start(document, clipRequest(), asset));
  document.requestClose();
  ASSERT_EQ(document.pendingAction().kind, EditorPendingActionKind::Close);
  preview.synchronize(document);
  EXPECT_FALSE(preview.active());
  ASSERT_TRUE(document.resolvePending(EditorPendingDecision::Cancel));
  preview.synchronize(document);
  preview.advance(1);
  EXPECT_FALSE(preview.active());
  EXPECT_TRUE(document.document());
}

TEST_F(EditorCharacterPreviewTest,
       StopDiscardsSnapshotAndLaterControlsRequireFreshStart) {
  ASSERT_TRUE(preview.start(document, clipRequest("walk"), asset));
  preview.advance(.2);
  preview.stop();
  EXPECT_FALSE(preview.active());
  EXPECT_TRUE(preview.pose().empty());
  EXPECT_EQ(preview.actor(), editor_no_object);
  preview.pause();
  preview.restart();
  preview.seek(.3);
  preview.selectClip("interact");
  preview.advance(10);
  preview.synchronize(document);
  EXPECT_FALSE(preview.active());
  ASSERT_TRUE(preview.start(document, clipRequest("idle"), asset));
  EXPECT_DOUBLE_EQ(preview.time(), 0);
  EXPECT_FALSE(preview.paused());
  expectPoseNear(preview.pose(), CharacterPlayback(asset).pose());
  const auto before = preview.pose();
  EXPECT_THROW(preview.advance(-1), std::invalid_argument);
  EXPECT_THROW(preview.advance(std::numeric_limits<double>::infinity()),
               std::invalid_argument);
  expectPoseNear(preview.pose(), before);
}
