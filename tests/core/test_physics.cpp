#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <stdexcept>
#include <string>
#include <thread>

#include "core/physics/physics_world.hpp"
#include "core/world/prototype_level.hpp"
#include "core/world/scene_assets.hpp"
#include "prototype_level_fixture.hpp"

namespace {
class ScopedPhysicsFailure {
 public:
  explicit ScopedPhysicsFailure(const char* stage) { set(stage); }
  ~ScopedPhysicsFailure() { set(""); }

  ScopedPhysicsFailure(const ScopedPhysicsFailure&) = delete;
  ScopedPhysicsFailure& operator=(const ScopedPhysicsFailure&) = delete;

 private:
  static void set(const char* stage) {
#if defined(_WIN32)
    if (_putenv_s("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE", stage) != 0) {
      throw std::runtime_error("Failed to configure physics failure stage");
    }
#else
    if (stage[0] == '\0') {
      if (unsetenv("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE") != 0) {
        throw std::runtime_error("Failed to clear physics failure stage");
      }
    } else if (setenv("NEAR_LAUGH_FORCE_PHYSICS_FAILURE_STAGE", stage, 1) !=
               0) {
      throw std::runtime_error("Failed to configure physics failure stage");
    }
#endif
  }
};

PhysicsCharacterState simulate(PhysicsWorld& world, PhysicsVector horizontal,
                               int steps, bool crouch_requested = false) {
  constexpr float delta = 1.0F / 60.0F;
  PhysicsCharacterState state = world.characterState();
  for (int step = 0; step < steps; ++step) {
    float vertical_velocity = state.linear_velocity.y;
    if (state.supported() && vertical_velocity <= 0.1F) {
      vertical_velocity = 0.0F;
    }
    vertical_velocity -= 18.0F * delta;
    state =
        world.stepCharacter({{horizontal.x, vertical_velocity, horizontal.z},
                             {0.0F, -18.0F, 0.0F},
                             crouch_requested},
                            delta);
  }
  return state;
}

}  // namespace

TEST(PhysicsLifetime, RepeatedCreateDestroyLeavesNoGlobalOwner) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  for (int cycle = 0; cycle < 4; ++cycle) {
    const PhysicsWorld physics(level);
    EXPECT_EQ(physics.staticBodyCount(), level.solids().size() + 1U);
    EXPECT_TRUE(physics.hasTerrainCollision());
  }
}

TEST(PhysicsLifetime, InteriorPartialConstructionAndWallsRetainOwnership) {
  const auto level =
      loadPrototypeLevel("resources/levels/apartment-stairs.level.json");
  for (const char* stage : {"runtime-factory", "world", "static-bodies",
                            "model-proxy", "character"}) {
    {
      ScopedPhysicsFailure failure(stage);
      EXPECT_THROW(static_cast<void>(PhysicsWorld{level}), std::runtime_error);
    }
    PhysicsWorld recovered(level, *level.entry("lower-landing"));
    EXPECT_FALSE(recovered.hasTerrainCollision());
    const auto state = simulate(recovered, {4, 0, 0}, 120);
    EXPECT_TRUE(state.supported());
    EXPECT_LT(state.foot_position.x, 1.7F);
    EXPECT_TRUE(recovered.staticSegmentBlocked({0, 1, -16.2F}, {3, 1, -16.2F}));
  }
}

