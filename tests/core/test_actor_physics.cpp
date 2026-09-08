#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "core/gameplay/authored_interaction.hpp"
#include "core/physics/physics_world.hpp"

namespace {
LevelDocument actorDocument(std::size_t count = 1) {
  LevelDocument d;
  d.solids = {{{0, -.25F, 0},
               {10, .25F, 10},
               {255, 255, 255, 255},
               PrototypeSolidKind::Floor}};
  d.entries = {{"default", {{-4, 0, -4}, 0}}};
  d.default_entry = "default";
  for (std::size_t i = 0; i < count; ++i) {
    const auto id = "actor-" + std::to_string(i);
    d.characters.marks.push_back({id, {float(i) * 2, 0, 0}, float(i) * 45});
    d.characters.actors.push_back({id, "test-mannequin", id, {}, 1, {}, {}});
  }
  return d;
}

class ActorFailure {
 public:
  explicit ActorFailure(const std::string& stage) { set(stage.c_str()); }
  ~ActorFailure() { set(""); }

 private:
  static void set(const char* stage) {
#if defined(_WIN32)
    (void)_putenv_s("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE", stage);
#else
    if (*stage)
      (void)setenv("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE", stage, 1);
    else
      (void)unsetenv("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE");
#endif
  }
};
}  // namespace

TEST(ActorPhysics, ZeroAndFourProxiesKeepAuthoredInitialPoses) {
  for (const std::size_t count : {0U, 4U}) {
    const auto d = actorDocument(count);
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    EXPECT_EQ(physics.actorCount(), count);
    EXPECT_EQ(physics.staticBodyCount(), 1U);
    EXPECT_THROW((void)physics.actorState(count), std::out_of_range);
    for (int step = 0; step < 60; ++step) {
      physics.advanceWorld(1.F / 60);
      (void)physics.stepCharacter({{0, -.3F, 0}}, 1.F / 60);
    }
    for (std::size_t i = 0; i < count; ++i) {
      EXPECT_EQ(physics.actorState(i).feet_position,
                d.characters.marks[i].feet_position);
      EXPECT_EQ(physics.actorState(i).yaw_degrees,
                d.characters.marks[i].yaw_degrees);
    }
  }
}

TEST(ActorPhysics, EachPartialConstructionFailureAllowsFreshWorld) {
  const auto level = makePrototypeLevel(actorDocument(4));
  for (int cycle = 0; cycle < 2; ++cycle) {
    for (int i = 1; i <= 4; ++i) {
      {
        ActorFailure failure("actor-body-" + std::to_string(i));
        try {
          PhysicsWorld failed(level);
          FAIL() << "Expected injected actor creation failure";
        } catch (const std::runtime_error& error) {
          EXPECT_NE(
              std::string(error.what()).find("actor-" + std::to_string(i - 1)),
              std::string::npos);
        }
      }
      PhysicsWorld recovered(level);
      EXPECT_EQ(recovered.actorCount(), 4U);
      recovered.advanceWorld(1.F / 60);
      EXPECT_TRUE(recovered.usesSingleThreadedJobs());
    }
  }
}

TEST(ActorPhysics, SweepsThinWallAndReportsOnlyAcceptedDistance) {
  auto d = actorDocument();
  d.solids.push_back({{0, 1.5F, 2},
                      {2, 1.5F, .005F},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::Boundary});
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  physics.advanceWorld(1.F / 60);
  const auto moved = physics.advanceActor(0, {0, 0, 4}, 0);
  EXPECT_EQ(moved.obstruction, PhysicsActorObstruction::Static);
  EXPECT_GT(moved.horizontal_distance, 1.7F);
  EXPECT_LT(moved.state.feet_position.z, 1.75F);
  EXPECT_NEAR(moved.horizontal_distance, moved.state.feet_position.z, .0001F);
  EXPECT_NEAR(moved.state.feet_position.y, 0, .001F);
  const auto waiting = physics.advanceActor(0, {0, 0, .025F}, 0);
  EXPECT_LT(waiting.horizontal_distance, .001F);
  EXPECT_EQ(physics.actorState(0).feet_position, waiting.state.feet_position);
}

