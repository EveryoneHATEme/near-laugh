#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

#include "core/player/player_controller.hpp"
#include "core/world/prototype_level.hpp"

namespace {
constexpr float fixed_delta = 1.0F / 60.0F;

void settle(PlayerController& player) {
  player.sampleInput({}, true);
  for (int step = 0; step < 120; ++step) {
    player.fixedStep(fixed_delta);
  }
  ASSERT_TRUE(player.state().supported());
}

float horizontalSpeed(PhysicsVector velocity) {
  return std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
}

PhysicsVector groundedRequest(FpsActionSnapshot actions) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  settle(player);
  player.sampleInput(actions, true);
  player.fixedStep(fixed_delta);
  return player.requestedHorizontalVelocity();
}

struct HomogeneousPoint {
  float x;
  float y;
  float z;
  float w;
};

HomogeneousPoint transform(const CameraFrame& frame,
                           std::array<float, 3> point) {
  const auto& matrix = frame.view_projection;
  return {
      matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] +
          matrix[12],
      matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] +
          matrix[13],
      matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] +
          matrix[14],
      matrix[3] * point[0] + matrix[7] * point[1] + matrix[11] * point[2] +
          matrix[15],
  };
}
}  // namespace

TEST(PlayerMovement, EveryGroundAxisUsesCurrentYaw) {
  FpsActionSnapshot actions;
  actions.move_forward = true;
  PhysicsVector velocity = groundedRequest(actions);
  EXPECT_NEAR(velocity.x, 0.0F, 0.0001F);
  EXPECT_NEAR(velocity.z, -player_walk_speed, 0.0001F);

  actions = {};
  actions.move_backward = true;
  velocity = groundedRequest(actions);
  EXPECT_NEAR(velocity.z, player_walk_speed, 0.0001F);

  actions = {};
  actions.move_left = true;
  velocity = groundedRequest(actions);
  EXPECT_NEAR(velocity.x, -player_walk_speed, 0.0001F);

  actions = {};
  actions.move_right = true;
  velocity = groundedRequest(actions);
  EXPECT_NEAR(velocity.x, player_walk_speed, 0.0001F);
}

TEST(PlayerMovement, DiagonalIsNormalizedAndSprintUsesSevenMetersPerSecond) {
  FpsActionSnapshot actions;
  actions.move_forward = true;
  actions.move_right = true;
  EXPECT_NEAR(horizontalSpeed(groundedRequest(actions)), player_walk_speed,
              0.0001F);
  actions.sprint = true;
  EXPECT_NEAR(horizontalSpeed(groundedRequest(actions)), player_sprint_speed,
              0.0001F);
}

TEST(PlayerMovement, GravityAndGroundedMotionUseDifferentPolicies) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  player.sampleInput({}, true);
  player.fixedStep(fixed_delta);
  EXPECT_LT(player.state().linear_velocity.y, 0.0F);

  settle(player);
  FpsActionSnapshot forward;
  forward.move_forward = true;
  player.sampleInput(forward, true);
  player.fixedStep(fixed_delta);
  EXPECT_NEAR(horizontalSpeed(player.requestedHorizontalVelocity()),
              player_walk_speed, 0.0001F);
}

TEST(PlayerMovement, AirControlApproachesRequestAtBoundedAcceleration) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  settle(player);
  FpsActionSnapshot jump;
  jump.jump = true;
  player.sampleInput(jump, true);
  player.fixedStep(fixed_delta);
  player.fixedStep(fixed_delta);
  ASSERT_FALSE(player.state().supported());

  FpsActionSnapshot forward;
  forward.move_forward = true;
  player.sampleInput(forward, true);
  player.fixedStep(fixed_delta);
  const float controlled_speed =
      horizontalSpeed(player.requestedHorizontalVelocity());
  EXPECT_GT(controlled_speed, 0.0F);
  EXPECT_LE(controlled_speed, player_air_control * fixed_delta + 0.0001F);
  EXPECT_LT(controlled_speed, player_walk_speed);
}

TEST(PlayerJump, LatchSurvivesAZeroStepRenderIteration) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  settle(player);
  FpsActionSnapshot jump;
  jump.jump = true;
  player.sampleInput(jump, true);
  EXPECT_TRUE(player.jumpPending());
  player.sampleInput({}, true);
  EXPECT_TRUE(player.jumpPending());
  player.fixedStep(fixed_delta);
  EXPECT_FALSE(player.jumpPending());
  EXPECT_GT(player.state().linear_velocity.y, 0.0F);
}