TEST(PhysicsLifetime, FailureAfterSeveralDoorBodiesReleasesAllOwners) {
  auto doc = prototypeLevelDocument();
  doc.terrain.reset();
  doc.props.clear();
  doc.light_switch.reset();
  doc.solids = {{{0, -.25F, 0},
                 {10, .25F, 10},
                 {160, 160, 160, 255},
                 PrototypeSolidKind::Floor,
                 "prototype-floor"}};
  doc.entries = {{"start", {{0, 0, 3}, 0}}};
  doc.default_entry = "start";
  DoorDefinition first;
  first.id = "first";
  first.hinge_position = {-4, .02F, 0};
  DoorDefinition second = first;
  second.id = "second";
  second.hinge_position.x = 4;
  doc.doors = {first, second};
  const auto level = makePrototypeLevel(doc);
  for (int cycle = 0; cycle < 3; ++cycle) {
    {
      ScopedPhysicsFailure failure("door-bodies");
      EXPECT_THROW(static_cast<void>(PhysicsWorld{level}), std::runtime_error);
    }
    PhysicsWorld recovered(level);
    EXPECT_EQ(recovered.doorCount(), 2U);
    EXPECT_TRUE(recovered.worldSegmentBlocked({-3.5F, 1, 1}, {-3.5F, 1, -1}));
    EXPECT_TRUE(recovered.worldSegmentBlocked({4.5F, 1, 1}, {4.5F, 1, -1}));
  }
}

TEST(PhysicsLifetime, RejectsDuplicateActiveRuntimeWithoutDamagingOwner) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld owner(level);
  EXPECT_THROW(static_cast<void>(PhysicsWorld{level}), std::runtime_error);
  EXPECT_EQ(owner.staticBodyCount(), level.solids().size() + 1U);
  EXPECT_TRUE(owner.hasTerrainCollision());
}

TEST(PhysicsLifetime, PartialInitializationFailuresReleaseEveryStage) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  for (const char* stage : {"runtime-factory", "world", "static-bodies",
                            "model-proxy", "character"}) {
    {
      const ScopedPhysicsFailure failure(stage);
      EXPECT_THROW(static_cast<void>(PhysicsWorld{level}), std::runtime_error)
          << stage;
    }
    EXPECT_NO_THROW(static_cast<void>(PhysicsWorld{level})) << stage;
  }
}

TEST(PhysicsWorld, AdvancesOnTheCallingThreadWithSingleThreadedJobs) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  const std::thread::id caller = std::this_thread::get_id();
  EXPECT_TRUE(physics.usesSingleThreadedJobs());
  const PhysicsCharacterState state = physics.stepCharacter(
      {{0.0F, -0.3F, 0.0F}, {0.0F, -18.0F, 0.0F}, false}, 1.0F / 60.0F);
  EXPECT_EQ(std::this_thread::get_id(), caller);
  EXPECT_LT(state.foot_position.y, level.playerSpawn().foot_position.y);
}

TEST(PhysicsWorld, StaticTerrainSupportsTheFallingCharacter) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  const PhysicsCharacterState state = simulate(physics, {}, 180);
  EXPECT_TRUE(state.supported());
  EXPECT_NEAR(state.foot_position.y,
              prototypeTerrainHeightAt(*level.terrain(), state.foot_position.x,
                                       state.foot_position.z),
              0.03F);
}

TEST(PhysicsWorld, TraversesTerrainFeaturesAndCannotEscapeAtADepression) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();

  {
    PhysicsWorld hill_world(level);
    static_cast<void>(simulate(hill_world, {}, 120));
    static_cast<void>(simulate(hill_world, {-5.0F, 0.0F, 0.0F}, 180));
    const PhysicsCharacterState hill =
        simulate(hill_world, {0.0F, 0.0F, -5.0F}, 180);
    ASSERT_TRUE(hill.supported());
    EXPECT_GT(hill.foot_position.y, 0.3F);
    EXPECT_NEAR(hill.foot_position.y,
                prototypeTerrainHeightAt(*level.terrain(), hill.foot_position.x,
                                         hill.foot_position.z),
                0.04F);
  }

  PhysicsWorld depression_world(level);
  static_cast<void>(simulate(depression_world, {}, 120));
  static_cast<void>(simulate(depression_world, {5.0F, 0.0F, 0.0F}, 170));
  const PhysicsCharacterState depression =
      simulate(depression_world, {0.0F, 0.0F, -5.0F}, 216);
  ASSERT_TRUE(depression.supported());
  EXPECT_LT(depression.foot_position.y, -0.3F);
  EXPECT_NEAR(
      depression.foot_position.y,
      prototypeTerrainHeightAt(*level.terrain(), depression.foot_position.x,
                               depression.foot_position.z),
      0.04F);

  const PhysicsCharacterState blocked =
      simulate(depression_world, {8.0F, 0.0F, 0.0F}, 120);
  EXPECT_LT(blocked.foot_position.x, 23.7F);
  EXPECT_GT(blocked.foot_position.x, 23.0F);
  EXPECT_TRUE(blocked.supported());
}

