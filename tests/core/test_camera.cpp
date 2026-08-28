#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>

#include "core/camera/free_fly_camera.hpp"

namespace {
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

float distance(CameraPosition first, CameraPosition second) {
  const float x = first.x - second.x;
  const float y = first.y - second.y;
  const float z = first.z - second.z;
  return std::sqrt(x * x + y * y + z * z);
}
}  // namespace

TEST(FreeFlyCamera, InitialPoseFacesThePrototypeScene) {
  const FreeFlyCamera camera;
  const HomogeneousPoint center =
      transform(camera.frame(16.0F / 9.0F), {0.0F, 2.0F, 0.0F});
  EXPECT_NEAR(center.x / center.w, 0.0F, 0.0001F);
  EXPECT_NEAR(center.y / center.w, 0.0F, 0.0001F);
  EXPECT_GT(center.z / center.w, 0.0F);
  EXPECT_LT(center.z / center.w, 1.0F);
}

TEST(FreeFlyCamera, UsesVulkanZeroToOneDepthAndColumnMajorStorage) {
  const FreeFlyCamera camera;
  const CameraFrame frame = camera.frame(1.0F);
  const HomogeneousPoint near_point = transform(frame, {0.0F, 2.0F, 7.9F});
  const HomogeneousPoint far_point = transform(frame, {0.0F, 2.0F, -92.0F});
  EXPECT_NEAR(near_point.z / near_point.w, 0.0F, 0.0001F);
  EXPECT_NEAR(far_point.z / far_point.w, 1.0F, 0.0001F);
}

TEST(FreeFlyCamera, FramebufferAspectChangesHorizontalProjection) {
  const FreeFlyCamera camera;
  const HomogeneousPoint square =
      transform(camera.frame(1.0F), {1.0F, 2.0F, 0.0F});
  const HomogeneousPoint wide =
      transform(camera.frame(2.0F), {1.0F, 2.0F, 0.0F});
  EXPECT_NEAR(wide.x / wide.w, (square.x / square.w) * 0.5F, 0.0001F);
  EXPECT_THROW(static_cast<void>(camera.frame(0.0F)), std::invalid_argument);
}

TEST(FreeFlyCamera, MouseLookAndPitchLimitAreDeterministic) {
  FreeFlyCamera camera;
  FpsActionSnapshot actions;
  actions.look_delta_x = 100.0;
  actions.look_delta_y = -1000.0;
  camera.update(actions, 0.0);
  EXPECT_FLOAT_EQ(camera.yawDegrees(), -80.0F);
  EXPECT_FLOAT_EQ(camera.pitchDegrees(), 89.0F);

  actions.look_delta_y = 2000.0;
  camera.update(actions, 0.0);
  EXPECT_FLOAT_EQ(camera.pitchDegrees(), -89.0F);
}

TEST(FreeFlyCamera, MovementUsesHorizontalOrientationAndVerticalActions) {
  FreeFlyCamera camera;
  FpsActionSnapshot turn;
  turn.look_delta_x = 900.0;
  camera.update(turn, 0.0);

  const CameraPosition before = camera.position();
  FpsActionSnapshot move;
  move.move_forward = true;
  move.jump = true;
  camera.update(move, 0.1);
  const CameraPosition after = camera.position();
  EXPECT_GT(after.x, before.x);
  EXPECT_GT(after.y, before.y);
  EXPECT_NEAR(after.z, before.z, 0.0001F);
  EXPECT_NEAR(distance(before, after), 0.4F, 0.0001F);
}

TEST(FreeFlyCamera, CombinedAxesDoNotExceedNormalOrSprintSpeed) {
  FpsActionSnapshot move;
  move.move_forward = true;
  move.move_right = true;
  move.jump = true;

  FreeFlyCamera normal;
  const CameraPosition normal_before = normal.position();
  normal.update(move, 0.1);
  EXPECT_NEAR(distance(normal_before, normal.position()), 0.4F, 0.0001F);

  FreeFlyCamera sprint;
  const CameraPosition sprint_before = sprint.position();
  move.sprint = true;
  sprint.update(move, 0.1);
  EXPECT_NEAR(distance(sprint_before, sprint.position()), 1.2F, 0.0001F);
}

TEST(FreeFlyCamera, EveryTranslationActionMovesOnItsExpectedAxis) {
  const CameraPosition initial = FreeFlyCamera{}.position();

  const auto moved = [](FpsActionSnapshot actions) {
    FreeFlyCamera camera;
    camera.update(actions, 0.1);
    return camera.position();
  };

  FpsActionSnapshot actions;
  actions.move_forward = true;
  EXPECT_LT(moved(actions).z, initial.z);
  actions = {};
  actions.move_backward = true;
  EXPECT_GT(moved(actions).z, initial.z);
  actions = {};
  actions.move_left = true;
  EXPECT_LT(moved(actions).x, initial.x);
  actions = {};
  actions.move_right = true;
  EXPECT_GT(moved(actions).x, initial.x);
  actions = {};
  actions.jump = true;
  EXPECT_GT(moved(actions).y, initial.y);
  actions = {};
  actions.crouch = true;
  EXPECT_LT(moved(actions).y, initial.y);
}

TEST(FrameTiming, CapsOrdinaryDeltasAndResetsAfterBlockedWait) {
  using namespace std::chrono_literals;
  FrameClock clock;
  const FrameClock::Clock::time_point start{};
  EXPECT_DOUBLE_EQ(clock.sample(start), 0.0);
  EXPECT_NEAR(clock.sample(start + 16ms), 0.016, 0.000001);
  EXPECT_DOUBLE_EQ(clock.sample(start + 1s), 0.1);
  clock.reset();
  EXPECT_DOUBLE_EQ(clock.sample(start + 10s), 0.0);
}

TEST(CursorCapture, ReleasePrecedesRecaptureAndNavigationState) {
  FpsActionSnapshot actions;
  actions.menu = true;
  actions.primary_action = true;
  EXPECT_EQ(cursorCaptureTransition(true, actions),
            CursorCaptureTransition::Release);
  EXPECT_FALSE(
      freeFlyNavigationActive(true, cursorCaptureTransition(true, actions)));
  EXPECT_EQ(cursorCaptureTransition(false, actions),
            CursorCaptureTransition::None);
  EXPECT_FALSE(
      freeFlyNavigationActive(false, cursorCaptureTransition(false, actions)));

  actions.menu = false;
  EXPECT_EQ(cursorCaptureTransition(false, actions),
            CursorCaptureTransition::Capture);
  EXPECT_FALSE(
      freeFlyNavigationActive(false, cursorCaptureTransition(false, actions)));
  EXPECT_EQ(cursorCaptureTransition(true, actions),
            CursorCaptureTransition::None);
  EXPECT_TRUE(
      freeFlyNavigationActive(true, cursorCaptureTransition(true, actions)));
}