TEST(PlayerJump, HeldJumpIsConsumedExactlyOnceAcrossCatchUpSteps) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  settle(player);
  FpsActionSnapshot jump;
  jump.jump = true;
  player.sampleInput(jump, true);
  player.fixedStep(fixed_delta);
  const float first_vertical = player.state().linear_velocity.y;
  player.fixedStep(fixed_delta);
  player.fixedStep(fixed_delta);
  EXPECT_FALSE(player.jumpPending());
  EXPECT_LT(player.state().linear_velocity.y, first_vertical);

  for (int step = 0; step < 180; ++step) {
    player.sampleInput(jump, true);
    player.fixedStep(fixed_delta);
  }
  EXPECT_TRUE(player.state().supported());
  EXPECT_LE(player.state().linear_velocity.y, 0.1F);
}

TEST(PlayerJump, AirbornePressWaitsForFirstEligibleGroundedStep) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  settle(player);
  FpsActionSnapshot jump;
  jump.jump = true;
  player.sampleInput(jump, true);
  player.fixedStep(fixed_delta);
  player.sampleInput({}, true);
  player.fixedStep(fixed_delta);
  ASSERT_FALSE(player.state().supported());

  player.sampleInput(jump, true);
  ASSERT_TRUE(player.jumpPending());
  for (int step = 0; step < 180 && !player.state().supported(); ++step) {
    player.fixedStep(fixed_delta);
  }
  ASSERT_TRUE(player.state().supported());
  ASSERT_TRUE(player.jumpPending());
  player.fixedStep(fixed_delta);
  EXPECT_FALSE(player.jumpPending());
  EXPECT_GT(player.state().linear_velocity.y, 0.0F);
}

TEST(PlayerCamera, RetainsVulkanProjectionStorageAndAspectBehavior) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  const float camera_y = level.playerSpawn().foot_position.y +
                         player_standing_eye_height;
  const CameraFrame square = player.cameraFrame(1.0F, 1.0F);
  const HomogeneousPoint near_point =
      transform(square, {0.0F, camera_y, 6.9F});
  const HomogeneousPoint far_point =
      transform(square, {0.0F, camera_y, -93.0F});
  EXPECT_NEAR(near_point.z / near_point.w, 0.0F, 0.0001F);
  EXPECT_NEAR(far_point.z / far_point.w, 1.0F, 0.0001F);

  const HomogeneousPoint square_offset =
      transform(square, {1.0F, camera_y, 0.0F});
  const HomogeneousPoint wide_offset =
      transform(player.cameraFrame(2.0F, 1.0F), {1.0F, camera_y, 0.0F});
  EXPECT_NEAR(wide_offset.x / wide_offset.w,
              (square_offset.x / square_offset.w) * 0.5F, 0.0001F);
  EXPECT_THROW(static_cast<void>(player.cameraFrame(0.0F, 1.0F)),
               std::invalid_argument);
}

TEST(PlayerCamera, AppliesLookOncePerSampleAndClampsPitch) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  FpsActionSnapshot actions;
  actions.look_delta_x = 100.0;
  actions.look_delta_y = -1000.0;
  player.sampleInput(actions, true);
  EXPECT_FLOAT_EQ(player.yawDegrees(), -80.0F);
  EXPECT_FLOAT_EQ(player.pitchDegrees(), 89.0F);
  player.fixedStep(fixed_delta);
  player.fixedStep(fixed_delta);
  EXPECT_FLOAT_EQ(player.yawDegrees(), -80.0F);

  actions.look_delta_x = 0.0;
  actions.look_delta_y = 2000.0;
  player.sampleInput(actions, true);
  EXPECT_FLOAT_EQ(player.pitchDegrees(), -89.0F);
}

TEST(PlayerCamera, UsesActualStanceEyeHeight) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  settle(player);
  const PlayerCameraPosition standing = player.interpolatedCameraPosition(1.0F);
  EXPECT_NEAR(standing.y, player.state().foot_position.y +
                              player_standing_eye_height,
              0.0001F);

  FpsActionSnapshot crouch;
  crouch.crouch = true;
  player.sampleInput(crouch, true);
  player.fixedStep(fixed_delta);
  const PlayerCameraPosition crouched = player.interpolatedCameraPosition(1.0F);
  EXPECT_NEAR(crouched.y, player.state().foot_position.y +
                              player_crouched_eye_height,
              0.0001F);
}