TEST(ActorPhysics, GroundedStairsAscendAndDescendWithoutExtraHorizontalTravel) {
  auto d = actorDocument();
  for (int i = 0; i < 4; ++i)
    d.solids.push_back({{0, .1F * (i + 1), 1.3F + .6F * i},
                        {2, .1F * (i + 1), .3F},
                        {255, 255, 255, 255},
                        PrototypeSolidKind::WalkableStep});
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  auto moved = physics.advanceActor(0, {0, 0, 3.1F}, 0);
  EXPECT_EQ(moved.obstruction, PhysicsActorObstruction::None);
  EXPECT_NEAR(moved.state.feet_position.z, 3.1F, .001F);
  EXPECT_NEAR(moved.state.feet_position.y, .8F, .005F);
  EXPECT_NEAR(moved.horizontal_distance, 3.1F, .001F);
  moved = physics.advanceActor(0, {0, 0, -3.1F}, 180);
  EXPECT_EQ(moved.obstruction, PhysicsActorObstruction::None);
  EXPECT_NEAR(moved.state.feet_position.z, 0, .001F);
  EXPECT_NEAR(moved.state.feet_position.y, 0, .005F);
}

TEST(ActorPhysics, OverHeightStepAndUnsupportedGapRetainSafePose) {
  for (bool gap : {false, true}) {
    auto d = actorDocument();
    if (gap) {
      d.solids[0].center.z = -4.5F;
      d.solids[0].half_extent.z = 5.5F;
      d.solids.push_back({{0, -.25F, 5},
                          {10, .25F, 2},
                          {255, 255, 255, 255},
                          PrototypeSolidKind::Floor});
    } else {
      d.solids.push_back({{0, .2F, 2},
                          {2, .2F, 1},
                          {255, 255, 255, 255},
                          PrototypeSolidKind::WalkableStep});
    }
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    const auto moved = physics.advanceActor(0, {0, 0, 5}, 0);
    EXPECT_NE(moved.obstruction, PhysicsActorObstruction::None);
    EXPECT_LT(moved.state.feet_position.z, 1.3F);
    EXPECT_GE(moved.state.feet_position.y, -.301F);
    const auto waiting = physics.advanceActor(0, {0, 0, .025F}, 0);
    EXPECT_LT(waiting.horizontal_distance, .001F);
  }
}

TEST(ActorPhysics, SupportedTerrainSlopePreservesAuthoredFeetArrivalTolerance) {
  auto d = actorDocument();
  PrototypeTerrain terrain;
  terrain.origin = {-24, 0, -24};
  terrain.sample_spacing = .5F;
  const float slope = std::tan((player_maximum_slope_degrees - 1) *
                               3.14159265358979323846F / 180);
  for (std::size_t z = 0; z < prototype_terrain_sample_count; ++z)
    for (std::size_t x = 0; x < prototype_terrain_sample_count; ++x) {
      const float world_z =
          terrain.origin.z + float(z) * terrain.sample_spacing;
      terrain.heights[z * prototype_terrain_sample_count + x] =
          std::clamp(world_z, -3.F, 3.F) * slope;
    }
  d.terrain = terrain;
  d.solids[0].center = {15, -.25F, 15};
  d.solids[0].half_extent = {1, .25F, 1};
  d.entries[0].pose.foot_position = {15, 3 * slope, 18};
  d.characters.marks.push_back({"slope-end", {0, slope, 1}, 0});
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  const auto moved = physics.advanceActor(0, {0, 0, 1}, 0);
  EXPECT_EQ(moved.obstruction, PhysicsActorObstruction::None);
  EXPECT_NEAR(moved.state.feet_position.z, 1, .001F);
  EXPECT_NEAR(moved.state.feet_position.y,
              d.characters.marks.back().feet_position.y, .02F);
  const auto upper = physics.advanceActor(0, {0, 0, 3}, 0);
  EXPECT_EQ(upper.obstruction, PhysicsActorObstruction::None);
  EXPECT_NEAR(upper.state.feet_position.z, 4, .001F);
  EXPECT_NEAR(upper.state.feet_position.y, 3 * slope, .002F);
  const auto lower = physics.advanceActor(0, {0, 0, -8}, 180);
  EXPECT_EQ(lower.obstruction, PhysicsActorObstruction::None);
  EXPECT_NEAR(lower.state.feet_position.z, -4, .001F);
  EXPECT_NEAR(lower.state.feet_position.y, -3 * slope, .002F);

  d.solids.push_back({{0, 2.F, 0},
                      {.3F, .075F, .3F},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::LowClearance});
  const auto diagnostics = validateLevelDocument(d);
  EXPECT_TRUE(std::any_of(
      diagnostics.begin(), diagnostics.end(), [](const auto& error) {
        return error.document_path.starts_with("characters.actors[0]");
      }));
  EXPECT_THROW((void)makePrototypeLevel(d), std::exception);
}

