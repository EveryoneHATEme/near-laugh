#include <gtest/gtest.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>

#include "core/physics/physics_world.hpp"
#include "core/world/prototype_level.hpp"

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
    state = world.stepCharacter(
        {{horizontal.x, vertical_velocity, horizontal.z},
         {0.0F, -18.0F, 0.0F}, crouch_requested},
        delta);
  }
  return state;
}
}  // namespace

TEST(PhysicsLifetime, RepeatedCreateDestroyLeavesNoGlobalOwner) {
  const PrototypeLevel level;
  for (int cycle = 0; cycle < 4; ++cycle) {
    const PhysicsWorld physics(level);
    EXPECT_EQ(physics.staticBodyCount(), level.solids().size());
  }
}

TEST(PhysicsLifetime, RejectsDuplicateActiveRuntimeWithoutDamagingOwner) {
  const PrototypeLevel level;
  PhysicsWorld owner(level);
  EXPECT_THROW(static_cast<void>(PhysicsWorld{level}), std::runtime_error);
  EXPECT_EQ(owner.staticBodyCount(), level.solids().size());
}

TEST(PhysicsLifetime, PartialInitializationFailuresReleaseEveryStage) {
  const PrototypeLevel level;
  for (const char* stage :
       {"runtime-factory", "world", "static-bodies", "character"}) {
    {
      const ScopedPhysicsFailure failure(stage);
      EXPECT_THROW(static_cast<void>(PhysicsWorld{level}), std::runtime_error)
          << stage;
    }
    EXPECT_NO_THROW(static_cast<void>(PhysicsWorld{level})) << stage;
  }
}

TEST(PhysicsWorld, AdvancesOnTheCallingThreadWithSingleThreadedJobs) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  const std::thread::id caller = std::this_thread::get_id();
  EXPECT_TRUE(physics.usesSingleThreadedJobs());
  const PhysicsCharacterState state =
      physics.stepCharacter({{0.0F, -0.3F, 0.0F},
                            {0.0F, -18.0F, 0.0F}, false},
                            1.0F / 60.0F);
  EXPECT_EQ(std::this_thread::get_id(), caller);
  EXPECT_LT(state.foot_position.y, level.playerSpawn().foot_position.y);
}

TEST(PhysicsWorld, StaticFloorSupportsTheFallingCharacter) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  const PhysicsCharacterState state = simulate(physics, {}, 180);
  EXPECT_TRUE(state.supported());
  EXPECT_NEAR(state.foot_position.y, 0.0F, 0.03F);
  EXPECT_GE(state.foot_position.y, -0.001F);
}

TEST(PhysicsWorld, StaticBoundaryBlocksCharacter) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  const PhysicsCharacterState state =
      simulate(physics, {20.0F, 0.0F, 0.0F}, 90);
  EXPECT_LT(state.foot_position.x, 9.7F);
  EXPECT_GT(state.foot_position.x, 9.0F);
}

TEST(PhysicsWorld, StaticObstacleRejectsForwardMovement) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  const PhysicsCharacterState state =
      simulate(physics, {0.0F, 0.0F, -8.0F}, 120);
  EXPECT_GT(state.foot_position.z, 1.3F);
  EXPECT_LT(state.foot_position.z, 2.0F);
}

TEST(PhysicsCharacter, SpawnsUnsupportedThenSettlesStably) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  const PhysicsCharacterState initial = physics.characterState();
  EXPECT_EQ(initial.ground_state, PhysicsGroundState::InAir);
  EXPECT_FLOAT_EQ(initial.foot_position.x,
                  level.playerSpawn().foot_position.x);
  EXPECT_FLOAT_EQ(initial.foot_position.z,
                  level.playerSpawn().foot_position.z);

  const PhysicsCharacterState settled = simulate(physics, {}, 120);
  ASSERT_TRUE(settled.supported());
  const PhysicsCharacterState stable = simulate(physics, {}, 120);
  EXPECT_TRUE(stable.supported());
  EXPECT_NEAR(stable.foot_position.y, settled.foot_position.y, 0.002F);
}

TEST(PhysicsCharacter, SlidesTangentiallyAlongBoundary) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  const float initial_z = physics.characterState().foot_position.z;
  const PhysicsCharacterState state =
      simulate(physics, {8.0F, 0.0F, -2.0F}, 180);
  EXPECT_LT(state.foot_position.x, 9.7F);
  EXPECT_GT(state.foot_position.x, 9.0F);
  EXPECT_LT(state.foot_position.z, initial_z - 4.0F);
}

TEST(PhysicsCharacter, TraversesAuthoredLowStepWithoutEmbedding) {
  const PrototypeLevel level;
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
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  const PhysicsCharacterState standing = simulate(physics, {}, 120);
  const PhysicsCharacterState crouched = simulate(physics, {}, 1, true);
  EXPECT_EQ(crouched.stance, PhysicsPlayerStance::Crouched);
  EXPECT_NEAR(crouched.foot_position.x, standing.foot_position.x, 0.001F);
  EXPECT_NEAR(crouched.foot_position.y, standing.foot_position.y, 0.002F);
  EXPECT_NEAR(crouched.foot_position.z, standing.foot_position.z, 0.001F);
}

TEST(PhysicsCharacter, LowClearanceRouteRejectsStandingPlayer) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  static_cast<void>(simulate(physics, {}, 120));
  static_cast<void>(simulate(physics, {6.0F, 0.0F, 0.0F}, 60));
  const PhysicsCharacterState blocked =
      simulate(physics, {0.0F, 0.0F, -4.0F}, 90);
  EXPECT_EQ(blocked.stance, PhysicsPlayerStance::Standing);
  EXPECT_GT(blocked.foot_position.z, 4.7F);
}

TEST(PhysicsCharacter, CrouchesThroughRouteAndStandsAfterClearance) {
  const PrototypeLevel level;
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
