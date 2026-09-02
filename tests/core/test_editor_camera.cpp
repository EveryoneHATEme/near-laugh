#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>

#include "editor/editor_camera.hpp"

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
  return {matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] +
              matrix[12],
          matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] +
              matrix[13],
          matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] +
              matrix[14],
          matrix[3] * point[0] + matrix[7] * point[1] + matrix[11] * point[2] +
              matrix[15]};
}

float distance(EditorCameraPosition first, EditorCameraPosition second) {
  const float x = first.x - second.x;
  const float y = first.y - second.y;
  const float z = first.z - second.z;
  return std::sqrt(x * x + y * y + z * z);
}
}  // namespace

TEST(EditorCamera, UsesKnownVulkanTransformAndFramebufferAspect) {
  const EditorCamera camera;
  const HomogeneousPoint center =
      transform(camera.frame(16.0F / 9.0F), {0.0F, 2.0F, 0.0F});
  EXPECT_NEAR(center.x / center.w, 0.0F, 0.0001F);
  EXPECT_NEAR(center.y / center.w, 0.0F, 0.0001F);
  EXPECT_GT(center.z / center.w, 0.0F);
  EXPECT_LT(center.z / center.w, 1.0F);

  const HomogeneousPoint square =
      transform(camera.frame(1.0F), {1.0F, 2.0F, 0.0F});
  const HomogeneousPoint wide =
      transform(camera.frame(2.0F), {1.0F, 2.0F, 0.0F});
  EXPECT_NEAR(wide.x / wide.w, (square.x / square.w) * 0.5F, 0.0001F);
  EXPECT_THROW(static_cast<void>(camera.frame(0.0F)), std::invalid_argument);
}

TEST(EditorCamera, CoversMovementAxesSprintMouseLookAndPitchLimit) {
  const EditorCameraPosition initial = EditorCamera{}.position();
  const auto moved = [](EditorNavigationInput input) {
    EditorCamera camera;
    camera.update(input, 0.1);
    return camera.position();
  };
  EditorNavigationInput input;
  input.move_forward = true;
  EXPECT_LT(moved(input).z, initial.z);
  input = {};
  input.move_backward = true;
  EXPECT_GT(moved(input).z, initial.z);
  input = {};
  input.move_left = true;
  EXPECT_LT(moved(input).x, initial.x);
  input = {};
  input.move_right = true;
  EXPECT_GT(moved(input).x, initial.x);
  input = {};
  input.move_up = true;
  EXPECT_GT(moved(input).y, initial.y);
  input = {};
  input.move_down = true;
  EXPECT_LT(moved(input).y, initial.y);

  input = {};
  input.move_forward = true;
  input.move_right = true;
  input.move_up = true;
  EditorCamera normal;
  normal.update(input, 0.1);
  EXPECT_NEAR(distance(initial, normal.position()), 0.4F, 0.0001F);
  input.sprint = true;
  EditorCamera sprint;
  sprint.update(input, 0.1);
  EXPECT_NEAR(distance(initial, sprint.position()), 1.2F, 0.0001F);

  EditorCamera look;
  input = {};
  input.look_delta_x = 100.0;
  input.look_delta_y = -1000.0;
  look.update(input, 0.0);
  EXPECT_FLOAT_EQ(look.yawDegrees(), -80.0F);
  EXPECT_FLOAT_EQ(look.pitchDegrees(), 89.0F);
  input.look_delta_y = 2000.0;
  look.update(input, 0.0);
  EXPECT_FLOAT_EQ(look.pitchDegrees(), -89.0F);
}

TEST(EditorInputOwnership, UiCaptureSuppressesConflictingCameraInput) {
  InputAccumulator accumulator;
  accumulator.setKey(PhysicalKey::W, true);
  accumulator.setKey(PhysicalKey::LeftShift, true);
  accumulator.addCursorPosition(10.0, 20.0);
  accumulator.addCursorPosition(13.0, 24.0);

  const EditorNavigationInput inactive =
      editorNavigationInput(accumulator.snapshot(), false, {});
  EXPECT_FALSE(inactive.move_forward);
  EXPECT_DOUBLE_EQ(inactive.look_delta_x, 0.0);

  const EditorNavigationInput captured = editorNavigationInput(
      accumulator.snapshot(), true, {.keyboard = true, .pointer = true});
  EXPECT_FALSE(captured.move_forward);
  EXPECT_FALSE(captured.sprint);
  EXPECT_DOUBLE_EQ(captured.look_delta_x, 0.0);

  const EditorNavigationInput navigation =
      editorNavigationInput(accumulator.snapshot(), true, {});
  EXPECT_TRUE(navigation.move_forward);
  EXPECT_TRUE(navigation.sprint);
  EXPECT_DOUBLE_EQ(navigation.look_delta_x, 3.0);
  EXPECT_DOUBLE_EQ(navigation.look_delta_y, 4.0);
}

TEST(EditorCamera, BoundsTimingAndResetsAcrossWait) {
  using namespace std::chrono_literals;
  EditorFrameClock clock;
  const EditorFrameClock::Clock::time_point start{};
  EXPECT_DOUBLE_EQ(clock.sample(start), 0.0);
  EXPECT_NEAR(clock.sample(start + 16ms), 0.016, 0.000001);
  EXPECT_DOUBLE_EQ(clock.sample(start + 1s), 0.1);
  clock.reset();
  EXPECT_DOUBLE_EQ(clock.sample(start + 10s), 0.0);
}