TEST(ActorPhysics, PropsBlockTravelAndCannotSupplyStepSupport) {
  auto d = actorDocument();
  d.props = {{"proxy",
              "prototype-chair",
              {0, 0, 2},
              0,
              1,
              {{{0, .1F, 0}, {1, .1F, .01F}}}}};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  const auto moved = physics.advanceActor(0, {0, 0, 4}, 0);
  EXPECT_NE(moved.obstruction, PhysicsActorObstruction::None);
  EXPECT_GT(moved.horizontal_distance, 1.7F);
  EXPECT_LT(moved.state.feet_position.z, 2);
  EXPECT_NEAR(moved.state.feet_position.y, 0, .002F);
}

TEST(ActorPhysics,
     StepsJustAboveTheLimitCannotBeClimbedByRepeatedCapsuleContacts) {
  for (float height : {.301F, .31F, .4F}) {
    auto d = actorDocument();
    d.solids.push_back({{0, height / 2, 2},
                        {2, height / 2, 1},
                        {255, 255, 255, 255},
                        PrototypeSolidKind::WalkableStep});
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    for (int i = 0; i < 200; ++i) {
      physics.advanceWorld(1.F / 60);
      (void)physics.advanceActor(0, {0, 0, .025F}, 0);
    }
    EXPECT_LT(physics.actorState(0).feet_position.z, 1) << height;
  }
}

TEST(ActorPhysics, SweptStairLiftCannotCrossALowCeiling) {
  auto d = actorDocument();
  d.solids.push_back({{0, .1F, 2},
                      {2, .1F, 1},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::WalkableStep});
  d.solids.push_back({{0, 2.F, 1},
                      {2, .1F, 1},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::LowClearance});
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  const auto moved = physics.advanceActor(0, {0, 0, 4}, 0);
  EXPECT_NE(moved.obstruction, PhysicsActorObstruction::None);
  EXPECT_LT(moved.state.feet_position.z, 1);
  EXPECT_LT(moved.state.feet_position.y, .05F);
}

TEST(ActorPhysics, PlayerCannotWalkThroughOrRemainStandingOnActor) {
  auto d = actorDocument();
  d.solids.push_back({{-1, 2.5F, 0},
                      {.6F, .1F, 1},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::Floor});
  d.entries[0].pose.foot_position = {-1, 2.6F, 0};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  auto state = physics.characterState();
  bool falling_beside_actor = false;
  for (int step = 0; step < 300; ++step) {
    physics.advanceWorld(1.F / 60);
    const float vertical =
        (state.supported() ? 0 : state.linear_velocity.y) - .3F;
    const float horizontal = step < 60 ? 1.2F : state.linear_velocity.x;
    state = physics.stepCharacter(
        {{horizontal, vertical, state.linear_velocity.z}}, 1.F / 60);
    if (state.foot_position.y > 1.7F && state.foot_position.y < 2.3F &&
        std::hypot(state.foot_position.x, state.foot_position.z) < .65F) {
      falling_beside_actor = true;
      EXPECT_FALSE(state.supported());
    }
  }
  EXPECT_TRUE(falling_beside_actor);
  EXPECT_NEAR(state.foot_position.y, 0, .05F);
  EXPECT_GE(std::hypot(state.foot_position.x, state.foot_position.z), .57F);
  EXPECT_EQ(physics.actorState(0).feet_position,
            d.characters.marks[0].feet_position);
}