TEST(PhysicsWorld, StaticBoundaryBlocksCharacter) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  const PhysicsCharacterState state =
      simulate(physics, {20.0F, 0.0F, 0.0F}, 90);
  EXPECT_LT(state.foot_position.x, 23.7F);
  EXPECT_GT(state.foot_position.x, 23.0F);
}

TEST(PhysicsWorld, StaticObstacleRejectsForwardMovement) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  const PhysicsCharacterState state =
      simulate(physics, {0.0F, 0.0F, -8.0F}, 120);
  EXPECT_GT(state.foot_position.z, 1.3F);
  EXPECT_LT(state.foot_position.z, 2.0F);
}

TEST(PhysicsWorld, StaticChairProxyMatchesPlacementAndBlocksThePlayer) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  ASSERT_EQ(physics.staticBodyCount(), level.solids().size() + 1U);
  const PhysicsStaticSolid chair = physics.staticBody(level.solids().size());
  const WorldPosition expected_center = propBoxWorldCenter(
      level.props().front(), level.props().front().collision_boxes.front());
  const WorldExtent expected_half_extent = propBoxWorldHalfExtent(
      level.props().front(), level.props().front().collision_boxes.front());
  EXPECT_FLOAT_EQ(chair.center.x, expected_center.x);
  EXPECT_FLOAT_EQ(chair.center.y, expected_center.y);
  EXPECT_FLOAT_EQ(chair.center.z, expected_center.z);
  EXPECT_FLOAT_EQ(chair.half_extent.x, expected_half_extent.x);
  EXPECT_FLOAT_EQ(chair.half_extent.y, expected_half_extent.y);
  EXPECT_FLOAT_EQ(chair.half_extent.z, expected_half_extent.z);
  EXPECT_FLOAT_EQ(chair.yaw_degrees, level.props().front().yaw_degrees);
  EXPECT_EQ(chair.kind, PrototypeSolidKind::Obstacle);

  static_cast<void>(simulate(physics, {}, 120));
  static_cast<void>(simulate(physics, {4.0F, 0.0F, 0.0F}, 27));
  static_cast<void>(simulate(physics, {0.0F, 0.0F, -5.0F}, 72));
  const float yaw =
      level.props().front().yaw_degrees * std::numbers::pi_v<float> / 180.0F;
  const float local_z_x = std::sin(yaw);
  const float local_z_z = std::cos(yaw);
  const PhysicsCharacterState blocked =
      simulate(physics, {-local_z_x * 5.0F, 0.0F, -local_z_z * 5.0F}, 120);
  const float offset_x = blocked.foot_position.x - chair.center.x;
  const float offset_z = blocked.foot_position.z - chair.center.z;
  const float front_distance = offset_x * local_z_x + offset_z * local_z_z;
  const float lateral_distance =
      offset_x * std::cos(yaw) - offset_z * std::sin(yaw);
  EXPECT_GT(front_distance, chair.half_extent.z + player_capsule_radius - 0.1F);
  EXPECT_LT(front_distance,
            chair.half_extent.z + player_capsule_radius + 0.15F);
  EXPECT_NEAR(lateral_distance, 0.0F, 0.25F);
}

TEST(PhysicsCharacter, SpawnsUnsupportedThenSettlesStably) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  const PhysicsCharacterState initial = physics.characterState();
  EXPECT_EQ(initial.ground_state, PhysicsGroundState::InAir);
  EXPECT_FLOAT_EQ(initial.foot_position.x, level.playerSpawn().foot_position.x);
  EXPECT_FLOAT_EQ(initial.foot_position.z, level.playerSpawn().foot_position.z);

  const PhysicsCharacterState settled = simulate(physics, {}, 120);
  ASSERT_TRUE(settled.supported());
  const PhysicsCharacterState stable = simulate(physics, {}, 120);
  EXPECT_TRUE(stable.supported());
  EXPECT_NEAR(stable.foot_position.y, settled.foot_position.y, 0.002F);
}