TEST(PlayerAim, UsesCurrentStandingAndCrouchedSimulatedEye) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  settle(player);
  PlayerAim aim = player.currentAim();
  EXPECT_NEAR(aim.eye_position.y,
              player.state().foot_position.y + player_standing_eye_height,
              0.0001F);

  FpsActionSnapshot crouch;
  crouch.crouch = true;
  player.sampleInput(crouch, true);
  player.fixedStep(fixed_delta);
  aim = player.currentAim();
  EXPECT_NEAR(aim.eye_position.y,
              player.state().foot_position.y + player_crouched_eye_height,
              0.0001F);
}

TEST(PlayerAim, AppliesPartialAndClampedMaximumRecoilToAimAndCamera) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);

  const PlayerAim zero = player.currentAim();
  EXPECT_NEAR(zero.direction.x, 0.0F, 0.0001F);
  EXPECT_NEAR(zero.direction.y, 0.0F, 0.0001F);
  EXPECT_NEAR(zero.direction.z, -1.0F, 0.0001F);

  const PlayerAim partial = player.currentAim(4.0F);
  EXPECT_GT(partial.direction.y, 0.0F);
  EXPECT_GT(partial.direction.z, zero.direction.z + 0.001F);
  const CameraFrame partial_camera = player.cameraFrame(1.0F, 1.0F, 4.0F);
  const HomogeneousPoint centered = transform(
      partial_camera,
      {partial.eye_position.x + partial.direction.x,
       partial.eye_position.y + partial.direction.y,
       partial.eye_position.z + partial.direction.z});
  EXPECT_NEAR(centered.x / centered.w, 0.0F, 0.0001F);
  EXPECT_NEAR(centered.y / centered.w, 0.0F, 0.0001F);

  FpsActionSnapshot almost_up;
  almost_up.look_delta_y = -880.0;
  player.sampleInput(almost_up, true);
  const PlayerAim maximum = player.currentAim(8.0F);
  EXPECT_NEAR(maximum.direction.y,
              std::sin(player_pitch_limit_degrees *
                       3.14159265358979323846F / 180.0F),
              0.0001F);
  EXPECT_THROW(static_cast<void>(player.currentAim(
                   std::numeric_limits<float>::quiet_NaN())),
               std::invalid_argument);
}

TEST(PlayerCursor, ReleasePrecedesRecaptureAndTransitionsNeutralizeControls) {
  FpsActionSnapshot actions;
  actions.menu = true;
  actions.primary_action = true;
  EXPECT_EQ(playerCursorTransition(true, actions),
            PlayerCursorCaptureTransition::Release);
  EXPECT_FALSE(playerControlsActive(
      true, playerCursorTransition(true, actions)));
  EXPECT_EQ(playerCursorTransition(false, actions),
            PlayerCursorCaptureTransition::None);

  actions.menu = false;
  EXPECT_EQ(playerCursorTransition(false, actions),
            PlayerCursorCaptureTransition::Capture);
  EXPECT_FALSE(playerControlsActive(
      false, playerCursorTransition(false, actions)));
  EXPECT_TRUE(playerControlsActive(
      true, playerCursorTransition(true, actions)));
}

TEST(PlayerCursor, ReleasedControlsAreNeutralWhileAirbornePhysicsContinues) {
  const PrototypeLevel level;
  PhysicsWorld physics(level);
  PlayerController player(physics, level.playerSpawn().yaw_degrees);
  FpsActionSnapshot actions;
  actions.move_forward = true;
  actions.jump = true;
  actions.crouch = true;
  actions.sprint = true;
  actions.look_delta_x = 500.0;
  const float initial_y = player.state().foot_position.y;
  player.sampleInput(actions, false);
  player.fixedStep(fixed_delta);
  EXPECT_FLOAT_EQ(player.yawDegrees(), level.playerSpawn().yaw_degrees);
  EXPECT_FALSE(player.jumpPending());
  EXPECT_EQ(player.state().stance, PhysicsPlayerStance::Standing);
  EXPECT_FLOAT_EQ(horizontalSpeed(player.requestedHorizontalVelocity()), 0.0F);
  EXPECT_LT(player.state().foot_position.y, initial_y);
}