TEST(ActorPhysics, AcceptedActorBlocksInteractionUntilItMovesAway) {
  auto d = actorDocument();
  DoorDefinition door;
  door.id = "door";
  door.hinge_position = {-.45F, .02F, 1};
  d.doors = {door};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  DoorController doors(level.doors());
  LightSwitchController lights(level.environmentLight(), level.lightSwitches());
  AuthoredInteraction interaction;
  const PlayerViewPose view{{0, 1.2F, -.65F}, {0, 0, 1}};
  EXPECT_FALSE(physics.staticSegmentBlocked({0, 1.2F, -.65F}, {0, 1.2F, .9F}));
  EXPECT_TRUE(physics.worldSegmentBlocked({0, 1.2F, -.65F}, {0, 1.2F, .9F}));
  EXPECT_TRUE(physics.worldSegmentBlocked({0, 1.2F, 0}, {0, 1.2F, -.65F}));
  for (int action = 0; action < 3; ++action) {
    (void)interaction.update({}, true, view, level, physics, doors, lights);
    PlayerActionSnapshot input;
    input.interact = action == 0;
    input.lock = action == 1;
    input.secondary_action = action == 2;
    EXPECT_FALSE(
        interaction.update(input, true, view, level, physics, doors, lights));
    EXPECT_FALSE(doors.state(0).moving);
  }
  (void)physics.advanceActor(0, {2, 0, 0}, 0);
  (void)interaction.update({}, true, view, level, physics, doors, lights);
  PlayerActionSnapshot input;
  input.interact = true;
  EXPECT_TRUE(
      interaction.update(input, true, view, level, physics, doors, lights));
  EXPECT_TRUE(doors.state(0).moving);
}

TEST(ActorPhysics, ActorObstructsSwitchWithoutBecomingAnInteractionTarget) {
  auto d = actorDocument();
  d.environment_light.point_lights = {{{0, 3, 0}, {1, 1, 1}, 1, 10, "light"}};
  d.light_switches = {{{0, 1.2F, 1}, 0, "light", "switch"}};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  DoorController doors(level.doors());
  LightSwitchController lights(level.environmentLight(), level.lightSwitches());
  AuthoredInteraction interaction;
  const PlayerViewPose view{{0, 1.2F, -.65F}, {0, 0, 1}};
  PlayerActionSnapshot input;
  input.interact = true;
  (void)interaction.update({}, true, view, level, physics, doors, lights);
  EXPECT_FALSE(
      interaction.update(input, true, view, level, physics, doors, lights));
  EXPECT_EQ(lights.pointLightEnabled()[0], 1);
  (void)physics.advanceActor(0, {2, 0, 0}, 0);
  (void)interaction.update({}, true, view, level, physics, doors, lights);
  (void)interaction.update(input, true, view, level, physics, doors, lights);
  EXPECT_EQ(lights.pointLightEnabled()[0], 0);
}

TEST(ActorPhysics, ActorCapsuleRejectsStandingAndClearanceAllowsItAgain) {
  auto d = actorDocument();
  d.characters.marks[0].feet_position.y = 1.3F;
  d.solids.push_back({{0, .65F, 0},
                      {.02F, .65F, .02F},
                      {255, 255, 255, 255},
                      PrototypeSolidKind::Floor});
  d.entries[0].pose.foot_position = {1.5F, 0, 0};
  const auto level = makePrototypeLevel(d);
  PhysicsWorld physics(level);
  PhysicsCharacterState state;
  for (int step = 0; step < 60; ++step) {
    physics.advanceWorld(1.F / 60);
    state = physics.stepCharacter({{-1, -.3F, 0}, {0, -18, 0}, true}, 1.F / 60);
  }
  EXPECT_NEAR(state.foot_position.x, .5F, .025F);
  state = physics.stepCharacter({{0, -.3F, 0}, {0, -18, 0}, false}, 1.F / 60);
  EXPECT_EQ(state.stance, PhysicsPlayerStance::Crouched);
  for (int step = 0; step < 60; ++step) {
    physics.advanceWorld(1.F / 60);
    state = physics.stepCharacter({{1, -.3F, 0}, {0, -18, 0}, false}, 1.F / 60);
  }
  EXPECT_EQ(state.stance, PhysicsPlayerStance::Standing);
  EXPECT_EQ(physics.actorState(0).feet_position,
            d.characters.marks[0].feet_position);
}