TEST(PhysicsCharacter, SlidesTangentiallyAlongBoundary) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  const float initial_z = physics.characterState().foot_position.z;
  const PhysicsCharacterState state =
      simulate(physics, {8.0F, 0.0F, -2.0F}, 180);
  EXPECT_LT(state.foot_position.x, 23.7F);
  EXPECT_GT(state.foot_position.x, 23.0F);
  EXPECT_LT(state.foot_position.z, initial_z - 4.0F);
}

TEST(PhysicsCharacter, TraversesAuthoredLowStepWithoutEmbedding) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  static_cast<void>(simulate(physics, {-6.5F, 0.0F, 0.0F}, 60));
  const PhysicsCharacterState on_step =
      simulate(physics, {0.0F, 0.0F, -4.0F}, 45);
  EXPECT_NEAR(on_step.foot_position.y, 0.3F, 0.06F);
  EXPECT_GT(on_step.foot_position.z, 2.5F);
  EXPECT_LT(on_step.foot_position.z, 5.5F);

  const PhysicsCharacterState beyond_step =
      simulate(physics, {0.0F, 0.0F, -4.0F}, 45);
  EXPECT_NEAR(beyond_step.foot_position.y, 0.0F, 0.04F);
  EXPECT_LT(beyond_step.foot_position.z, 2.5F);
}

TEST(PhysicsCharacter, CrouchPreservesFootPosition) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  const PhysicsCharacterState standing = simulate(physics, {}, 120);
  const PhysicsCharacterState crouched = simulate(physics, {}, 1, true);
  EXPECT_EQ(crouched.stance, PhysicsPlayerStance::Crouched);
  EXPECT_NEAR(crouched.foot_position.x, standing.foot_position.x, 0.001F);
  EXPECT_NEAR(crouched.foot_position.y, standing.foot_position.y, 0.002F);
  EXPECT_NEAR(crouched.foot_position.z, standing.foot_position.z, 0.001F);
}

TEST(PhysicsCharacter, LowClearanceRouteRejectsStandingPlayer) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  static_cast<void>(simulate(physics, {6.0F, 0.0F, 0.0F}, 60));
  const PhysicsCharacterState blocked =
      simulate(physics, {0.0F, 0.0F, -4.0F}, 90);
  EXPECT_EQ(blocked.stance, PhysicsPlayerStance::Standing);
  EXPECT_GT(blocked.foot_position.z, 4.7F);
}

TEST(PhysicsCharacter, CrouchesThroughRouteAndStandsAfterClearance) {
  const PrototypeLevel level = loadPackagedPrototypeLevel();
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  static_cast<void>(simulate(physics, {6.0F, 0.0F, 0.0F}, 60));
  static_cast<void>(simulate(physics, {}, 1, true));
  const PhysicsCharacterState beneath_roof =
      simulate(physics, {0.0F, 0.0F, -4.0F}, 75, true);
  ASSERT_EQ(beneath_roof.stance, PhysicsPlayerStance::Crouched);
  ASSERT_LT(beneath_roof.foot_position.z, 4.0F);
  ASSERT_GT(beneath_roof.foot_position.z, -1.0F);

  const PhysicsCharacterState blocked_stand = simulate(physics, {}, 1, false);
  EXPECT_EQ(blocked_stand.stance, PhysicsPlayerStance::Crouched);

  const PhysicsCharacterState after_clearance =
      simulate(physics, {0.0F, 0.0F, -4.0F}, 75, false);
  EXPECT_LT(after_clearance.foot_position.z, -1.8F);
  EXPECT_EQ(after_clearance.stance, PhysicsPlayerStance::Standing);
}
