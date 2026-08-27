#include <gtest/gtest.h>

#include "core/platform/input.hpp"

TEST(InputAccumulator, TracksEngineOwnedKeysAndButtons) {
  InputAccumulator input;
  input.setKey(Key::MoveForward, true);
  input.setMouseButton(MouseButton::Left, true);
  EXPECT_TRUE(input.snapshot().isKeyDown(Key::MoveForward));
  EXPECT_TRUE(input.snapshot().isMouseButtonDown(MouseButton::Left));
  input.setKey(Key::MoveForward, false);
  input.setMouseButton(MouseButton::Left, false);
  EXPECT_FALSE(input.snapshot().isKeyDown(Key::MoveForward));
  EXPECT_FALSE(input.snapshot().isMouseButtonDown(MouseButton::Left));
}

TEST(InputAccumulator, ResetsCursorDeltaPerEventBatch) {
  InputAccumulator input;
  input.addCursorPosition(10.0, 20.0);
  input.addCursorPosition(13.5, 18.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_x, 3.5);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_y, -2.0);

  input.beginEventBatch();
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_x, 0.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_y, 0.0);
  input.addCursorPosition(14.0, 19.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_x, 0.5);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_y, 1.0);
}

TEST(InputAccumulator, CaptureResetDiscardsCursorJump) {
  InputAccumulator input;
  input.addCursorPosition(1.0, 2.0);
  input.resetCursorTracking();
  input.addCursorPosition(100.0, 200.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_x, 0.0);
  EXPECT_DOUBLE_EQ(input.snapshot().cursor_delta_y, 0.0);
}