TEST(ActorPhysics, ActorBatchesUseDurablePriorityAndProtectCrossedPaths) {
  std::array<WorldPosition, 4> first_run;
  for (bool reversed : {false, true}) {
    auto d = actorDocument(4);
    d.characters.marks[0].feet_position = {-1, 0, 0};
    d.characters.marks[1].feet_position = {0, 0, -1};
    if (reversed)
      std::reverse(d.characters.actors.begin(), d.characters.actors.end());
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    std::array<PhysicsActorMotion, 4> moves{
        {{{2, 0, 0}, 90}, {{0, 0, 2}, 0}, {{}, 90}, {{}, 135}}};
    if (reversed) std::reverse(moves.begin(), moves.end());
    physics.advanceWorld(1.F / 60);
    const auto results = physics.advanceActors(moves);
    for (std::size_t i = 0; i < 4; ++i) {
      const auto ordered = reversed ? 3 - i : i;
      if (!reversed)
        first_run[i] = results[ordered].state.feet_position;
      else
        EXPECT_EQ(results[ordered].state.feet_position, first_run[i]);
    }
    const auto blocked = reversed ? 2 : 1;
    EXPECT_EQ(results[blocked].obstruction, PhysicsActorObstruction::Actor);
    EXPECT_LT(results[blocked].state.feet_position.z, -.49F);
  }
}

TEST(ActorPhysics, PlayerSweptPathAndOpposingActorsPreventSwaps) {
  {
    auto d = actorDocument();
    d.characters.marks[0].feet_position = {0, 0, -1};
    d.entries[0].pose.foot_position = {-2, 0, 0};
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    physics.advanceWorld(1.F / 60);
    const auto player = physics.stepCharacter({{240, -.3F, 0}}, 1.F / 60);
    EXPECT_GT(player.foot_position.x, 1);
    const auto actor = physics.advanceActor(0, {0, 0, 2}, 0);
    EXPECT_EQ(actor.obstruction, PhysicsActorObstruction::Player);
    EXPECT_LT(actor.state.feet_position.z, -.6F);
  }
  {
    auto d = actorDocument(2);
    d.characters.marks[0].feet_position = {-1, 0, 0};
    d.characters.marks[1].feet_position = {1, 0, 0};
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    const std::array<PhysicsActorMotion, 2> moves{
        {{{.025F, 0, 0}, 90}, {{-.025F, 0, 0}, 270}}};
    for (int i = 0; i < 100; ++i) {
      physics.advanceWorld(1.F / 60);
      (void)physics.advanceActors(moves);
    }
    const auto before = physics.actorState(0).feet_position;
    const auto results = physics.advanceActors(moves);
    EXPECT_EQ(results[0].obstruction, PhysicsActorObstruction::Actor);
    EXPECT_EQ(results[1].obstruction, PhysicsActorObstruction::Actor);
    EXPECT_LT(results[0].horizontal_distance, .001F);
    EXPECT_GE(results[1].state.feet_position.x - before.x, .499F);
  }
}

TEST(ActorPhysics, OpeningAndClosingDoorsStopAndRequireExplicitReactivation) {
  for (bool initially_open : {false, true}) {
    auto d = actorDocument();
    d.characters.marks[0].feet_position = {0, 0, -1};
    DoorDefinition door;
    door.id = "door";
    door.hinge_position = {-1, .02F, 0};
    door.width = 2;
    door.initially_open = initially_open;
    d.doors = {door};
    const auto level = makePrototypeLevel(d);
    PhysicsWorld physics(level);
    DoorController doors(level.doors());
    const auto steps = [&] {
      for (int i = 0; i < 100; ++i) {
        physics.advanceWorld(1.F / 60);
        (void)physics.stepCharacter({{0, -.3F, 0}}, 1.F / 60);
        doors.fixedStep(1.F / 60, physics);
      }
    };
    (void)doors.act(0, DoorAction::Interact, {-4, 1.5F, -4});
    steps();
    EXPECT_FALSE(doors.state(0).moving);
    EXPECT_EQ(doors.state(0).feedback, DoorResultKind::Obstructed);
    const auto stopped = doors.state(0).angle;
    EXPECT_GT(stopped, 0);
    EXPECT_LT(stopped, 90);
    const auto cleared =
        physics.advanceActor(0, {0, 0, initially_open ? 2.F : -2.F}, 0);
    EXPECT_EQ(cleared.obstruction, PhysicsActorObstruction::None);
    steps();
    EXPECT_EQ(doors.state(0).angle, stopped);
    (void)doors.act(0, DoorAction::Interact, {-4, 1.5F, -4});
    steps();
    EXPECT_NE(doors.state(0).angle, stopped);
    EXPECT_EQ(doors.state(0).angle, physics.doorAngle(0));
  }
}
